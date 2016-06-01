/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_H
#define DS_H

#include "util/common.h"

#include "arm/arm.h"
#include "core/log.h"

#include "ds/video.h"

extern const uint32_t DS_ARM946ES_FREQUENCY;
extern const uint32_t DS_ARM7TDMI_FREQUENCY;

enum DSIRQ {
	DS_IRQ_VBLANK = 0x0,
	DS_IRQ_HBLANK = 0x1,
	DS_IRQ_VCOUNTER = 0x2,
	DS_IRQ_TIMER0 = 0x3,
	DS_IRQ_TIMER1 = 0x4,
	DS_IRQ_TIMER2 = 0x5,
	DS_IRQ_TIMER3 = 0x6,
	DS_IRQ_SIO = 0x7,
	DS_IRQ_DMA0 = 0x8,
	DS_IRQ_DMA1 = 0x9,
	DS_IRQ_DMA2 = 0xA,
	DS_IRQ_DMA3 = 0xB,
	DS_IRQ_KEYPAD = 0xC,
	DS_IRQ_SLOT2 = 0xD,
	DS_IRQ_IPC_SYNC = 0x10,
	DS_IRQ_IPC_EMPTY = 0x11,
	DS_IRQ_IPC_NOT_EMPTY = 0x12,
	DS_IRQ_SLOT1_TRANS = 0x13,
	DS_IRQ_SLOT1 = 0x14,
	DS_IRQ_GEOM_FIFO = 0x15,
	DS_IRQ_LID = 0x16,
	DS_IRQ_SPI = 0x17,
	DS_IRQ_WIFI = 0x18,
};

struct DS;
struct Patch;
struct VFile;
struct mDebugger;

mLOG_DECLARE_CATEGORY(DS);

struct DS {
	struct mCPUComponent d;

	struct ARMCore* arm7;
	struct ARMCore* arm9;
	struct DSVideo video;

	struct mCoreSync* sync;

	struct ARMDebugger* debugger;

	int springIRQ7;
	int springIRQ9;

	uint32_t biosChecksum;
	int* keySource;
	struct mRTCSource* rtcSource;
	struct mRumble* rumble;

	uint32_t romCrc32;
	struct VFile* romVf;

	struct mKeyCallback* keyCallback;
};

struct DSCartridge {
	char title[12];
	uint32_t id;

	uint16_t maker;
	uint8_t type;
	uint8_t encryptionSeed;
	uint8_t size;
	uint8_t reserved[8];
	uint8_t region;
	uint8_t version;
	uint8_t autostart;
	uint32_t arm9Offset;
	uint32_t arm9Entry;
	uint32_t arm9Base;
	uint32_t arm9Size;
	uint32_t arm7Offset;
	uint32_t arm7Entry;
	uint32_t arm7Base;
	uint32_t arm7Size;
	uint32_t fntOffset;
	uint32_t fntSize;
	uint32_t fatOffset;
	uint32_t fatSize;
	uint32_t arm9FileOverlayOffset;
	uint32_t arm9FileOverlaySize;
	uint32_t arm7FileOverlayOffset;
	uint32_t arm7FileOverlaySize;
	uint32_t busTiming;
	uint32_t busKEY1Timing;
	uint32_t iconOffset;
	uint16_t secureAreaCrc16;
	uint16_t secureAreaDelay;
	// TODO: Fill in more
	// And ROM data...
};

void DSCreate(struct DS* ds);
void DSDestroy(struct DS* ds);

void DSAttachDebugger(struct DS* ds, struct mDebugger* debugger);
void DSDetachDebugger(struct DS* ds);

bool DSLoadROM(struct DS* ds, struct VFile* vf);
void DSUnloadROM(struct DS* ds);
void DSApplyPatch(struct DS* ds, struct Patch* patch);

bool DSIsROM(struct VFile* vf);
void DSGetGameCode(struct DS* ds, char* out);
void DSGetGameTitle(struct DS* ds, char* out);

#endif
