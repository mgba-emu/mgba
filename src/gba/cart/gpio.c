/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/cart/gpio.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba-util/formatting.h>
#include <mgba-util/hash.h>
#include <mgba-util/memory.h>

mLOG_DEFINE_CATEGORY(GBA_HW, "GBA Pak Hardware", "gba.hardware");

MGBA_EXPORT const int GBA_LUX_LEVELS[10] = { 5, 11, 18, 27, 42, 62, 84, 109, 139, 183 };

static void _readPins(struct GBACartridgeHardware* hw);
static void _outputPins(struct GBACartridgeHardware* hw, unsigned pins);

static void _rtcReadPins(struct GBACartridgeHardware* hw);
static unsigned _rtcOutput(struct GBACartridgeHardware* hw);
static void _rtcProcessByte(struct GBACartridgeHardware* hw);
static void _rtcUpdateClock(struct GBACartridgeHardware* hw);
static unsigned _rtcBCD(unsigned value);

static void _gyroReadPins(struct GBACartridgeHardware* hw);

static void _rumbleReadPins(struct GBACartridgeHardware* hw);

static void _lightReadPins(struct GBACartridgeHardware* hw);

static const int RTC_BYTES[8] = {
	0, // Force reset
	0, // Empty
	7, // Date/Time
	0, // Force IRQ
	1, // Control register
	0, // Empty
	3, // Time
	0 // Empty
};

void GBAHardwareInit(struct GBACartridgeHardware* hw, uint16_t* base) {
	hw->gpioBase = base;
	GBAHardwareClear(hw);
}

void GBAHardwareClear(struct GBACartridgeHardware* hw) {
	hw->devices = HW_NONE | (hw->devices & HW_GB_PLAYER_DETECTION);
	hw->readWrite = GPIO_WRITE_ONLY;
	hw->pinState = 0;
	hw->direction = 0;
}

void GBAHardwareGPIOWrite(struct GBACartridgeHardware* hw, uint32_t address, uint16_t value) {
	if (!hw->gpioBase) {
		return;
	}
	switch (address) {
	case GPIO_REG_DATA:
		if (!hw->p->vbaBugCompat) {
			hw->pinState &= ~hw->direction;
			hw->pinState |= value & hw->direction;
		} else {
			hw->pinState = value;
		}
		_readPins(hw);
		break;
	case GPIO_REG_DIRECTION:
		hw->direction = value;
		break;
	case GPIO_REG_CONTROL:
		hw->readWrite = value;
		break;
	default:
		mLOG(GBA_HW, WARN, "Invalid GPIO address");
	}
	if (hw->readWrite) {
		STORE_16(hw->pinState, 0, hw->gpioBase);
		STORE_16(hw->direction, 2, hw->gpioBase);
		STORE_16(hw->readWrite, 4, hw->gpioBase);
	} else {
		hw->gpioBase[0] = 0;
		hw->gpioBase[1] = 0;
		hw->gpioBase[2] = 0;
	}
}

void GBAHardwareInitRTC(struct GBACartridgeHardware* hw) {
	hw->devices |= HW_RTC;
	hw->rtc.bytesRemaining = 0;

	hw->rtc.transferStep = 0;

	hw->rtc.bitsRead = 0;
	hw->rtc.bits = 0;
	hw->rtc.commandActive = 0;
	hw->rtc.command = 0;
	hw->rtc.control = 0x40;
	memset(hw->rtc.time, 0, sizeof(hw->rtc.time));

	hw->rtc.lastLatch = 0;
	hw->rtc.offset = 0;
}

void _readPins(struct GBACartridgeHardware* hw) {
	if (hw->devices & HW_RTC) {
		_rtcReadPins(hw);
	}

	if (hw->devices & HW_GYRO) {
		_gyroReadPins(hw);
	}

	if (hw->devices & HW_RUMBLE) {
		_rumbleReadPins(hw);
	}

	if (hw->devices & HW_LIGHT_SENSOR) {
		_lightReadPins(hw);
	}
}

