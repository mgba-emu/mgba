/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_DMA_H
#define GBA_DMA_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

enum GBADMAControl {
	GBA_DMA_INCREMENT = 0,
	GBA_DMA_DECREMENT = 1,
	GBA_DMA_FIXED = 2,
	GBA_DMA_INCREMENT_RELOAD = 3
};

enum GBADMATiming {
	GBA_DMA_TIMING_NOW = 0,
	GBA_DMA_TIMING_VBLANK = 1,
	GBA_DMA_TIMING_HBLANK = 2,
	GBA_DMA_TIMING_CUSTOM = 3
};

DECL_BITFIELD(GBADMARegister, uint16_t);
DECL_BITS(GBADMARegister, DestControl, 5, 2);
DECL_BITS(GBADMARegister, SrcControl, 7, 2);
DECL_BIT(GBADMARegister, Repeat, 9);
DECL_BIT(GBADMARegister, Width, 10);
DECL_BIT(GBADMARegister, DRQ, 11);
DECL_BITS(GBADMARegister, Timing, 12, 2);
DECL_BIT(GBADMARegister, DoIRQ, 14);
DECL_BIT(GBADMARegister, Enable, 15);

mLOG_DECLARE_CATEGORY(GBA_DMA);

struct GBADMA {
	GBADMARegister reg;

	uint32_t source;
	uint32_t dest;
	int32_t count;
	uint32_t nextSource;
	uint32_t nextDest;
	int32_t nextCount;
	uint32_t when;
};

struct GBA;
void GBADMAInit(struct GBA* gba);
void GBADMAReset(struct GBA* gba);

uint32_t GBADMAWriteSAD(struct GBA* gba, int dma, uint32_t address);
uint32_t GBADMAWriteDAD(struct GBA* gba, int dma, uint32_t address);
void GBADMAWriteCNT_LO(struct GBA* gba, int dma, uint16_t count);
uint16_t GBADMAWriteCNT_HI(struct GBA* gba, int dma, uint16_t control);

struct GBADMA;
void GBADMASchedule(struct GBA* gba, int number, struct GBADMA* info);
void GBADMARunHblank(struct GBA* gba, int32_t cycles);
void GBADMARunVblank(struct GBA* gba, int32_t cycles);
void GBADMARunDisplayStart(struct GBA* gba, int32_t cycles);
void GBADMAUpdate(struct GBA* gba);

CXX_GUARD_END

#endif
