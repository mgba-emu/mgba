/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_MEMORY_H
#define GB_MEMORY_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/gb/interface.h>

mLOG_DECLARE_CATEGORY(GB_MBC);
mLOG_DECLARE_CATEGORY(GB_MEM);

struct GB;

enum {
	GB_BASE_CART_BANK0 = 0x0000,
	GB_BASE_CART_BANK1 = 0x4000,
	GB_BASE_CART_HALFBANK1 = 0x4000,
	GB_BASE_CART_HALFBANK2 = 0x6000,
	GB_BASE_VRAM = 0x8000,
	GB_BASE_EXTERNAL_RAM = 0xA000,
	GB_BASE_EXTERNAL_RAM_HALFBANK0 = 0xA000,
	GB_BASE_EXTERNAL_RAM_HALFBANK1 = 0xB000,
	GB_BASE_WORKING_RAM_BANK0 = 0xC000,
	GB_BASE_WORKING_RAM_BANK1 = 0xD000,
	GB_BASE_OAM = 0xFE00,
	GB_BASE_UNUSABLE = 0xFEA0,
	GB_BASE_IO = 0xFF00,
	GB_BASE_HRAM = 0xFF80,
	GB_BASE_IE = 0xFFFF
};

enum {
	GB_REGION_CART_BANK0 = 0x0,
	GB_REGION_CART_BANK1 = 0x4,
	GB_REGION_VRAM = 0x8,
	GB_REGION_EXTERNAL_RAM = 0xA,
	GB_REGION_WORKING_RAM_BANK0 = 0xC,
	GB_REGION_WORKING_RAM_BANK1 = 0xD,
	GB_REGION_WORKING_RAM_BANK1_MIRROR = 0xE,
	GB_REGION_OTHER = 0xF,
};

enum {
	GB_SIZE_CART_BANK0 = 0x4000,
	GB_SIZE_CART_HALFBANK = 0x2000,
	GB_SIZE_CART_MAX = 0x800000,
	GB_SIZE_VRAM = 0x4000,
	GB_SIZE_VRAM_BANK0 = 0x2000,
	GB_SIZE_EXTERNAL_RAM = 0x2000,
	GB_SIZE_EXTERNAL_RAM_HALFBANK = 0x1000,
	GB_SIZE_WORKING_RAM = 0x8000,
	GB_SIZE_WORKING_RAM_BANK0 = 0x1000,
	GB_SIZE_OAM = 0xA0,
	GB_SIZE_IO = 0x80,
	GB_SIZE_HRAM = 0x7F,

	GB_SIZE_MBC6_FLASH = 0x100000,
};

struct GBMemory;
typedef void (*GBMemoryBankControllerWrite)(struct GB*, uint16_t address, uint8_t value);
typedef uint8_t (*GBMemoryBankControllerRead)(struct GBMemory*, uint16_t address);

DECL_BITFIELD(GBMBC7Field, uint8_t);
DECL_BIT(GBMBC7Field, CS, 7);
DECL_BIT(GBMBC7Field, CLK, 6);
DECL_BIT(GBMBC7Field, DI, 1);
DECL_BIT(GBMBC7Field, DO, 0);

enum GBMBC7MachineState {
	GBMBC7_STATE_IDLE = 0,
	GBMBC7_STATE_READ_COMMAND = 1,
	GBMBC7_STATE_DO = 2,

	GBMBC7_STATE_EEPROM_EWDS = 0x10,
	GBMBC7_STATE_EEPROM_WRAL = 0x11,
	GBMBC7_STATE_EEPROM_ERAL = 0x12,
	GBMBC7_STATE_EEPROM_EWEN = 0x13,
	GBMBC7_STATE_EEPROM_WRITE = 0x14,
	GBMBC7_STATE_EEPROM_READ = 0x18,
	GBMBC7_STATE_EEPROM_ERASE = 0x1C,
};

enum GBTAMA5Register {
	GBTAMA5_BANK_LO = 0x0,
	GBTAMA5_BANK_HI = 0x1,
	GBTAMA5_WRITE_LO = 0x4,
	GBTAMA5_WRITE_HI = 0x5,
	GBTAMA5_ADDR_HI = 0x6,
	GBTAMA5_ADDR_LO = 0x7,
	GBTAMA5_MAX = 0x8,
	GBTAMA5_ACTIVE = 0xA,
	GBTAMA5_READ_LO = 0xC,
	GBTAMA5_READ_HI = 0xD,
};