void _outputPins(struct GBACartridgeHardware* hw, unsigned pins) {
	if (hw->readWrite) {
		uint16_t old;
		LOAD_16(old, 0, hw->gpioBase);
		old &= hw->direction;
		hw->pinState = old | (pins & ~hw->direction & 0xF);
		STORE_16(hw->pinState, 0, hw->gpioBase);
	}
}

// == RTC

void _rtcReadPins(struct GBACartridgeHardware* hw) {
	// Transfer sequence:
	// P: 0 | 1 |  2 | 3
	// == Initiate
	// > HI | - | LO | -
	// > HI | - | HI | -
	// == Transfer bit (x8)
	// > LO | x | HI | -
	// > HI | - | HI | -
	// < ?? | x | ?? | -
	// == Terminate
	// >  - | - | LO | -
	switch (hw->rtc.transferStep) {
	case 0:
		if ((hw->pinState & 5) == 1) {
			hw->rtc.transferStep = 1;
		}
		break;
	case 1:
		if ((hw->pinState & 5) == 5) {
			hw->rtc.transferStep = 2;
		} else if ((hw->pinState & 5) != 1) {
			hw->rtc.transferStep = 0;
		}
		break;
	case 2:
		if (!(hw->pinState & 1)) {
			hw->rtc.bits &= ~(1 << hw->rtc.bitsRead);
			hw->rtc.bits |= ((hw->pinState & 2) >> 1) << hw->rtc.bitsRead;
		} else {
			if (hw->pinState & 4) {
				if (!RTCCommandDataIsReading(hw->rtc.command)) {
					++hw->rtc.bitsRead;
					if (hw->rtc.bitsRead == 8) {
						_rtcProcessByte(hw);
					}
				} else {
					_outputPins(hw, 5 | (_rtcOutput(hw) << 1));
					++hw->rtc.bitsRead;
					if (hw->rtc.bitsRead == 8) {
						--hw->rtc.bytesRemaining;
						if (hw->rtc.bytesRemaining <= 0) {
							hw->rtc.commandActive = 0;
							hw->rtc.command = 0;
						}
						hw->rtc.bitsRead = 0;
					}
				}
			} else {
				hw->rtc.bitsRead = 0;
				hw->rtc.bytesRemaining = 0;
				hw->rtc.commandActive = 0;
				hw->rtc.command = 0;
				hw->rtc.transferStep = hw->pinState & 1;
				_outputPins(hw, 1);
			}
		}
		break;
	}
}

void _rtcProcessByte(struct GBACartridgeHardware* hw) {
	--hw->rtc.bytesRemaining;
	if (!hw->rtc.commandActive) {
		RTCCommandData command;
		command = hw->rtc.bits;
		if (RTCCommandDataGetMagic(command) == 0x06) {
			hw->rtc.command = command;

			hw->rtc.bytesRemaining = RTC_BYTES[RTCCommandDataGetCommand(command)];
			hw->rtc.commandActive = hw->rtc.bytesRemaining > 0;
			mLOG(GBA_HW, DEBUG, "Got RTC command %x", RTCCommandDataGetCommand(command));
			switch (RTCCommandDataGetCommand(command)) {
			case RTC_RESET:
				hw->rtc.control = 0;
				break;
			case RTC_DATETIME:
			case RTC_TIME:
				_rtcUpdateClock(hw);
				break;
			case RTC_FORCE_IRQ:
			case RTC_CONTROL:
				break;
			}
		} else {
			mLOG(GBA_HW, WARN, "Invalid RTC command byte: %02X", hw->rtc.bits);
		}
	} else {
		switch (RTCCommandDataGetCommand(hw->rtc.command)) {
		case RTC_CONTROL:
			hw->rtc.control = hw->rtc.bits;
			break;
		case RTC_FORCE_IRQ:
			mLOG(GBA_HW, STUB, "Unimplemented RTC command %u", RTCCommandDataGetCommand(hw->rtc.command));
			break;
		case RTC_RESET:
		case RTC_DATETIME:
		case RTC_TIME:
			break;
		}
	}

	hw->rtc.bits = 0;
	hw->rtc.bitsRead = 0;
	if (!hw->rtc.bytesRemaining) {
		hw->rtc.commandActive = 0;
		hw->rtc.command = 0;
	}
}

