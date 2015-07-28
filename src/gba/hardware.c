/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "hardware.h"

#include "gba/io.h"
#include "gba/serialize.h"
#include "util/hash.h"

static void _readPins(struct GBACartridgeHardware* hw);
static void _outputPins(struct GBACartridgeHardware* hw, unsigned pins);

static void _rtcReadPins(struct GBACartridgeHardware* hw);
static unsigned _rtcOutput(struct GBACartridgeHardware* hw);
static void _rtcProcessByte(struct GBACartridgeHardware* hw);
static void _rtcUpdateClock(struct GBACartridgeHardware* hw);
static unsigned _rtcBCD(unsigned value);

static time_t _rtcGenericCallback(struct GBARTCSource* source);

static void _gyroReadPins(struct GBACartridgeHardware* hw);

static void _rumbleReadPins(struct GBACartridgeHardware* hw);

static void _lightReadPins(struct GBACartridgeHardware* hw);

static uint16_t _gbpRead(struct GBAKeyCallback*);
static uint16_t _gbpSioWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
static int32_t _gbpSioProcessEvents(struct GBASIODriver* driver, int32_t cycles);

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

	hw->gbpCallback.d.readKeys = _gbpRead;
	hw->gbpCallback.p = hw;
	hw->gbpDriver.d.init = 0;
	hw->gbpDriver.d.deinit = 0;
	hw->gbpDriver.d.load = 0;
	hw->gbpDriver.d.unload = 0;
	hw->gbpDriver.d.writeRegister = _gbpSioWriteRegister;
	hw->gbpDriver.d.processEvents = _gbpSioProcessEvents;
	hw->gbpDriver.p = hw;
}

void GBAHardwareClear(struct GBACartridgeHardware* hw) {
	hw->devices = HW_NONE;
	hw->direction = GPIO_WRITE_ONLY;
	hw->pinState = 0;
	hw->direction = 0;
}

void GBAHardwareGPIOWrite(struct GBACartridgeHardware* hw, uint32_t address, uint16_t value) {
	switch (address) {
	case GPIO_REG_DATA:
		hw->pinState &= ~hw->direction;
		hw->pinState |= value;
		_readPins(hw);
		break;
	case GPIO_REG_DIRECTION:
		hw->direction = value;
		break;
	case GPIO_REG_CONTROL:
		hw->readWrite = value;
		break;
	default:
		GBALog(hw->p, GBA_LOG_WARN, "Invalid GPIO address");
	}
	if (hw->readWrite) {
		uint16_t old = hw->gpioBase[0];
		old &= ~hw->direction;
		hw->gpioBase[0] = old | hw->pinState;
	} else {
		hw->gpioBase[0] = 0;
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
		uint16_t old = hw->gpioBase[0];
		old &= hw->direction;
		hw->pinState = old | (pins & ~hw->direction & 0xF);
		hw->gpioBase[0] = hw->pinState;
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
		}
		break;
	case 2:
		if (!(hw->pinState & 1)) {
			hw->rtc.bits &= ~(1 << hw->rtc.bitsRead);
			hw->rtc.bits |= ((hw->pinState & 2) >> 1) << hw->rtc.bitsRead;
		} else {
			if (hw->pinState & 4) {
				// GPIO direction should always != reading
				if (hw->direction & 2) {
					if (RTCCommandDataIsReading(hw->rtc.command)) {
						GBALog(hw->p, GBA_LOG_GAME_ERROR, "Attempting to write to RTC while in read mode");
					}
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
							hw->rtc.command = RTCCommandDataClearReading(hw->rtc.command);
						}
						hw->rtc.bitsRead = 0;
					}
				}
			} else {
				hw->rtc.bitsRead = 0;
				hw->rtc.bytesRemaining = 0;
				hw->rtc.commandActive = 0;
				hw->rtc.command = RTCCommandDataClearReading(hw->rtc.command);
				hw->rtc.transferStep = 0;
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
			GBALog(hw->p, GBA_LOG_WARN, "Invalid RTC command byte: %02X", hw->rtc.bits);
		}
	} else {
		switch (RTCCommandDataGetCommand(hw->rtc.command)) {
		case RTC_CONTROL:
			hw->rtc.control = hw->rtc.bits;
			break;
		case RTC_FORCE_IRQ:
			GBALog(hw->p, GBA_LOG_STUB, "Unimplemented RTC command %u", RTCCommandDataGetCommand(hw->rtc.command));
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
		hw->rtc.command = RTCCommandDataClearReading(hw->rtc.command);
	}
}

