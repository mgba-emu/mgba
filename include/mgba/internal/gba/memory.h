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
#include <mgba/internal/gba/savedata.h>
#include <mgba/internal/gba/cart/ereader.h>
#include <mgba/internal/gba/cart/gpio.h>
#include <mgba/internal/gba/cart/matrix.h>
#include <mgba/internal/gba/cart/vfame.h>

enum GBAMemoryRegion {
	GBA_REGION_BIOS = 0x0,
	GBA_REGION_EWRAM = 0x2,
	GBA_REGION_IWRAM = 0x3,
	GBA_REGION_IO = 0x4,
	GBA_REGION_PALETTE_RAM = 0x5,
	GBA_REGION_VRAM = 0x6,
	GBA_REGION_OAM = 0x7,
	GBA_REGION_ROM0 = 0x8,
	GBA_REGION_ROM0_EX = 0x9,
	GBA_REGION_ROM1 = 0xA,
	GBA_REGION_ROM1_EX = 0xB,
	GBA_REGION_ROM2 = 0xC,
	GBA_REGION_ROM2_EX = 0xD,
	GBA_REGION_SRAM = 0xE,
	GBA_REGION_SRAM_MIRROR = 0xF
};

enum GBAMemoryBase {
	GBA_BASE_BIOS = 0x00000000,
	GBA_BASE_EWRAM = 0x02000000,
	GBA_BASE_IWRAM = 0x03000000,
	GBA_BASE_IO = 0x04000000,
	GBA_BASE_PALETTE_RAM = 0x05000000,
	GBA_BASE_VRAM = 0x06000000,
	GBA_BASE_OAM = 0x07000000,
	GBA_BASE_ROM0 = 0x08000000,
	GBA_BASE_ROM0_EX = 0x09000000,
	GBA_BASE_ROM1 = 0x0A000000,
	GBA_BASE_ROM1_EX = 0x0B000000,
	GBA_BASE_ROM2 = 0x0C000000,
	GBA_BASE_ROM2_EX = 0x0D000000,
	GBA_BASE_SRAM = 0x0E000000,
	GBA_BASE_SRAM_MIRROR = 0x0F000000
};

enum {
	GBA_SIZE_BIOS = 0x00004000,
	GBA_SIZE_EWRAM = 0x00040000,
	GBA_SIZE_IWRAM = 0x00008000,
	GBA_SIZE_IO = 0x00000400,
	GBA_SIZE_PALETTE_RAM = 0x00000400,
	GBA_SIZE_VRAM = 0x00018000,
	GBA_SIZE_OAM = 0x00000400,
	GBA_SIZE_ROM0 = 0x02000000,
	GBA_SIZE_ROM1 = 0x02000000,
	GBA_SIZE_ROM2 = 0x02000000,
	GBA_SIZE_SRAM = 0x00008000,
	GBA_SIZE_SRAM512 = 0x00010000,
	GBA_SIZE_FLASH512 = 0x00010000,
	GBA_SIZE_FLASH1M = 0x00020000,
	GBA_SIZE_EEPROM = 0x00002000,
	GBA_SIZE_EEPROM512 = 0x00000200,

	GBA_SIZE_AGB_PRINT = 0x10000
};

enum {
	OFFSET_MASK = 0x00FFFFFF,
	BASE_OFFSET = 24
};

enum {
	AGB_PRINT_BASE = 0x00FD0000,
	AGB_PRINT_TOP = 0x00FE0000,
	AGB_PRINT_PROTECT = 0x00FE2FFE,
	AGB_PRINT_STRUCT = 0x00FE20F8,
	AGB_PRINT_FLUSH_ADDR = 0x00FE209C,
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
	struct GBACartEReader ereader;
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

	uint32_t agbPrintBase;
	uint16_t agbPrintProtect;
	struct GBAPrintContext agbPrintCtx;
	uint16_t* agbPrintBuffer;
	uint16_t agbPrintProtectBackup;
	struct GBAPrintContext agbPrintCtxBackup;
	uint32_t agbPrintFuncBackup;
	uint16_t* agbPrintBufferBackup;

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
void GBAAdjustEWRAMWaitstates(struct GBA* gba, uint16_t parameters);

struct GBASerializedState;
void GBAMemorySerialize(const struct GBAMemory* memory, struct GBASerializedState* state);
void GBAMemoryDeserialize(struct GBAMemory* memory, const struct GBASerializedState* state);

void GBAPrintFlush(struct GBA* gba);

CXX_GUARD_END

#endif
