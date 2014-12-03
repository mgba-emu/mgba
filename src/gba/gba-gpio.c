/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba.h"

#include "gba-gpio.h"
#include "gba-sensors.h"
#include "gba-serialize.h"

#include <time.h>

static void _readPins(struct GBACartridgeGPIO* gpio);
static void _outputPins(struct GBACartridgeGPIO* gpio, unsigned pins);

static void _rtcReadPins(struct GBACartridgeGPIO* gpio);
static unsigned _rtcOutput(struct GBACartridgeGPIO* gpio);
static void _rtcProcessByte(struct GBACartridgeGPIO* gpio);
static void _rtcUpdateClock(struct GBACartridgeGPIO* gpio);
static unsigned _rtcBCD(unsigned value);

static void _gyroReadPins(struct GBACartridgeGPIO* gpio);

static void _rumbleReadPins(struct GBACartridgeGPIO* gpio);

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

void GBAGPIOInit(struct GBACartridgeGPIO* gpio, uint16_t* base) {
	gpio->gpioDevices = GPIO_NONE;
	gpio->direction = GPIO_WRITE_ONLY;
	gpio->gpioBase = base;
	gpio->pinState = 0;
	gpio->direction = 0;
}

void GBAGPIOWrite(struct GBACartridgeGPIO* gpio, uint32_t address, uint16_t value) {
	switch (address) {
	case GPIO_REG_DATA:
		gpio->pinState = value;
		_readPins(gpio);
		break;
	case GPIO_REG_DIRECTION:
		gpio->direction = value;
		break;
	case GPIO_REG_CONTROL:
		gpio->readWrite = value;
		break;
	default:
		GBALog(gpio->p, GBA_LOG_WARN, "Invalid GPIO address");
	}

	if (gpio->readWrite) {
		uint16_t old = gpio->gpioBase[0];
		old &= ~gpio->direction;
		gpio->gpioBase[0] = old | (value & gpio->direction);
	}
}

void GBAGPIOInitRTC(struct GBACartridgeGPIO* gpio) {
	gpio->gpioDevices |= GPIO_RTC;
	gpio->rtc.bytesRemaining = 0;

	gpio->rtc.transferStep = 0;

	gpio->rtc.bitsRead = 0;
	gpio->rtc.bits = 0;
	gpio->rtc.commandActive = 0;
	gpio->rtc.command.packed = 0;
	gpio->rtc.control.packed = 0x40;
	memset(gpio->rtc.time, 0, sizeof(gpio->rtc.time));
}

void _readPins(struct GBACartridgeGPIO* gpio) {
	if (gpio->gpioDevices & GPIO_RTC) {
		_rtcReadPins(gpio);
	}

	if (gpio->gpioDevices & GPIO_GYRO) {
		_gyroReadPins(gpio);
	}

	if (gpio->gpioDevices & GPIO_RUMBLE) {
		_rumbleReadPins(gpio);
	}
}

void _outputPins(struct GBACartridgeGPIO* gpio, unsigned pins) {
	if (gpio->readWrite) {
		uint16_t old = gpio->gpioBase[0];
		old &= gpio->direction;
		gpio->gpioBase[0] = old | (pins & ~gpio->direction & 0xF);
	}
}

// == RTC

void _rtcReadPins(struct GBACartridgeGPIO* gpio) {
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
	switch (gpio->rtc.transferStep) {
	case 0:
		if ((gpio->pinState & 5) == 1) {
			gpio->rtc.transferStep = 1;
		}
		break;
	case 1:
		if ((gpio->pinState & 5) == 5) {
			gpio->rtc.transferStep = 2;
		}
		break;
	case 2:
		if (!gpio->p0) {
			gpio->rtc.bits &= ~(1 << gpio->rtc.bitsRead);
			gpio->rtc.bits |= gpio->p1 << gpio->rtc.bitsRead;
		} else {
			if (gpio->p2) {
				// GPIO direction should always != reading
				if (gpio->dir1) {
					if (gpio->rtc.command.reading) {
						GBALog(gpio->p, GBA_LOG_GAME_ERROR, "Attempting to write to RTC while in read mode");
					}
					++gpio->rtc.bitsRead;
					if (gpio->rtc.bitsRead == 8) {
						_rtcProcessByte(gpio);
					}
				} else {
					_outputPins(gpio, 5 | (_rtcOutput(gpio) << 1));
					++gpio->rtc.bitsRead;
					if (gpio->rtc.bitsRead == 8) {
						--gpio->rtc.bytesRemaining;
						if (gpio->rtc.bytesRemaining <= 0) {
							gpio->rtc.commandActive = 0;
							gpio->rtc.command.reading = 0;
						}
						gpio->rtc.bitsRead = 0;
					}
				}
			} else {
				gpio->rtc.bitsRead = 0;
				gpio->rtc.bytesRemaining = 0;
				gpio->rtc.commandActive = 0;
				gpio->rtc.command.reading = 0;
				gpio->rtc.transferStep = 0;
			}
		}
		break;
	}
}