unsigned _rtcOutput(struct GBACartridgeHardware* hw) {
	uint8_t outputByte = 0;
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
	return output;
}

void _rtcUpdateClock(struct GBACartridgeHardware* hw) {
	time_t t;
	struct GBARTCSource* rtc = hw->p->rtcSource;
	if (rtc) {
		if (rtc->sample) {
			rtc->sample(rtc);
		}
		t = rtc->unixTime(rtc);
	} else {
		t = time(0);
	}
	struct tm date;
#if defined(_WIN32)
	date = *localtime(&t);
#elif defined(__CELLOS_LV2__)
   memcpy(&date, localtime(&t), sizeof(date));
#else
	localtime_r(&t, &date);
#endif
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

time_t _rtcGenericCallback(struct GBARTCSource* source) {
	struct GBARTCGenericSource* rtc = (struct GBARTCGenericSource*) source;
	switch (rtc->override) {
	case RTC_NO_OVERRIDE:
	default:
		return time(0);
	case RTC_FIXED:
		return rtc->value;
	case RTC_FAKE_EPOCH:
		return rtc->value + rtc->p->video.frameCounter * (int64_t) VIDEO_TOTAL_LENGTH / GBA_ARM7TDMI_FREQUENCY;
	}
}

void GBARTCGenericSourceInit(struct GBARTCGenericSource* rtc, struct GBA* gba) {
	rtc->p = gba;
	rtc->override = RTC_NO_OVERRIDE;
	rtc->value = 0;
	rtc->d.sample = 0;
	rtc->d.unixTime = _rtcGenericCallback;
}

// == Gyro

void GBAHardwareInitGyro(struct GBACartridgeHardware* hw) {
	hw->devices |= HW_GYRO;
	hw->gyroSample = 0;
	hw->gyroEdge = 0;
}

void _gyroReadPins(struct GBACartridgeHardware* hw) {
	struct GBARotationSource* gyro = hw->p->rotationSource;
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
	struct GBARumble* rumble = hw->p->rumble;
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
		GBALog(hw->p, GBA_LOG_DEBUG, "[SOLAR] Got reset");
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
	GBALog(hw->p, GBA_LOG_DEBUG, "[SOLAR] Output %u with pins %u", hw->lightCounter, hw->pinState);
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
			GBALog(hw->p, GBA_LOG_GAME_ERROR, "Tilt sensor wrote wrong byte to %04x: %02x", address, value);
		}
		break;
	case 0x8100:
		if (value == 0xAA && hw->tiltState == 1) {
			hw->tiltState = 0;
			struct GBARotationSource* rotationSource = hw->p->rotationSource;
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
			GBALog(hw->p, GBA_LOG_GAME_ERROR, "Tilt sensor wrote wrong byte to %04x: %02x", address, value);
		}
		break;
	default:
		GBALog(hw->p, GBA_LOG_GAME_ERROR, "Invalid tilt sensor write to %04x: %02x", address, value);
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
		GBALog(hw->p, GBA_LOG_GAME_ERROR, "Invalid tilt sensor read from %04x", address);
		break;
	}
	return 0xFF;
}

// == Game Boy Player

