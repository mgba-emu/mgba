/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_H
#define GB_H

#include "util/common.h"

#include "lr35902/lr35902.h"

#include "gb/memory.h"

extern const uint32_t DMG_LR35902_FREQUENCY;
extern const uint32_t CGB_LR35902_FREQUENCY;
extern const uint32_t SGB_LR35902_FREQUENCY;

// TODO: Prefix GBAIRQ
enum GBIRQ {
	GB_IRQ_VBLANK = 0x0,
	GB_IRQ_LCDSTAT = 0x1,
	GB_IRQ_TIMER = 0x2,
	GB_IRQ_SIO = 0x3,
	GB_IRQ_KEYPAD = 0x4,
};

struct GB {
	struct LR35902Component d;

	struct LR35902Core* cpu;
	struct GBMemory memory;

	int* keySource;

	void* pristineRom;
	size_t pristineRomSize;
	size_t yankedRomSize;
	uint32_t romCrc32;
	struct VFile* romVf;

	const char* activeFile;
};

void GBCreate(struct GB* gb);
void GBDestroy(struct GB* gb);

void GBReset(struct LR35902Core* cpu);

void GBWriteIE(struct GB* gb, uint8_t value);
void GBRaiseIRQ(struct GB* gb, enum GBIRQ irq);
void GBTestIRQ(struct LR35902Core* cpu);
void GBHalt(struct GB* gb);
void GBStop(struct GB* gb);

struct VFile;
bool GBLoadROM(struct GB* gb, struct VFile* vf, struct VFile* sav, const char* fname);
void GBYankROM(struct GB* gb);
void GBUnloadROM(struct GB* gb);

struct Patch;
void GBApplyPatch(struct GB* gb, struct Patch* patch);

bool GBIsROM(struct VFile* vf);

void GBFrameStarted(struct GB* gb);
void GBFrameEnded(struct GB* gb);

#endif
