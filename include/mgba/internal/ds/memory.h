/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_MEMORY_H
#define DS_MEMORY_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/ds/dma.h>
#include <mgba/internal/ds/io.h>

enum DSMemoryRegion {
	DS7_REGION_BIOS = 0x0,
	DS9_REGION_ITCM = 0x0,
	DS9_REGION_ITCM_MIRROR = 0x1,
	DS_REGION_RAM = 0x2,
	DS_REGION_WORKING_RAM = 0x3,
	DS_REGION_IO = 0x4,
	DS9_REGION_PALETTE_RAM = 0x5,
	DS_REGION_VRAM = 0x6,
	DS9_REGION_OAM = 0x7,
	DS_REGION_SLOT2 = 0x8,
	DS_REGION_SLOT2_EX = 0x9,
	DS_REGION_SLOT2_SRAM = 0xA,
	DS9_REGION_BIOS = 0xFF,
};

enum DSMemoryBase {
	DS7_BASE_BIOS = 0x00000000,
	DS9_BASE_ITCM = 0x00000000,
	DS_BASE_RAM = 0x02000000,
	DS9_BASE_DTCM = 0x027C0000,
	DS_BASE_WORKING_RAM = 0x03000000,
	DS7_BASE_WORKING_RAM = 0x03800000,
	DS_BASE_IO = 0x04000000,
	DS9_BASE_PALETTE_RAM = 0x05000000,
	DS_BASE_VRAM = 0x06000000,
	DS9_BASE_OAM = 0x07000000,
	DS_BASE_SLOT2 = 0x08000000,
	DS_BASE_SLOT2_EX = 0x09000000,
	DS9_BASE_BIOS = 0xFFFF0000,
};

enum {
	DS9_SIZE_ITCM = 0x00008000,
	DS9_SIZE_DTCM = 0x00004000,
	DS7_SIZE_BIOS = 0x00004000,
	DS9_SIZE_BIOS = 0x00008000,
	DS_SIZE_RAM = 0x00400000,
	DS_SIZE_VRAM = 0x000A4000,
	DS_SIZE_WORKING_RAM = 0x00008000,
	DS7_SIZE_WORKING_RAM = 0x00010000,
	DS9_SIZE_PALETTE_RAM = 0x00000800,
	DS9_SIZE_OAM = 0x00000800,
	DS_SIZE_SLOT2 = 0x02000000,
	DS_SIZE_SLOT2_SRAM = 0x00010000,
};

enum {
	DS_OFFSET_MASK = 0x00FFFFFF,
	DS_BASE_OFFSET = 24,
	DS_VRAM_OFFSET = 14
};

mLOG_DECLARE_CATEGORY(DS_MEM);

struct DSMemory {
	uint32_t* bios7;
	uint32_t* bios9;
	uint32_t* itcm;
	uint32_t* dtcm;
	uint32_t* ram;
	uint32_t* wram;
	uint32_t* wram7;
	uint32_t* rom;
	uint16_t io7[DS7_REG_MAX >> 1];
	uint16_t io9[DS9_REG_MAX >> 1];

	uint16_t vramMirror[9][0x40];
	uint16_t vramMode[9][8];
	uint16_t* vramBank[9];

	size_t romSize;
	size_t wramSize7;
	size_t wramSize9;

	uint32_t dtcmBase;
	uint32_t dtcmSize;
	uint32_t itcmSize;
};

struct DSCoreMemory {
	uint16_t* io;
	int activeRegion;

	char waitstatesSeq32[256];
	char waitstatesSeq16[256];
	char waitstatesNonseq32[256];
	char waitstatesNonseq16[256];
	char waitstatesPrefetchSeq32[16];
	char waitstatesPrefetchSeq16[16];
	char waitstatesPrefetchNonseq32[16];
	char waitstatesPrefetchNonseq16[16];

	struct GBADMA dma[4];
	struct mTimingEvent dmaEvent;
	int activeDMA;
};

struct DS;
void DSMemoryInit(struct DS* ds);
void DSMemoryDeinit(struct DS* ds);

void DSMemoryReset(struct DS* ds);

uint32_t DS7Load32(struct ARMCore* cpu, uint32_t address, int* cycleCounter);
uint32_t DS7Load16(struct ARMCore* cpu, uint32_t address, int* cycleCounter);
uint32_t DS7Load8(struct ARMCore* cpu, uint32_t address, int* cycleCounter);

void DS7Store32(struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter);
void DS7Store16(struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter);
void DS7Store8(struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter);

uint32_t DS7LoadMultiple(struct ARMCore*, uint32_t baseAddress, int mask, enum LSMDirection direction,
                         int* cycleCounter);
uint32_t DS7StoreMultiple(struct ARMCore*, uint32_t baseAddress, int mask, enum LSMDirection direction,
                          int* cycleCounter);

uint32_t DS9Load32(struct ARMCore* cpu, uint32_t address, int* cycleCounter);
uint32_t DS9Load16(struct ARMCore* cpu, uint32_t address, int* cycleCounter);
uint32_t DS9Load8(struct ARMCore* cpu, uint32_t address, int* cycleCounter);

void DS9Store32(struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter);
void DS9Store16(struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter);
void DS9Store8(struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter);

uint32_t DS9LoadMultiple(struct ARMCore*, uint32_t baseAddress, int mask, enum LSMDirection direction,
                         int* cycleCounter);
uint32_t DS9StoreMultiple(struct ARMCore*, uint32_t baseAddress, int mask, enum LSMDirection direction,
                          int* cycleCounter);

#endif