static const uint16_t _logoPalette[] = {
	0xFFDF, 0x640C, 0xE40C, 0xE42D, 0x644E, 0xE44E, 0xE46E, 0x68AF,
	0xE8B0, 0x68D0, 0x68F0, 0x6911, 0xE911, 0x6D32, 0xED32, 0xED73,
	0x6D93, 0xED94, 0x6DB4, 0xF1D5, 0x71F5, 0xF1F6, 0x7216, 0x7257,
	0xF657, 0x7678, 0xF678, 0xF699, 0xF6B9, 0x76D9, 0xF6DA, 0x7B1B,
	0xFB1B, 0xFB3C, 0x7B5C, 0x7B7D, 0xFF7D, 0x7F9D, 0x7FBE, 0x7FFF,
	0x642D, 0x648E, 0xE88F, 0xE8F1, 0x6D52, 0x6D73, 0xF1B4, 0xF216,
	0x7237, 0x7698, 0x7AFA, 0xFAFA, 0xFB5C, 0xFFBE, 0x7FDE, 0xFFFF,
	0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static const uint32_t _logoHash = 0xEEDA6963;

static const uint32_t _gbpTxData[] = {
	0x0000494E, 0x0000494E,
	0xB6B1494E, 0xB6B1544E,
	0xABB1544E, 0xABB14E45,
	0xB1BA4E45, 0xB1BA4F44,
	0xB0BB4F44, 0xB0BB8002,
	0x10000010, 0x20000013,
	0x30000003, 0x30000003,
	0x30000003, 0x30000003,
	0x30000003, 0x00000000,
};

static const uint32_t _gbpRxData[] = {
	0x00000000, 0x494EB6B1,
	0x494EB6B1, 0x544EB6B1,
	0x544EABB1, 0x4E45ABB1,
	0x4E45B1BA, 0x4F44B1BA,
	0x4F44B0BB, 0x8000B0BB,
	0x10000010, 0x20000013,
	0x40000004, 0x40000004,
	0x40000004, 0x40000004,
	0x40000004, 0x40000004
};

bool GBAHardwarePlayerCheckScreen(const struct GBAVideo* video) {
	if (memcmp(video->palette, _logoPalette, sizeof(_logoPalette)) != 0) {
		return false;
	}
	uint32_t hash = hash32(&video->renderer->vram[0x4000], 0x4000, 0);
	return hash == _logoHash;
}

void GBAHardwarePlayerUpdate(struct GBA* gba) {
	if (gba->memory.hw.devices & HW_GB_PLAYER) {
		if (GBAHardwarePlayerCheckScreen(&gba->video)) {
			++gba->memory.hw.gbpInputsPosted;
			gba->memory.hw.gbpInputsPosted %= 3;
			gba->keyCallback = &gba->memory.hw.gbpCallback.d;
		} else {
			// TODO: Save and restore
			gba->keyCallback = 0;
		}
		gba->memory.hw.gbpTxPosition = 0;
		return;
	}
	if (gba->keyCallback || gba->sio.drivers.normal) {
		return;
	}
	if (GBAHardwarePlayerCheckScreen(&gba->video)) {
		gba->memory.hw.devices |= HW_GB_PLAYER;
		gba->memory.hw.gbpInputsPosted = 0;
		gba->memory.hw.gbpNextEvent = INT_MAX;
		gba->keyCallback = &gba->memory.hw.gbpCallback.d;
		GBASIOSetDriver(&gba->sio, &gba->memory.hw.gbpDriver.d, SIO_NORMAL_32);
	}
}

uint16_t _gbpRead(struct GBAKeyCallback* callback) {
	struct GBAGBPKeyCallback* gbpCallback = (struct GBAGBPKeyCallback*) callback;
	if (gbpCallback->p->gbpInputsPosted == 2) {
		return 0x30F;
	}
	return 0x3FF;
}

uint16_t _gbpSioWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBAGBPSIODriver* gbp = (struct GBAGBPSIODriver*) driver;
	if (address == REG_SIOCNT) {
		if (value & 0x0080) {
			if (gbp->p->gbpTxPosition <= 16 && gbp->p->gbpTxPosition > 0) {
				uint32_t rx = gbp->p->p->memory.io[REG_SIODATA32_LO >> 1] | (gbp->p->p->memory.io[REG_SIODATA32_HI >> 1] << 16);
				uint32_t expected = _gbpRxData[gbp->p->gbpTxPosition];
				// TODO: Check expected
				uint32_t mask = 0;
				if (gbp->p->gbpTxPosition == 15) {
					mask = 0x22;
					if (gbp->p->p->rumble) {
						gbp->p->p->rumble->setRumble(gbp->p->p->rumble, (rx & mask) == mask);
					}
				}
			}
			gbp->p->gbpNextEvent = 2048;
		}
		value &= 0x78FB;
	}
	return value;
}