unsigned _rtcOutput(struct GBACartridgeHardware* hw) {
	uint8_t outputByte = 0;
	if (!hw->rtc.commandActive) {
		mLOG(GBA_HW, GAME_ERROR, "Attempting to use RTC without an active command");
		return 0;
	}
	switch (RTCCommandDataGetCommand(hw->rtc.command)) {
	case RTC_CONTROL:
		outputByte = hw->rtc.control;
		break;
	case RTC_DATETIME:
	case RTC_TIME:
		outputByte = hw->rtc.time[7 - hw->rtc.bytesRemaining];
		break;
	case RTC_FORCE_IRQ:
	case RTC_RESET:
		break;
	}
	unsigned output = (outputByte >> hw->rtc.bitsRead) & 1;
	if (hw->rtc.bitsRead == 0) {
		mLOG(GBA_HW, DEBUG, "RTC output byte %02X", outputByte);
	}
	return output;
}

void _rtcUpdateClock(struct GBACartridgeHardware* hw) {
	time_t t;
	struct mRTCSource* rtc = hw->p->rtcSource;
	if (rtc) {
		if (rtc->sample) {
			rtc->sample(rtc);
		}
		t = rtc->unixTime(rtc);
	} else {
		t = time(0);
	}
	hw->rtc.lastLatch = t;
	t -= hw->rtc.offset;

	struct tm date;
	localtime_r(&t, &date);
	hw->rtc.time[0] = _rtcBCD(date.tm_year - 100);
	hw->rtc.time[1] = _rtcBCD(date.tm_mon + 1);
	hw->rtc.time[2] = _rtcBCD(date.tm_mday);
	hw->rtc.time[3] = _rtcBCD(date.tm_wday);
	if (RTCControlIsHour24(hw->rtc.control)) {
		hw->rtc.time[4] = _rtcBCD(date.tm_hour);
	} else {
		hw->rtc.time[4] = _rtcBCD(date.tm_hour % 12);
	}
	hw->rtc.time[5] = _rtcBCD(date.tm_min);
	hw->rtc.time[6] = _rtcBCD(date.tm_sec);
}

unsigned _rtcBCD(unsigned value) {
	int counter = value % 10;
	value /= 10;
	counter += (value % 10) << 4;
	return counter;
}

// == Gyro

void GBAHardwareInitGyro(struct GBACartridgeHardware* hw) {
	hw->devices |= HW_GYRO;
	hw->gyroSample = 0;
	hw->gyroEdge = 0;
}

void _gyroReadPins(struct GBACartridgeHardware* hw) {
	struct mRotationSource* gyro = hw->p->rotationSource;
	if (!gyro || !gyro->readGyroZ) {
		return;
	}

	if (hw->pinState & 1) {
		if (gyro->sample) {
			gyro->sample(gyro);
		}
		int32_t sample = gyro->readGyroZ(gyro);

		// Normalize to ~12 bits, focused on 0x6C0
		hw->gyroSample = (sample >> 21) + 0x6C0; // Crop off an extra bit so that we can't go negative
	}

	if (hw->gyroEdge && !(hw->pinState & 2)) {
		// Write bit on falling edge
		unsigned bit = hw->gyroSample >> 15;
		hw->gyroSample <<= 1;
		_outputPins(hw, bit << 2);
	}

	hw->gyroEdge = !!(hw->pinState & 2);
}

// == Rumble

void GBAHardwareInitRumble(struct GBACartridgeHardware* hw) {
	hw->devices |= HW_RUMBLE;
}

void _rumbleReadPins(struct GBACartridgeHardware* hw) {
	struct mRumble* rumble = hw->p->rumble;
	if (!rumble) {
		return;
	}

	rumble->setRumble(rumble, !!(hw->pinState & 8));
}

// == Light sensor

void GBAHardwareInitLight(struct GBACartridgeHardware* hw) {
	hw->devices |= HW_LIGHT_SENSOR;
	hw->lightCounter = 0;
	hw->lightEdge = false;
	hw->lightSample = 0xFF;
}

