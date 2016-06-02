/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_MEMORY_H
#define DS_MEMORY_H

#include "util/common.h"

#include "arm/arm.h"
#include "core/log.h"

enum DSMemoryRegion {
	DS7_REGION_BIOS = 0x0,
	DS9_REGION_ITCM = 0x0,
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
	DS_BASE_WORKING_RAM = 0x03000000,
	DS_BASE_IO = 0x04000000,
	DS9_BASE_PALETTE_RAM = 0x05000000,
	DS_BASE_VRAM = 0x06000000,
	DS9_BASE_OAM = 0x07000000,
	DS_BASE_SLOT2 = 0x08000000,
	DS_BASE_SLOT2_EX = 0x09000000,
	DS9_BASE_BIOS = 0xFFFF0000,
};

enum {
	DS7_SIZE_BIOS = 0x00004000,
	DS9_SIZE_BIOS = 0x00008000,
	DS_SIZE_RAM = 0x00400000,
	DS_SIZE_WORKING_RAM = 0x00008000,
	DS9_SIZE_PALETTE_RAM = 0x00000800,
	DS9_SIZE_OAM = 0x00000800,
	DS_SIZE_SLOT2 = 0x02000000,
	DS_SIZE_SLOT2_SRAM = 0x00010000,
};

enum {
	DS_OFFSET_MASK = 0x00FFFFFF,
	DS_BASE_OFFSET = 24
};

enum DSDMAControl {
	DS_DMA_INCREMENT = 0,
	DS_DMA_DECREMENT = 1,
	DS_DMA_FIXED = 2,
	DS_DMA_INCREMENT_RELOAD = 3
};

enum DSDMATiming {
	DS_DMA_TIMING_NOW = 0,
	DS_DMA_TIMING_VBLANK = 1,
	DS_DMA_TIMING_HBLANK = 2,
	DS7_DMA_TIMING_SLOT1 = 2,
	DS_DMA_TIMING_DISPLAY_START = 3,
	DS7_DMA_TIMING_CUSTOM = 3,
	DS_DMA_TIMING_MEMORY_DISPLAY = 4,
	DS9_DMA_TIMING_SLOT1 = 5,
	DS_DMA_TIMING_SLOT2 = 6,
	DS_DMA_TIMING_GEOM_FIFO = 7,
};

mLOG_DECLARE_CATEGORY(DS_MEM);

DECL_BITFIELD(DSDMARegister, uint16_t);
DECL_BITS(DSDMARegister, DestControl, 5, 2);
DECL_BITS(DSDMARegister, SrcControl, 7, 2);
DECL_BIT(DSDMARegister, Repeat, 9);
DECL_BIT(DSDMARegister, Width, 10);
DECL_BITS(DSDMARegister, Timing7, 12, 2);
DECL_BITS(DSDMARegister, Timing9, 11, 3);
DECL_BIT(DSDMARegister, DoIRQ, 14);
DECL_BIT(DSDMARegister, Enable, 15);

struct DSDMA {
	DSDMARegister reg;

	uint32_t source;
	uint32_t dest;
	int32_t count;
	uint32_t nextSource;
	uint32_t nextDest;
	int32_t nextCount;
	int32_t nextEvent;
};

struct DSMemory {
	uint32_t* bios7;
	uint32_t* bios9;
	uint32_t* ram;
	uint32_t* wram;
	uint32_t* rom;

	size_t romSize;

	char waitstatesSeq32[256];
	char waitstatesSeq16[256];
	char waitstatesNonseq32[256];
	char waitstatesNonseq16[256];
	char waitstatesPrefetchSeq32[16];
	char waitstatesPrefetchSeq16[16];
	char waitstatesPrefetchNonseq32[16];
	char waitstatesPrefetchNonseq16[16];
	int activeRegion7;
	int activeRegion9;

	struct DSDMA dma7[4];
	struct DSDMA dma9[4];
	int activeDMA7;
	int activeDMA9;
	int32_t nextDMA;
	int32_t eventDiff;
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
