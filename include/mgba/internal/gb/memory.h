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
	GB_SIZE_WORKING_RAM = 0x8000,
	GB_SIZE_WORKING_RAM_BANK0 = 0x1000,
	GB_SIZE_OAM = 0xA0,
	GB_SIZE_IO = 0x80,
	GB_SIZE_HRAM = 0x7F,
};

enum {
	GB_SRAM_DIRT_NEW = 1,
	GB_SRAM_DIRT_SEEN = 2
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
	GBTAMA5_CS = 0x6,
	GBTAMA5_ADDR_LO = 0x7,
	GBTAMA5_MAX = 0x8,
	GBTAMA5_ACTIVE = 0xA,
	GBTAMA5_READ_LO = 0xC,
	GBTAMA5_READ_HI = 0xD,
};

struct GBMBC1State {
	int mode;
	int multicartStride;
};

struct GBMBC6State {
	int currentBank1;
	uint8_t* romBank1;
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

struct GBPocketCamState {
	bool registersActive;
	uint8_t registers[0x36];
};

struct GBTAMA5State {
	uint8_t reg;
	uint8_t registers[GBTAMA5_MAX];
};

union GBMBCState {
	struct GBMBC1State mbc1;
	struct GBMBC6State mbc6;
	struct GBMBC7State mbc7;
	struct GBPocketCamState pocketCam;
	struct GBTAMA5State tama5;
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

	uint8_t* wram;
	uint8_t* wramBank;
	int wramCurrentBank;

	bool sramAccess;
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

struct LR35902Core;
void GBMemoryInit(struct GB* gb);
void GBMemoryDeinit(struct GB* gb);

void GBMemoryReset(struct GB* gb);
void GBMemorySwitchWramBank(struct GBMemory* memory, int bank);

uint8_t GBLoad8(struct LR35902Core* cpu, uint16_t address);
void GBStore8(struct LR35902Core* cpu, uint16_t address, int8_t value);

int GBCurrentSegment(struct LR35902Core* cpu, uint16_t address);

uint8_t GBView8(struct LR35902Core* cpu, uint16_t address, int segment);

void GBMemoryDMA(struct GB* gb, uint16_t base);
uint8_t GBMemoryWriteHDMA5(struct GB* gb, uint8_t value);

void GBPatch8(struct LR35902Core* cpu, uint16_t address, int8_t value, int8_t* old, int segment);

struct GBSerializedState;
void GBMemorySerialize(const struct GB* gb, struct GBSerializedState* state);
void GBMemoryDeserialize(struct GB* gb, const struct GBSerializedState* state);

CXX_GUARD_END

#endif
