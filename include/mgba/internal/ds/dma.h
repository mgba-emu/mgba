/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_DMA_H
#define DS_DMA_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gba/dma.h>

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

DECL_BITS(GBADMARegister, Timing9, 11, 3);

struct DS;
struct DSCommon;
void DSDMAInit(struct DS* ds);
void DSDMAReset(struct DSCommon* dscore);

uint32_t DSDMAWriteSAD(struct DSCommon* dscore, int dma, uint32_t address);
uint32_t DSDMAWriteDAD(struct DSCommon* dscore, int dma, uint32_t address);
void DS7DMAWriteCNT(struct DSCommon* dscore, int dma, uint32_t value);
void DS9DMAWriteCNT(struct DSCommon* dscore, int dma, uint32_t value);

struct DSDMA;
void DSDMASchedule(struct DSCommon* dscore, int number, struct GBADMA* info);
void DSDMAUpdate(struct DSCommon* dscore);

CXX_GUARD_END

#endif