enum GBTAMA6RTCRegister {
	GBTAMA6_RTC_PA0_SECOND_1 = 0x0,
	GBTAMA6_RTC_PA0_SECOND_10 = 0x1,
	GBTAMA6_RTC_PA0_MINUTE_1 = 0x2,
	GBTAMA6_RTC_PA0_MINUTE_10 = 0x3,
	GBTAMA6_RTC_PA0_HOUR_1 = 0x4,
	GBTAMA6_RTC_PA0_HOUR_10 = 0x5,
	GBTAMA6_RTC_PA0_WEEK = 0x6,
	GBTAMA6_RTC_PA0_DAY_1 = 0x7,
	GBTAMA6_RTC_PA0_DAY_10 = 0x8,
	GBTAMA6_RTC_PA0_MONTH_1 = 0x9,
	GBTAMA6_RTC_PA0_MONTH_10 = 0xA,
	GBTAMA6_RTC_PA0_YEAR_1 = 0xB,
	GBTAMA6_RTC_PA0_YEAR_10 = 0xC,
	GBTAMA6_RTC_PA1_MINUTE_1 = 0x2,
	GBTAMA6_RTC_PA1_MINUTE_10 = 0x3,
	GBTAMA6_RTC_PA1_HOUR_1 = 0x4,
	GBTAMA6_RTC_PA1_HOUR_10 = 0x5,
	GBTAMA6_RTC_PA1_WEEK = 0x6,
	GBTAMA6_RTC_PA1_DAY_1 = 0x7,
	GBTAMA6_RTC_PA1_DAY_10 = 0x8,
	GBTAMA6_RTC_PA1_24_HOUR = 0xA,
	GBTAMA6_RTC_PA1_LEAP_YEAR = 0xB,
	GBTAMA6_RTC_PAGE = 0xD,
	GBTAMA6_RTC_TEST = 0xE,
	GBTAMA6_RTC_RESET = 0xF,
	GBTAMA6_RTC_MAX
};

enum GBTAMA6Command {
	GBTAMA6_DISABLE_TIMER = 0x0,
	GBTAMA6_ENABLE_TIMER = 0x1,
	GBTAMA6_MINUTE_WRITE = 0x4,
	GBTAMA6_HOUR_WRITE = 0x5,
	GBTAMA6_MINUTE_READ = 0x6,
	GBTAMA6_HOUR_READ = 0x7,
	GBTAMA6_DISABLE_ALARM = 0x10,
	GBTAMA6_ENABLE_ALARM = 0x11,
};

enum GBHuC3Register {
	GBHUC3_RTC_MINUTES_LO = 0x10,
	GBHUC3_RTC_MINUTES_MI = 0x11,
	GBHUC3_RTC_MINUTES_HI = 0x12,
	GBHUC3_RTC_DAYS_LO = 0x13,
	GBHUC3_RTC_DAYS_MI = 0x14,
	GBHUC3_RTC_DAYS_HI = 0x15,
	GBHUC3_RTC_ENABLE = 0x16,
	GBHUC3_SPEAKER_TONE = 0x26,
	GBHUC3_SPEAKER_ENABLE = 0x27,
	GBHUC3_ALARM_MINUTES_LO = 0x58,
	GBHUC3_ALARM_MINUTES_MI = 0x59,
	GBHUC3_ALARM_MINUTES_HI = 0x5A,
	GBHUC3_ALARM_DAYS_LO = 0x5B,
	GBHUC3_ALARM_DAYS_MI = 0x5C,
	GBHUC3_ALARM_DAYS_HI = 0x5D,
	GBHUC3_ALARM_TONE = 0x5E,
	GBHUC3_ALARM_ENABLE = 0x5F,
};

enum GBHuC3Mode {
	GBHUC3_MODE_SRAM_RO = 0x0,
	GBHUC3_MODE_SRAM_RW = 0xA,
	GBHUC3_MODE_IN = 0xB,
	GBHUC3_MODE_OUT = 0xC,
	GBHUC3_MODE_COMMIT = 0xD,
};

enum GBHuC3Command {
	GBHUC3_CMD_LATCH = 0x0,
	GBHUC3_CMD_SET_RTC = 0x1,
	GBHUC3_CMD_RO = 0x2,
	GBHUC3_CMD_TONE = 0xE,
};

enum GBSachenLockMode {
	GB_SACHEN_LOCKED_DMG = 0,
	GB_SACHEN_LOCKED_CGB,
	GB_SACHEN_UNLOCKED
};

struct GBMBC1State {
	int mode;
	int multicartStride;
	uint8_t bankLo;
	uint8_t bankHi;
};

struct GBMBC6State {
	bool flashBank0;
	bool flashBank1;
};

struct GBMBC7State {
	enum GBMBC7MachineState state;
	uint16_t sr;
	uint8_t address;
	bool writable;
	int srBits;
	uint8_t access;
	uint8_t latch;
	GBMBC7Field eeprom;
};

