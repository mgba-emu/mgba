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
#include "gb/timer.h"
#include "gb/video.h"

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

enum GBIRQVector {
	GB_VECTOR_VBLANK = 0x40,
	GB_VECTOR_LCDSTAT = 0x48,
	GB_VECTOR_TIMER = 0x50,
	GB_VECTOR_SIO = 0x58,
	GB_VECTOR_KEYPAD = 0x60,
};

struct GB {
	struct LR35902Component d;

	struct LR35902Core* cpu;
	struct GBMemory memory;
	struct GBVideo video;
	struct GBTimer timer;

	int* keySource;

	void* pristineRom;
	size_t pristineRomSize;
	size_t yankedRomSize;
	uint32_t romCrc32;
	struct VFile* romVf;

	const char* activeFile;
};

struct GBCartridge {
	uint8_t entry[4];
	uint8_t logo[48];
	union {
		char titleLong[16];
		struct {
			char titleShort[11];
			char maker[4];
			uint8_t cgb;
		};
	};
	char licensee[2];
	uint8_t sgb;
	uint8_t type;
	uint8_t romSize;
	uint8_t ramSize;
	uint8_t region;
	uint8_t oldLicensee;
	uint8_t version;
	uint8_t headerChecksum;
	uint16_t globalChecksum;
	// And ROM data...
};

void GBCreate(struct GB* gb);
void GBDestroy(struct GB* gb);

void GBReset(struct LR35902Core* cpu);

void GBUpdateIRQs(struct GB* gb);
void GBHalt(struct LR35902Core* cpu);
void GBStop(struct LR35902Core* cpu);

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
