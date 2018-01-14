/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_MEMORY_H
#define GBA_MEMORY_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/timing.h>

#include <mgba/internal/arm/arm.h>
#include <mgba/internal/gba/dma.h>
#include <mgba/internal/gba/hardware.h>
#include <mgba/internal/gba/savedata.h>
#include <mgba/internal/gba/vfame.h>
#include <mgba/internal/gba/matrix.h>

enum GBAMemoryRegion {
	REGION_BIOS = 0x0,
	REGION_WORKING_RAM = 0x2,
	REGION_WORKING_IRAM = 0x3,
	REGION_IO = 0x4,
	REGION_PALETTE_RAM = 0x5,
	REGION_VRAM = 0x6,
	REGION_OAM = 0x7,
	REGION_CART0 = 0x8,
	REGION_CART0_EX = 0x9,
	REGION_CART1 = 0xA,
	REGION_CART1_EX = 0xB,
	REGION_CART2 = 0xC,
	REGION_CART2_EX = 0xD,
	REGION_CART_SRAM = 0xE,
	REGION_CART_SRAM_MIRROR = 0xF
};

enum GBAMemoryBase {
	BASE_BIOS = 0x00000000,
	BASE_WORKING_RAM = 0x02000000,
	BASE_WORKING_IRAM = 0x03000000,
	BASE_IO = 0x04000000,
	BASE_PALETTE_RAM = 0x05000000,
	BASE_VRAM = 0x06000000,
	BASE_OAM = 0x07000000,
	BASE_CART0 = 0x08000000,
	BASE_CART0_EX = 0x09000000,
	BASE_CART1 = 0x0A000000,
	BASE_CART1_EX = 0x0B000000,
	BASE_CART2 = 0x0C000000,
	BASE_CART2_EX = 0x0D000000,
	BASE_CART_SRAM = 0x0E000000,
	BASE_CART_SRAM_MIRROR = 0x0F000000
};

enum {
	SIZE_BIOS = 0x00004000,
	SIZE_WORKING_RAM = 0x00040000,
	SIZE_WORKING_IRAM = 0x00008000,
	SIZE_IO = 0x00000400,
	SIZE_PALETTE_RAM = 0x00000400,
	SIZE_VRAM = 0x00018000,
	SIZE_OAM = 0x00000400,
	SIZE_CART0 = 0x02000000,
	SIZE_CART1 = 0x02000000,
	SIZE_CART2 = 0x02000000,
	SIZE_CART_SRAM = 0x00008000,
	SIZE_CART_FLASH512 = 0x00010000,
	SIZE_CART_FLASH1M = 0x00020000,
	SIZE_CART_EEPROM = 0x00002000,
	SIZE_CART_EEPROM512 = 0x00000200,

	SIZE_AGB_PRINT = 0x10000
};

enum {
	OFFSET_MASK = 0x00FFFFFF,
	BASE_OFFSET = 24
};

enum {
	AGB_PRINT_BASE = 0x00FD0000,
	AGB_PRINT_TOP = 0x00FE0000,
	AGB_PRINT_PROTECT = 0x00FE2FFE,
	AGB_PRINT_STRUCT = 0x01FE20F8,
	AGB_PRINT_FLUSH_ADDR = 0x01FE209C,
};

mLOG_DECLARE_CATEGORY(GBA_MEM);

struct GBAPrintContext {
	uint16_t request;
	uint16_t bank;
	uint16_t get;
	uint16_t put;
};

struct GBAMemory {
	uint32_t* bios;
	uint32_t* wram;
	uint32_t* iwram;
	uint32_t* rom;
	uint16_t io[512];

	struct GBACartridgeHardware hw;
	struct GBASavedata savedata;
	struct GBAVFameCart vfame;
	struct GBAMatrix matrix;
	size_t romSize;
	uint32_t romMask;
	uint16_t romID;
	int fullBios;

	char waitstatesSeq32[256];
	char waitstatesSeq16[256];
	char waitstatesNonseq32[256];
	char waitstatesNonseq16[256];
	int activeRegion;
	bool prefetch;
	uint32_t lastPrefetchedPc;
	uint32_t biosPrefetch;

	struct GBADMA dma[4];
	struct mTimingEvent dmaEvent;
	int activeDMA;
	uint32_t dmaTransferRegister;

	uint16_t agbPrint;
	struct GBAPrintContext agbPrintCtx;
	uint16_t* agbPrintBuffer;

	bool mirroring;
};

struct GBA;
void GBAMemoryInit(struct GBA* gba);
void GBAMemoryDeinit(struct GBA* gba);

void GBAMemoryReset(struct GBA* gba);

uint32_t GBALoad32(struct ARMCore* cpu, uint32_t address, int* cycleCounter);
uint32_t GBALoad16(struct ARMCore* cpu, uint32_t address, int* cycleCounter);
uint32_t GBALoad8(struct ARMCore* cpu, uint32_t address, int* cycleCounter);

uint32_t GBALoadBad(struct ARMCore* cpu);

void GBAStore32(struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter);
void GBAStore16(struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter);
void GBAStore8(struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter);

uint32_t GBAView32(struct ARMCore* cpu, uint32_t address);
uint16_t GBAView16(struct ARMCore* cpu, uint32_t address);
uint8_t GBAView8(struct ARMCore* cpu, uint32_t address);

void GBAPatch32(struct ARMCore* cpu, uint32_t address, int32_t value, int32_t* old);
void GBAPatch16(struct ARMCore* cpu, uint32_t address, int16_t value, int16_t* old);
void GBAPatch8(struct ARMCore* cpu, uint32_t address, int8_t value, int8_t* old);

uint32_t GBALoadMultiple(struct ARMCore*, uint32_t baseAddress, int mask, enum LSMDirection direction,
                         int* cycleCounter);
uint32_t GBAStoreMultiple(struct ARMCore*, uint32_t baseAddress, int mask, enum LSMDirection direction,
                          int* cycleCounter);

void GBAAdjustWaitstates(struct GBA* gba, uint16_t parameters);

struct GBASerializedState;
void GBAMemorySerialize(const struct GBAMemory* memory, struct GBASerializedState* state);
void GBAMemoryDeserialize(struct GBAMemory* memory, const struct GBASerializedState* state);

void GBAPrintFlush(struct GBA* gba);

CXX_GUARD_END

#endif
