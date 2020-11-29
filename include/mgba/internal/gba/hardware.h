/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_HARDWARE_H
#define GBA_HARDWARE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/gba/interface.h>

mLOG_DECLARE_CATEGORY(GBA_HW);

#define EREADER_DOTCODE_STRIDE 1420
#define EREADER_DOTCODE_SIZE (EREADER_DOTCODE_STRIDE * 40)
#define EREADER_CARDS_MAX 16

#define IS_GPIO_REGISTER(reg) ((reg) == GPIO_REG_DATA || (reg) == GPIO_REG_DIRECTION || (reg) == GPIO_REG_CONTROL)

struct GBARTCGenericSource {
	struct mRTCSource d;
	struct GBA* p;
	enum mRTCGenericType override;
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
	HW_GB_PLAYER_DETECTION = 64,
	HW_EREADER = 128
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

struct GBAGBPKeyCallback {
	struct mKeyCallback d;
	struct GBACartridgeHardware* p;
};

struct GBAGBPSIODriver {
	struct GBASIODriver d;
	struct GBACartridgeHardware* p;
};

DECL_BITFIELD(GPIOPin, uint16_t);

DECL_BITFIELD(EReaderControl0, uint8_t);
DECL_BIT(EReaderControl0, Data, 0);
DECL_BIT(EReaderControl0, Clock, 1);
DECL_BIT(EReaderControl0, Direction, 2);
DECL_BIT(EReaderControl0, LedEnable, 3);
DECL_BIT(EReaderControl0, Scan, 4);
DECL_BIT(EReaderControl0, Phi, 5);
DECL_BIT(EReaderControl0, PowerEnable, 6);
DECL_BITFIELD(EReaderControl1, uint8_t);
DECL_BIT(EReaderControl1, Scanline, 1);
DECL_BIT(EReaderControl1, Unk1, 4);
DECL_BIT(EReaderControl1, Voltage, 5);

enum EReaderStateMachine {
	EREADER_SERIAL_INACTIVE = 0,
	EREADER_SERIAL_STARTING,
	EREADER_SERIAL_BIT_0,
	EREADER_SERIAL_BIT_1,
	EREADER_SERIAL_BIT_2,
	EREADER_SERIAL_BIT_3,
	EREADER_SERIAL_BIT_4,
	EREADER_SERIAL_BIT_5,
	EREADER_SERIAL_BIT_6,
	EREADER_SERIAL_BIT_7,
	EREADER_SERIAL_END_BIT,
};

enum EReaderCommand {
	EREADER_COMMAND_IDLE = 0, // TODO: Verify on hardware
	EREADER_COMMAND_WRITE_DATA = 1,
	EREADER_COMMAND_SET_INDEX = 0x22,
	EREADER_COMMAND_READ_DATA = 0x23,
};

struct EReaderCard {
	void* data;
	size_t size;
};

struct GBACartridgeHardware {
	struct GBA* p;
	uint32_t devices;
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
	struct mTimingEvent gbpNextEvent;
	struct GBAGBPKeyCallback gbpCallback;
	struct GBAGBPSIODriver gbpDriver;

	uint16_t eReaderData[44];
	uint8_t eReaderSerial[92];
	uint16_t eReaderRegisterUnk;
	uint16_t eReaderRegisterReset;
	EReaderControl0 eReaderRegisterControl0;
	EReaderControl1 eReaderRegisterControl1;
	uint16_t eReaderRegisterLed;

	// TODO: Serialize these
	enum EReaderStateMachine eReaderState;
	enum EReaderCommand eReaderCommand;
	uint8_t eReaderActiveRegister;
	uint8_t eReaderByte;
	int eReaderX;
	int eReaderY;
	uint8_t* eReaderDots;
	struct EReaderCard eReaderCards[EREADER_CARDS_MAX];
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

void GBAHardwareInitEReader(struct GBACartridgeHardware* hw);
void GBAHardwareEReaderWrite(struct GBACartridgeHardware* hw, uint32_t address, uint16_t value);
void GBAHardwareEReaderWriteFlash(struct GBACartridgeHardware* hw, uint32_t address, uint8_t value);
uint16_t GBAHardwareEReaderRead(struct GBACartridgeHardware* hw, uint32_t address);
uint8_t GBAHardwareEReaderReadFlash(struct GBACartridgeHardware* hw, uint32_t address);
void GBAHardwareEReaderScan(struct GBACartridgeHardware* hw, const void* data, size_t size);

void GBARTCGenericSourceInit(struct GBARTCGenericSource* rtc, struct GBA* gba);

struct GBASerializedState;
void GBAHardwareSerialize(const struct GBACartridgeHardware* gpio, struct GBASerializedState* state);
void GBAHardwareDeserialize(struct GBACartridgeHardware* gpio, const struct GBASerializedState* state);

CXX_GUARD_END

#endif