void _lightReadPins(struct GBACartridgeHardware* hw) {
	if (hw->pinState & 4) {
		// Boktai chip select
		return;
	}
	if (hw->pinState & 2) {
		struct GBALuminanceSource* lux = hw->p->luminanceSource;
		mLOG(GBA_HW, DEBUG, "[SOLAR] Got reset");
		hw->lightCounter = 0;
		if (lux) {
			lux->sample(lux);
			hw->lightSample = lux->readLuminance(lux);
		} else {
			hw->lightSample = 0xFF;
		}
	}
	if ((hw->pinState & 1) && hw->lightEdge) {
		++hw->lightCounter;
	}
	hw->lightEdge = !(hw->pinState & 1);

	bool sendBit = hw->lightCounter >= hw->lightSample;
	_outputPins(hw, sendBit << 3);
	mLOG(GBA_HW, DEBUG, "[SOLAR] Output %u with pins %u", hw->lightCounter, hw->pinState);
}

// == Tilt

void GBAHardwareInitTilt(struct GBACartridgeHardware* hw) {
	hw->devices |= HW_TILT;
	hw->tiltX = 0xFFF;
	hw->tiltY = 0xFFF;
	hw->tiltState = 0;
}

void GBAHardwareTiltWrite(struct GBACartridgeHardware* hw, uint32_t address, uint8_t value) {
	switch (address) {
	case 0x8000:
		if (value == 0x55) {
			hw->tiltState = 1;
		} else {
			mLOG(GBA_HW, GAME_ERROR, "Tilt sensor wrote wrong byte to %04x: %02x", address, value);
		}
		break;
	case 0x8100:
		if (value == 0xAA && hw->tiltState == 1) {
			hw->tiltState = 0;
			struct mRotationSource* rotationSource = hw->p->rotationSource;
			if (!rotationSource || !rotationSource->readTiltX || !rotationSource->readTiltY) {
				return;
			}
			if (rotationSource->sample) {
				rotationSource->sample(rotationSource);
			}
			int32_t x = rotationSource->readTiltX(rotationSource);
			int32_t y = rotationSource->readTiltY(rotationSource);
			// Normalize to ~12 bits, focused on 0x3A0
			hw->tiltX = (x >> 21) + 0x3A0; // Crop off an extra bit so that we can't go negative
			hw->tiltY = (y >> 21) + 0x3A0;
		} else {
			mLOG(GBA_HW, GAME_ERROR, "Tilt sensor wrote wrong byte to %04x: %02x", address, value);
		}
		break;
	default:
		mLOG(GBA_HW, GAME_ERROR, "Invalid tilt sensor write to %04x: %02x", address, value);
		break;
	}
}

uint8_t GBAHardwareTiltRead(struct GBACartridgeHardware* hw, uint32_t address) {
	switch (address) {
	case 0x8200:
		return hw->tiltX & 0xFF;
	case 0x8300:
		return ((hw->tiltX >> 8) & 0xF) | 0x80;
	case 0x8400:
		return hw->tiltY & 0xFF;
	case 0x8500:
		return (hw->tiltY >> 8) & 0xF;
	default:
		mLOG(GBA_HW, GAME_ERROR, "Invalid tilt sensor read from %04x", address);
		break;
	}
	return 0xFF;
}

// == Serialization

