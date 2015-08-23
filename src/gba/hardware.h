/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_HARDWARE_H
#define GBA_HARDWARE_H

#include "util/common.h"
#include "gba/interface.h"

#include "macros.h"

#include <time.h>

#define IS_GPIO_REGISTER(reg) ((reg) == GPIO_REG_DATA || (reg) == GPIO_REG_DIRECTION || (reg) == GPIO_REG_CONTROL)

struct GBARTCGenericSource {
	struct GBARTCSource d;
	struct GBA* p;
	enum {
		RTC_NO_OVERRIDE,
		RTC_FIXED,
		RTC_FAKE_EPOCH
	} override;
	int64_t value;
};

enum GBAHardwareDevice {
	HW_NO_OVERRIDE = 0x8000,
	HW_NONE = 0,
	HW_RTC = 1,
	HW_RUMBLE = 2,
	HW_LIGHT_SENSOR = 4,
	HW_GYRO = 8,
	HW_TILT = 16,
	HW_GB_PLAYER = 32,
	HW_GB_PLAYER_DETECTION = 64
};

enum GPIORegister {
	GPIO_REG_DATA = 0xC4,
	GPIO_REG_DIRECTION = 0xC6,
	GPIO_REG_CONTROL = 0xC8
};

enum GPIODirection {
	GPIO_WRITE_ONLY = 0,
	GPIO_READ_WRITE = 1
};

DECL_BITFIELD(RTCControl, uint32_t);
DECL_BIT(RTCControl, MinIRQ, 3);
DECL_BIT(RTCControl, Hour24, 6);
DECL_BIT(RTCControl, Poweroff, 7);

enum RTCCommand {
	RTC_RESET = 0,
	RTC_DATETIME = 2,
	RTC_FORCE_IRQ = 3,
	RTC_CONTROL = 4,
	RTC_TIME = 6
};

DECL_BITFIELD(RTCCommandData, uint32_t);
DECL_BITS(RTCCommandData, Magic, 0, 4);
DECL_BITS(RTCCommandData, Command, 4, 3);
DECL_BIT(RTCCommandData, Reading, 7);

#pragma pack(push, 1)
struct GBARTC {
	int32_t bytesRemaining;
	int32_t transferStep;
	int32_t bitsRead;
	int32_t bits;
	int32_t commandActive;
	RTCCommandData command;
	RTCControl control;
	uint8_t time[7];
};
#pragma pack(pop)

struct GBARumble {
	void (*setRumble)(struct GBARumble*, int enable);
};

struct GBAGBPKeyCallback {
	struct GBAKeyCallback d;
	struct GBACartridgeHardware* p;
};

struct GBAGBPSIODriver {
	struct GBASIODriver d;
	struct GBACartridgeHardware* p;
};

DECL_BITFIELD(GPIOPin, uint16_t);

struct GBACartridgeHardware {
	struct GBA* p;
	int devices;
	enum GPIODirection readWrite;
	uint16_t* gpioBase;

	uint16_t pinState;
	uint16_t direction;

	struct GBARTC rtc;

	uint16_t gyroSample;
	bool gyroEdge;

	unsigned lightCounter : 12;
	uint8_t lightSample;
	bool lightEdge;

	uint16_t tiltX;
	uint16_t tiltY;
	int tiltState;

	unsigned gbpInputsPosted;
	int gbpTxPosition;
	int32_t gbpNextEvent;
	struct GBAGBPKeyCallback gbpCallback;
	struct GBAGBPSIODriver gbpDriver;
};

void GBAHardwareInit(struct GBACartridgeHardware* gpio, uint16_t* gpioBase);
void GBAHardwareClear(struct GBACartridgeHardware* gpio);

void GBAHardwareInitRTC(struct GBACartridgeHardware* gpio);
void GBAHardwareInitGyro(struct GBACartridgeHardware* gpio);
void GBAHardwareInitRumble(struct GBACartridgeHardware* gpio);
void GBAHardwareInitLight(struct GBACartridgeHardware* gpio);
void GBAHardwareInitTilt(struct GBACartridgeHardware* gpio);

void GBAHardwareGPIOWrite(struct GBACartridgeHardware* gpio, uint32_t address, uint16_t value);
void GBAHardwareTiltWrite(struct GBACartridgeHardware* gpio, uint32_t address, uint8_t value);
uint8_t GBAHardwareTiltRead(struct GBACartridgeHardware* gpio, uint32_t address);

struct GBAVideo;
void GBAHardwarePlayerUpdate(struct GBA* gba);
bool GBAHardwarePlayerCheckScreen(const struct GBAVideo* video);

void GBARTCGenericSourceInit(struct GBARTCGenericSource* rtc, struct GBA* gba);

struct GBASerializedState;
void GBAHardwareSerialize(const struct GBACartridgeHardware* gpio, struct GBASerializedState* state);
void GBAHardwareDeserialize(struct GBACartridgeHardware* gpio, const struct GBASerializedState* state);

#endif