void _rtcProcessByte(struct GBACartridgeGPIO* gpio) {
	--gpio->rtc.bytesRemaining;
	if (!gpio->rtc.commandActive) {
		union RTCCommandData command;
		command.packed = gpio->rtc.bits;
		if (command.magic == 0x06) {
			gpio->rtc.command = command;

			gpio->rtc.bytesRemaining = RTC_BYTES[gpio->rtc.command.command];
			gpio->rtc.commandActive = gpio->rtc.bytesRemaining > 0;
			switch (command.command) {
			case RTC_RESET:
				gpio->rtc.control.packed = 0;
				break;
			case RTC_DATETIME:
			case RTC_TIME:
				_rtcUpdateClock(gpio);
				break;
			case RTC_FORCE_IRQ:
			case RTC_CONTROL:
				break;
			}
		} else {
			GBALog(gpio->p, GBA_LOG_WARN, "Invalid RTC command byte: %02X", gpio->rtc.bits);
		}
	} else {
		switch (gpio->rtc.command.command) {
		case RTC_CONTROL:
			gpio->rtc.control.packed = gpio->rtc.bits;
			break;
		case RTC_FORCE_IRQ:
			GBALog(gpio->p, GBA_LOG_STUB, "Unimplemented RTC command %u", gpio->rtc.command.command);
			break;
		case RTC_RESET:
		case RTC_DATETIME:
		case RTC_TIME:
			break;
		}
	}

	gpio->rtc.bits = 0;
	gpio->rtc.bitsRead = 0;
	if (!gpio->rtc.bytesRemaining) {
		gpio->rtc.commandActive = 0;
		gpio->rtc.command.reading = 0;
	}
}

unsigned _rtcOutput(struct GBACartridgeGPIO* gpio) {
	uint8_t outputByte = 0;
	switch (gpio->rtc.command.command) {
	case RTC_CONTROL:
		outputByte = gpio->rtc.control.packed;
		break;
	case RTC_DATETIME:
	case RTC_TIME:
		outputByte = gpio->rtc.time[7 - gpio->rtc.bytesRemaining];
		break;
	case RTC_FORCE_IRQ:
	case RTC_RESET:
		break;
	}
	unsigned output = (outputByte >> gpio->rtc.bitsRead) & 1;
	return output;
}

void _rtcUpdateClock(struct GBACartridgeGPIO* gpio) {
	time_t t = time(0);
	struct tm date;
#ifdef _WIN32
	date = *localtime(&t);
#else
	localtime_r(&t, &date);
#endif
	gpio->rtc.time[0] = _rtcBCD(date.tm_year - 100);
	gpio->rtc.time[1] = _rtcBCD(date.tm_mon + 1);
	gpio->rtc.time[2] = _rtcBCD(date.tm_mday);
	gpio->rtc.time[3] = _rtcBCD(date.tm_wday);
	if (gpio->rtc.control.hour24) {
		gpio->rtc.time[4] = _rtcBCD(date.tm_hour);
	} else {
		gpio->rtc.time[4] = _rtcBCD(date.tm_hour % 12);
	}
	gpio->rtc.time[5] = _rtcBCD(date.tm_min);
	gpio->rtc.time[6] = _rtcBCD(date.tm_sec);
}

unsigned _rtcBCD(unsigned value) {
	int counter = value % 10;
	value /= 10;
	counter += (value % 10) << 4;
	return counter;
}

// == Gyro

void GBAGPIOInitGyro(struct GBACartridgeGPIO* gpio) {
	gpio->gpioDevices |= GPIO_GYRO;
	gpio->gyroSample = 0;
	gpio->gyroEdge = 0;
}

void _gyroReadPins(struct GBACartridgeGPIO* gpio) {
	struct GBARotationSource* gyro = gpio->p->rotationSource;
	if (!gyro) {
		return;
	}

	if (gpio->p0) {
		if (gyro->sample) {
			gyro->sample(gyro);
		}
		int32_t sample = gyro->readGyroZ(gyro);

		// Normalize to ~12 bits, focused on 0x6C0
		gpio->gyroSample = (sample >> 21) + 0x6C0; // Crop off an extra bit so that we can't go negative
	}

	if (gpio->gyroEdge && !gpio->p1) {
		// Write bit on falling edge
		unsigned bit = gpio->gyroSample >> 15;
		gpio->gyroSample <<= 1;
		_outputPins(gpio, bit << 2);
	}

	gpio->gyroEdge = gpio->p1;
}

// == Rumble

void GBAGPIOInitRumble(struct GBACartridgeGPIO* gpio) {
	gpio->gpioDevices |= GPIO_RUMBLE;
}

void _rumbleReadPins(struct GBACartridgeGPIO* gpio) {
	struct GBARumble* rumble = gpio->p->rumble;
	if (!rumble) {
		return;
	}

	rumble->setRumble(rumble, gpio->p3);
}

// == Serialization

void GBAGPIOSerialize(struct GBACartridgeGPIO* gpio, struct GBASerializedState* state) {
	state->gpio.readWrite = gpio->readWrite;
	state->gpio.pinState = gpio->pinState;
	state->gpio.pinDirection = gpio->direction;
	state->gpio.devices = gpio->gpioDevices;
	state->gpio.rtc = gpio->rtc;
	state->gpio.gyroSample = gpio->gyroSample;
	state->gpio.gyroEdge = gpio->gyroEdge;
}

void GBAGPIODeserialize(struct GBACartridgeGPIO* gpio, struct GBASerializedState* state) {
	gpio->readWrite = state->gpio.readWrite;
	gpio->pinState = state->gpio.pinState;
	gpio->direction = state->gpio.pinDirection;
	// TODO: Deterministic RTC
	gpio->rtc = state->gpio.rtc;
	gpio->gyroSample = state->gpio.gyroSample;
	gpio->gyroEdge = state->gpio.gyroEdge;
}
