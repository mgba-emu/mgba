/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba.h"

#include "gba-hardware.h"
#include "gba-serialize.h"

#include <time.h>

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
	hw->rtc.command.packed = 0;
	hw->rtc.control.packed = 0x40;
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
		if (!hw->p0) {
			hw->rtc.bits &= ~(1 << hw->rtc.bitsRead);
			hw->rtc.bits |= hw->p1 << hw->rtc.bitsRead;
		} else {
			if (hw->p2) {
				// GPIO direction should always != reading
				if (hw->dir1) {
					if (hw->rtc.command.reading) {
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
							hw->rtc.command.reading = 0;
						}
						hw->rtc.bitsRead = 0;
					}
				}
			} else {
				hw->rtc.bitsRead = 0;
				hw->rtc.bytesRemaining = 0;
				hw->rtc.commandActive = 0;
				hw->rtc.command.reading = 0;
				hw->rtc.transferStep = 0;
			}
		}
		break;
	}
}

void _rtcProcessByte(struct GBACartridgeHardware* hw) {
	--hw->rtc.bytesRemaining;
	if (!hw->rtc.commandActive) {
		union RTCCommandData command;
		command.packed = hw->rtc.bits;
		if (command.magic == 0x06) {
			hw->rtc.command = command;

			hw->rtc.bytesRemaining = RTC_BYTES[hw->rtc.command.command];
			hw->rtc.commandActive = hw->rtc.bytesRemaining > 0;
			switch (command.command) {
			case RTC_RESET:
				hw->rtc.control.packed = 0;
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
		switch (hw->rtc.command.command) {
		case RTC_CONTROL:
			hw->rtc.control.packed = hw->rtc.bits;
			break;
		case RTC_FORCE_IRQ:
			GBALog(hw->p, GBA_LOG_STUB, "Unimplemented RTC command %u", hw->rtc.command.command);
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
		hw->rtc.command.reading = 0;
	}
}

unsigned _rtcOutput(struct GBACartridgeHardware* hw) {
	uint8_t outputByte = 0;
	switch (hw->rtc.command.command) {
	case RTC_CONTROL:
		outputByte = hw->rtc.control.packed;
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
		rtc->sample(rtc);
		t = rtc->unixTime(rtc);
	} else {
		t = time(0);
	}
	struct tm date;
#ifdef _WIN32
	date = *localtime(&t);
#else
	localtime_r(&t, &date);
#endif
	hw->rtc.time[0] = _rtcBCD(date.tm_year - 100);
	hw->rtc.time[1] = _rtcBCD(date.tm_mon + 1);
	hw->rtc.time[2] = _rtcBCD(date.tm_mday);
	hw->rtc.time[3] = _rtcBCD(date.tm_wday);
	if (hw->rtc.control.hour24) {
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
	struct GBARotationSource* gyro = hw->p->rotationSource;
	if (!gyro) {
		return;
	}

	if (hw->p0) {
		if (gyro->sample) {
			gyro->sample(gyro);
		}
		int32_t sample = gyro->readGyroZ(gyro);

		// Normalize to ~12 bits, focused on 0x6C0
		hw->gyroSample = (sample >> 21) + 0x6C0; // Crop off an extra bit so that we can't go negative
	}

	if (hw->gyroEdge && !hw->p1) {
		// Write bit on falling edge
		unsigned bit = hw->gyroSample >> 15;
		hw->gyroSample <<= 1;
		_outputPins(hw, bit << 2);
	}

	hw->gyroEdge = hw->p1;
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

	rumble->setRumble(rumble, hw->p3);
}

// == Light sensor

void GBAHardwareInitLight(struct GBACartridgeHardware* hw) {
	hw->devices |= HW_LIGHT_SENSOR;
	hw->lightCounter = 0;
	hw->lightEdge = false;
	hw->lightSample = 0xFF;
}

void _lightReadPins(struct GBACartridgeHardware* hw) {
	if (hw->p2) {
		// Boktai chip select
		return;
	}
	if (hw->p1) {
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
	if (hw->p0 && hw->lightEdge) {
		++hw->lightCounter;
	}
	hw->lightEdge = !hw->p0;

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

// == Serialization

void GBAHardwareSerialize(struct GBACartridgeHardware* hw, struct GBASerializedState* state) {
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
}

void GBAHardwareDeserialize(struct GBACartridgeHardware* hw, struct GBASerializedState* state) {
	hw->readWrite = state->hw.readWrite;
	hw->pinState = state->hw.pinState;
	hw->direction = state->hw.pinDirection;
	// TODO: Deterministic RTC
	hw->rtc = state->hw.rtc;
	hw->gyroSample = state->hw.gyroSample;
	hw->gyroEdge = state->hw.gyroEdge;
	hw->tiltX = state->hw.tiltSampleX;
	hw->tiltY = state->hw.tiltSampleY;
	hw->tiltState = state->hw.tiltState;
	hw->lightCounter = state->hw.lightCounter;
	hw->lightSample = state->hw.lightSample;
	hw->lightEdge = state->hw.lightEdge;
}