int32_t _gbpSioProcessEvents(struct GBASIODriver* driver, int32_t cycles) {
	struct GBAGBPSIODriver* gbp = (struct GBAGBPSIODriver*) driver;
	gbp->p->gbpNextEvent -= cycles;
	if (gbp->p->gbpNextEvent <= 0) {
		uint32_t tx = 0;
		if (gbp->p->gbpTxPosition <= 16) {
			tx = _gbpTxData[gbp->p->gbpTxPosition];
			++gbp->p->gbpTxPosition;
		}
		gbp->p->p->memory.io[REG_SIODATA32_LO >> 1] = tx;
		gbp->p->p->memory.io[REG_SIODATA32_HI >> 1] = tx >> 16;
		if (gbp->d.p->a.normalControl.irq) {
			GBARaiseIRQ(gbp->p->p, IRQ_SIO);
		}
		gbp->d.p->a.normalControl.start = 0;
		gbp->p->p->memory.io[REG_SIOCNT >> 1] = gbp->d.p->a.siocnt;
		gbp->p->gbpNextEvent = INT_MAX;
	}
	return gbp->p->gbpNextEvent;
}

// == Serialization

void GBAHardwareSerialize(const struct GBACartridgeHardware* hw, struct GBASerializedState* state) {
	state->hw.readWrite = hw->readWrite;
	state->hw.pinState = hw->pinState;
	state->hw.pinDirection = hw->direction;
	state->hw.devices = hw->devices;
	state->hw.rtc = hw->rtc;
	state->hw.gyroSample = hw->gyroSample;
	state->hw.gyroEdge = hw->gyroEdge;
	state->hw.tiltSampleX = hw->tiltX;
	state->hw.tiltSampleY = hw->tiltY;
	state->hw.tiltState = hw->tiltState;
	state->hw.lightCounter = hw->lightCounter;
	state->hw.lightSample = hw->lightSample;
	state->hw.lightEdge = hw->lightEdge;
	state->hw.gbpInputsPosted = hw->gbpInputsPosted;
	state->hw.gbpTxPosition = hw->gbpTxPosition;
	state->hw.gbpNextEvent = hw->gbpNextEvent;
}

void GBAHardwareDeserialize(struct GBACartridgeHardware* hw, const struct GBASerializedState* state) {
	hw->readWrite = state->hw.readWrite;
	hw->pinState = state->hw.pinState;
	hw->direction = state->hw.pinDirection;
	hw->rtc = state->hw.rtc;
	hw->gyroSample = state->hw.gyroSample;
	hw->gyroEdge = state->hw.gyroEdge;
	hw->tiltX = state->hw.tiltSampleX;
	hw->tiltY = state->hw.tiltSampleY;
	hw->tiltState = state->hw.tiltState;
	hw->lightCounter = state->hw.lightCounter;
	hw->lightSample = state->hw.lightSample;
	hw->lightEdge = state->hw.lightEdge;
	hw->gbpInputsPosted = state->hw.gbpInputsPosted;
	hw->gbpTxPosition = state->hw.gbpTxPosition;
	hw->gbpNextEvent = state->hw.gbpNextEvent;
}