void GBAHardwareSerialize(const struct GBACartridgeHardware* hw, struct GBASerializedState* state) {
	GBASerializedHWFlags1 flags1 = 0;
	flags1 = GBASerializedHWFlags1SetReadWrite(flags1, hw->readWrite);
	STORE_16(hw->pinState, 0, &state->hw.pinState);
	STORE_16(hw->direction, 0, &state->hw.pinDirection);
	state->hw.devices = hw->devices;

	STORE_32(hw->rtc.bytesRemaining, 0, &state->hw.rtcBytesRemaining);
	STORE_32(hw->rtc.transferStep, 0, &state->hw.rtcTransferStep);
	STORE_32(hw->rtc.bitsRead, 0, &state->hw.rtcBitsRead);
	STORE_32(hw->rtc.bits, 0, &state->hw.rtcBits);
	STORE_32(hw->rtc.commandActive, 0, &state->hw.rtcCommandActive);
	STORE_32(hw->rtc.command, 0, &state->hw.rtcCommand);
	STORE_32(hw->rtc.control, 0, &state->hw.rtcControl);
	memcpy(state->hw.time, hw->rtc.time, sizeof(state->hw.time));

	STORE_16(hw->gyroSample, 0, &state->hw.gyroSample);
	flags1 = GBASerializedHWFlags1SetGyroEdge(flags1, hw->gyroEdge);
	STORE_16(hw->tiltX, 0, &state->hw.tiltSampleX);
	STORE_16(hw->tiltY, 0, &state->hw.tiltSampleY);
	state->hw.lightSample = hw->lightSample;
	flags1 = GBASerializedHWFlags1SetLightEdge(flags1, hw->lightEdge);
	STORE_16(flags1, 0, &state->hw.flags1);

	GBASerializedHWFlags2 flags2 = 0;
	flags2 = GBASerializedHWFlags2SetTiltState(flags2, hw->tiltState);
	flags2 = GBASerializedHWFlags1SetLightCounter(flags2, hw->lightCounter);

	// GBP stuff is only here for legacy reasons
	flags2 = GBASerializedHWFlags2SetGbpInputsPosted(flags2, hw->p->sio.gbp.inputsPosted);
	flags2 = GBASerializedHWFlags2SetGbpTxPosition(flags2, hw->p->sio.gbp.txPosition);
	STORE_32(hw->p->sio.gbp.event.when - mTimingCurrentTime(&hw->p->timing), 0, &state->hw.gbpNextEvent);

	state->hw.flags2 = flags2;
}

void GBAHardwareDeserialize(struct GBACartridgeHardware* hw, const struct GBASerializedState* state) {
	GBASerializedHWFlags1 flags1;
	LOAD_16(flags1, 0, &state->hw.flags1);
	hw->readWrite = GBASerializedHWFlags1GetReadWrite(flags1);
	LOAD_16(hw->pinState, 0, &state->hw.pinState);
	LOAD_16(hw->direction, 0, &state->hw.pinDirection);
	hw->devices = state->hw.devices;

	LOAD_32(hw->rtc.bytesRemaining, 0, &state->hw.rtcBytesRemaining);
	LOAD_32(hw->rtc.transferStep, 0, &state->hw.rtcTransferStep);
	LOAD_32(hw->rtc.bitsRead, 0, &state->hw.rtcBitsRead);
	LOAD_32(hw->rtc.bits, 0, &state->hw.rtcBits);
	LOAD_32(hw->rtc.commandActive, 0, &state->hw.rtcCommandActive);
	LOAD_32(hw->rtc.command, 0, &state->hw.rtcCommand);
	LOAD_32(hw->rtc.control, 0, &state->hw.rtcControl);
	memcpy(hw->rtc.time, state->hw.time, sizeof(hw->rtc.time));

	LOAD_16(hw->gyroSample, 0, &state->hw.gyroSample);
	hw->gyroEdge = GBASerializedHWFlags1GetGyroEdge(flags1);
	LOAD_16(hw->tiltX, 0, &state->hw.tiltSampleX);
	LOAD_16(hw->tiltY, 0, &state->hw.tiltSampleY);
	hw->tiltState = GBASerializedHWFlags2GetTiltState(state->hw.flags2);
	hw->lightCounter = GBASerializedHWFlags1GetLightCounter(flags1);
	hw->lightSample = state->hw.lightSample;
	hw->lightEdge = GBASerializedHWFlags1GetLightEdge(flags1);

	// GBP stuff is only here for legacy reasons
	hw->p->sio.gbp.inputsPosted = GBASerializedHWFlags2GetGbpInputsPosted(state->hw.flags2);
	hw->p->sio.gbp.txPosition = GBASerializedHWFlags2GetGbpTxPosition(state->hw.flags2);

	uint32_t when;
	LOAD_32(when, 0, &state->hw.gbpNextEvent);
	if (hw->devices & HW_GB_PLAYER) {
		GBASIOSetDriver(&hw->p->sio, &hw->p->sio.gbp.d, SIO_NORMAL_32);
		if (hw->p->memory.io[REG_SIOCNT >> 1] & 0x0080) {
			mTimingSchedule(&hw->p->timing, &hw->p->sio.gbp.event, when);
		}
	}
}
