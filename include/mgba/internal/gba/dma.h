/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_DMA_H
#define GBA_DMA_H

#include <mgba-util/common.h>

CXX_GUARD_START

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
void GBADMAUpdate(struct GBA* gba);

CXX_GUARD_END

#endif