struct GBMMM01State {
	bool locked;
	int currentBank0;
};

struct GBPocketCamState {
	bool registersActive;
	uint8_t registers[0x36];
};

struct GBTAMA5State {
	uint8_t reg;
	bool disabled;
	uint8_t registers[GBTAMA5_MAX];
	uint8_t rtcTimerPage[GBTAMA6_RTC_MAX];
	uint8_t rtcAlarmPage[GBTAMA6_RTC_MAX];
	uint8_t rtcFreePage0[GBTAMA6_RTC_MAX];
	uint8_t rtcFreePage1[GBTAMA6_RTC_MAX];
};

struct GBHuC3State {
	uint8_t index;
	uint8_t value;
	uint8_t mode;
	uint8_t registers[256];
};

struct GBPKJDState {
	uint8_t reg[2];
};

struct GBNTOldState {
	bool swapped;
	uint8_t baseBank;
	uint8_t bankCount;
	bool rumble;
};

struct GBNTNewState {
	bool splitMode;
};

struct GBBBDState {
	int dataSwapMode;
	int bankSwapMode;
};

struct GBSachenState {
	enum GBSachenLockMode locked;
	int transition;
	uint8_t mask;
	uint8_t unmaskedBank;
	uint8_t baseBank;
};

union GBMBCState {
	struct GBMBC1State mbc1;
	struct GBMBC6State mbc6;
	struct GBMBC7State mbc7;
	struct GBMMM01State mmm01;
	struct GBPocketCamState pocketCam;
	struct GBTAMA5State tama5;
	struct GBHuC3State huc3;
	struct GBNTOldState ntOld;
	struct GBNTNewState ntNew;
	struct GBPKJDState pkjd;
	struct GBBBDState bbd;
	struct GBSachenState sachen;
};

struct mRotationSource;
struct GBMemory {
	uint8_t* rom;
	uint8_t* romBase;
	uint8_t* romBank;
	enum GBMemoryBankControllerType mbcType;
	GBMemoryBankControllerWrite mbcWrite;
	GBMemoryBankControllerRead mbcRead;
	union GBMBCState mbcState;
	int currentBank;
	int currentBank0;
	int currentBank1;
	uint8_t* romBank1;
	int currentSramBank1;
	uint8_t* sramBank1;

	unsigned cartBusDecay;
	uint16_t cartBusPc;
	uint8_t cartBus;

	uint8_t* wram;
	uint8_t* wramBank;
	int wramCurrentBank;

	bool mbcReadBank0;
	bool mbcReadBank1;
	bool mbcReadHigh;
	bool mbcWriteHigh;

	bool sramAccess;
	bool directSramAccess;
	uint8_t* sram;
	uint8_t* sramBank;
	int sramCurrentBank;

	uint8_t io[GB_SIZE_IO];
	bool ime;
	uint8_t ie;

	uint8_t hram[GB_SIZE_HRAM];

	uint16_t dmaSource;
	uint16_t dmaDest;
	int dmaRemaining;

	uint16_t hdmaSource;
	uint16_t hdmaDest;
	int hdmaRemaining;
	bool isHdma;

	struct mTimingEvent dmaEvent;
	struct mTimingEvent hdmaEvent;

	size_t romSize;

	bool rtcAccess;
	int activeRtcReg;
	bool rtcLatched;
	uint8_t rtcRegs[5];
	time_t rtcLastLatch;
	struct mRTCSource* rtc;
	struct mRotationSource* rotation;
	struct mRumble* rumble;
	struct mImageSource* cam;
};

struct SM83Core;
void GBMemoryInit(struct GB* gb);
void GBMemoryDeinit(struct GB* gb);

void GBMemoryReset(struct GB* gb);
void GBMemorySwitchWramBank(struct GBMemory* memory, int bank);

uint8_t GBLoad8(struct SM83Core* cpu, uint16_t address);
void GBStore8(struct SM83Core* cpu, uint16_t address, int8_t value);

int GBCurrentSegment(struct SM83Core* cpu, uint16_t address);

uint8_t GBView8(struct SM83Core* cpu, uint16_t address, int segment);

void GBMemoryDMA(struct GB* gb, uint16_t base);
uint8_t GBMemoryWriteHDMA5(struct GB* gb, uint8_t value);

void GBPatch8(struct SM83Core* cpu, uint16_t address, int8_t value, int8_t* old, int segment);

struct GBSerializedState;
void GBMemorySerialize(const struct GB* gb, struct GBSerializedState* state);
void GBMemoryDeserialize(struct GB* gb, const struct GBSerializedState* state);

CXX_GUARD_END

#endif
