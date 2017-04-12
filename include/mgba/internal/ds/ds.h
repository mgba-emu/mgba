/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_H
#define DS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba-util/circle-buffer.h>

#include <mgba/internal/ds/audio.h>
#include <mgba/internal/ds/gx.h>
#include <mgba/internal/ds/memory.h>
#include <mgba/internal/ds/timer.h>
#include <mgba/internal/ds/video.h>
#include <mgba/internal/ds/wifi.h>
#include <mgba/internal/gba/hardware.h>

extern const uint32_t DS_ARM946ES_FREQUENCY;
extern const uint32_t DS_ARM7TDMI_FREQUENCY;
extern const uint8_t DS_CHIP_ID[4];

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

enum {
	DS_CPU_BLOCK_DMA = 1,
	DS_CPU_BLOCK_GX = 2
};

struct ARMCore;
struct DS;
struct Patch;
struct VFile;
struct mDebugger;

mLOG_DECLARE_CATEGORY(DS);

struct DSCommon {
	struct DS* p;

	struct ARMCore* cpu;
	struct GBATimer timers[4];
	struct mTiming timing;
	int springIRQ;

	struct DSCoreMemory memory;
	struct DSCommon* ipc;

	struct CircleBuffer fifo;
};

struct mCoreCallbacks;
struct DS {
	struct mCPUComponent d;

	struct DSCommon ds7;
	struct DSCommon ds9;
	struct DSMemory memory;
	struct DSVideo video;
	struct DSAudio audio;
	struct DSGX gx;
	struct DSWifi wifi;
	struct GBARTC rtc;

	struct mCoreSync* sync;
	struct mTimingEvent slice;
	struct ARMCore* activeCpu;
	uint32_t sliceStart;
	int32_t cycleDrift;

	struct ARMDebugger* debugger;

	int cpuBlocked;
	bool earlyExit;

	uint32_t bios7Checksum;
	uint32_t bios9Checksum;
	int* keySource;
	int* cursorSourceX;
	int* cursorSourceY;
	bool* touchSource;
	struct mRTCSource* rtcSource;
	struct mRumble* rumble;

	struct VFile* romVf;
	struct VFile* bios7Vf;
	struct VFile* bios9Vf;
	struct VFile* firmwareVf;

	struct mAVStream* stream;
	struct mKeyCallback* keyCallback;
	struct mCoreCallbacksList coreCallbacks;

	struct mTimingEvent divEvent;
	struct mTimingEvent sqrtEvent;

	bool isHomebrew;
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
	uint32_t arm9Autoload;
	uint32_t arm7Autoload;
	uint8_t secureAreaDisable[8];
	uint32_t usedSize;
	uint32_t romHeaderSize;
	uint8_t reserved2[56];
	uint8_t logo[156];
	uint16_t logoCrc16;
	uint16_t headerCrc16;
	// TODO: Fill in more
	// And ROM data...
};

void DSCreate(struct DS* ds);
void DSDestroy(struct DS* ds);

void DSRunLoop(struct DS* ds);
void DS7Step(struct DS* ds);
void DS9Step(struct DS* ds);

void DSAttachDebugger(struct DS* ds, struct mDebugger* debugger);
void DSDetachDebugger(struct DS* ds);

bool DSLoadROM(struct DS* ds, struct VFile* vf);
bool DSLoadSave(struct DS* ds, struct VFile* vf);
void DSUnloadROM(struct DS* ds);
void DSApplyPatch(struct DS* ds, struct Patch* patch);

bool DSIsBIOS7(struct VFile* vf);
bool DSIsBIOS9(struct VFile* vf);
bool DSLoadBIOS(struct DS* ds, struct VFile* vf);

bool DSIsFirmware(struct VFile* vf);
bool DSLoadFirmware(struct DS* ds, struct VFile* vf);

bool DSIsROM(struct VFile* vf);
void DSGetGameCode(struct DS* ds, char* out);
void DSGetGameTitle(struct DS* ds, char* out);

void DSWriteIME(struct ARMCore* cpu, uint16_t* io, uint16_t value);
void DSWriteIE(struct ARMCore* cpu, uint16_t* io, uint32_t value);
void DSRaiseIRQ(struct ARMCore* cpu, uint16_t* io, enum DSIRQ irq);

void DSFrameStarted(struct DS* ds);
void DSFrameEnded(struct DS* ds);

uint16_t DSWriteRTC(struct DS* ds, DSRegisterRTC value);

CXX_GUARD_END

#endif
