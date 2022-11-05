/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_H
#define GBA_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>

#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/video.h>
#include <mgba/internal/gba/audio.h>
#include <mgba/internal/gba/sio.h>
#include <mgba/internal/gba/timer.h>

#define GBA_ARM7TDMI_FREQUENCY 0x1000000U

enum GBAIRQ {
	GBA_IRQ_VBLANK = 0x0,
	GBA_IRQ_HBLANK = 0x1,
	GBA_IRQ_VCOUNTER = 0x2,
	GBA_IRQ_TIMER0 = 0x3,
	GBA_IRQ_TIMER1 = 0x4,
	GBA_IRQ_TIMER2 = 0x5,
	GBA_IRQ_TIMER3 = 0x6,
	GBA_IRQ_SIO = 0x7,
	GBA_IRQ_DMA0 = 0x8,
	GBA_IRQ_DMA1 = 0x9,
	GBA_IRQ_DMA2 = 0xA,
	GBA_IRQ_DMA3 = 0xB,
	GBA_IRQ_KEYPAD = 0xC,
	GBA_IRQ_GAMEPAK = 0xD
};

enum GBAIdleLoopOptimization {
	IDLE_LOOP_IGNORE = -1,
	IDLE_LOOP_REMOVE = 0,
	IDLE_LOOP_DETECT
};

enum {
	GBA_SP_BASE_SYSTEM = 0x03007F00,
	GBA_SP_BASE_IRQ = 0x03007FA0,
	GBA_SP_BASE_SUPERVISOR = 0x03007FE0
};

struct ARMCore;
struct GBA;
struct Patch;
struct VFile;

mLOG_DECLARE_CATEGORY(GBA);
mLOG_DECLARE_CATEGORY(GBA_DEBUG);

DECL_BITFIELD(GBADebugFlags, uint16_t);
DECL_BITS(GBADebugFlags, Level, 0, 3);
DECL_BIT(GBADebugFlags, Send, 8);

struct GBA {
	struct mCPUComponent d;

	struct ARMCore* cpu;
	struct GBAMemory memory;
	struct GBAVideo video;
	struct GBAAudio audio;
	struct GBASIO sio;

	struct mCoreSync* sync;
	struct mTiming timing;

	struct ARMDebugger* debugger;

	uint32_t bus;
	int performingDMA;

	struct GBATimer timers[4];

	int springIRQ;
	struct mTimingEvent irqEvent;

	uint32_t biosChecksum;
	uint16_t keysActive;
	uint16_t keysLast;
	struct mRotationSource* rotationSource;
	struct GBALuminanceSource* luminanceSource;
	struct mRTCSource* rtcSource;
	struct mRumble* rumble;

	bool isPristine;
	size_t pristineRomSize;
	size_t yankedRomSize;
	uint32_t romCrc32;
	struct VFile* romVf;
	struct VFile* biosVf;
	struct VFile* mbVf;

	struct mAVStream* stream;
	struct mKeyCallback* keyCallback;
	struct mCoreCallbacksList coreCallbacks;

	enum GBAIdleLoopOptimization idleOptimization;
	uint32_t idleLoop;
	uint32_t lastJump;
	bool haltPending;
	bool cpuBlocked;
	bool earlyExit;
	uint32_t dmaPC;
	uint32_t biosStall;

	int idleDetectionStep;
	int idleDetectionFailures;
	int32_t cachedRegisters[16];
	bool taintedRegisters[16];

	bool vbaBugCompat;
	bool hardCrash;
	bool allowOpposingDirections;

	bool debug;
	char debugString[0x100];
	GBADebugFlags debugFlags;
};

struct GBACartridge {
	uint32_t entry;
	uint8_t logo[156];
	char title[12];
	uint32_t id;
	uint16_t maker;
	uint8_t type;
	uint8_t unit;
	uint8_t device;
	uint8_t reserved[7];
	uint8_t version;
	uint8_t checksum;
	// And ROM data...
};

void GBACreate(struct GBA* gba);
void GBADestroy(struct GBA* gba);

void GBAReset(struct ARMCore* cpu);
void GBASkipBIOS(struct GBA* gba);

void GBARaiseIRQ(struct GBA* gba, enum GBAIRQ irq, uint32_t cyclesLate);
void GBATestIRQ(struct GBA* gba, uint32_t cyclesLate);
void GBAHalt(struct GBA* gba);
void GBAStop(struct GBA* gba);
void GBADebug(struct GBA* gba, uint16_t value);

#ifdef USE_ELF
struct ELF;

bool GBAVerifyELFEntry(struct ELF* elf, uint32_t target);
#endif

#ifdef USE_DEBUGGERS
struct mDebugger;
void GBAAttachDebugger(struct GBA* gba, struct mDebugger* debugger);
void GBADetachDebugger(struct GBA* gba);
#endif

void GBASetBreakpoint(struct GBA* gba, struct mCPUComponent* component, uint32_t address, enum ExecutionMode mode,
                      uint32_t* opcode);
void GBAClearBreakpoint(struct GBA* gba, uint32_t address, enum ExecutionMode mode, uint32_t opcode);

bool GBALoadROM(struct GBA* gba, struct VFile* vf);
bool GBALoadSave(struct GBA* gba, struct VFile* sav);
void GBAYankROM(struct GBA* gba);
void GBAUnloadROM(struct GBA* gba);
void GBALoadBIOS(struct GBA* gba, struct VFile* vf);
void GBAApplyPatch(struct GBA* gba, struct Patch* patch);

bool GBALoadMB(struct GBA* gba, struct VFile* vf);
void GBAUnloadMB(struct GBA* gba);

bool GBALoadNull(struct GBA* gba);

void GBAGetGameCode(const struct GBA* gba, char* out);
void GBAGetGameTitle(const struct GBA* gba, char* out);

void GBATestKeypadIRQ(struct GBA* gba);

void GBAFrameStarted(struct GBA* gba);
void GBAFrameEnded(struct GBA* gba);

CXX_GUARD_END

#endif
