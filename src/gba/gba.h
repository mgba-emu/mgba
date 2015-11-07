/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_H
#define GBA_H

#include "util/common.h"

#include "arm.h"
#include "debugger/debugger.h"

#include "gba/interface.h"
#include "gba/memory.h"
#include "gba/video.h"
#include "gba/audio.h"
#include "gba/sio.h"

extern const uint32_t GBA_ARM7TDMI_FREQUENCY;

enum GBAIRQ {
	IRQ_VBLANK = 0x0,
	IRQ_HBLANK = 0x1,
	IRQ_VCOUNTER = 0x2,
	IRQ_TIMER0 = 0x3,
	IRQ_TIMER1 = 0x4,
	IRQ_TIMER2 = 0x5,
	IRQ_TIMER3 = 0x6,
	IRQ_SIO = 0x7,
	IRQ_DMA0 = 0x8,
	IRQ_DMA1 = 0x9,
	IRQ_DMA2 = 0xA,
	IRQ_DMA3 = 0xB,
	IRQ_KEYPAD = 0xC,
	IRQ_GAMEPAK = 0xD
};

enum GBAComponent {
	GBA_COMPONENT_DEBUGGER,
	GBA_COMPONENT_CHEAT_DEVICE,
	GBA_COMPONENT_MAX
};

enum GBAIdleLoopOptimization {
	IDLE_LOOP_IGNORE = -1,
	IDLE_LOOP_REMOVE = 0,
	IDLE_LOOP_DETECT
};

enum {
	SP_BASE_SYSTEM = 0x03007F00,
	SP_BASE_IRQ = 0x03007FA0,
	SP_BASE_SUPERVISOR = 0x03007FE0
};

struct GBA;
struct GBAThread;
struct Patch;
struct VFile;

struct GBATimer {
	uint16_t reload;
	uint16_t oldReload;
	int32_t lastEvent;
	int32_t nextEvent;
	int32_t overflowInterval;
	unsigned prescaleBits : 4;
	unsigned countUp : 1;
	unsigned doIrq : 1;
	unsigned enable : 1;
};

struct GBA {
	struct ARMComponent d;

	struct ARMCore* cpu;
	struct GBAMemory memory;
	struct GBAVideo video;
	struct GBAAudio audio;
	struct GBASIO sio;

	struct GBASync* sync;

	struct ARMDebugger* debugger;

	uint32_t bus;
	int performingDMA;

	int timersEnabled;
	struct GBATimer timers[4];

	int springIRQ;
	uint32_t biosChecksum;
	int* keySource;
	struct GBARotationSource* rotationSource;
	struct GBALuminanceSource* luminanceSource;
	struct GBARTCSource* rtcSource;
	struct GBARumble* rumble;

	struct GBARRContext* rr;
	void* pristineRom;
	size_t pristineRomSize;
	size_t yankedRomSize;
	uint32_t romCrc32;
	struct VFile* romVf;
	struct VFile* biosVf;

	const char* activeFile;

	GBALogHandler logHandler;
	enum GBALogLevel logLevel;
	struct GBAAVStream* stream;
	struct GBAKeyCallback* keyCallback;
	struct GBAStopCallback* stopCallback;

	enum GBAIdleLoopOptimization idleOptimization;
	uint32_t idleLoop;
	uint32_t lastJump;
	bool haltPending;
	int idleDetectionStep;
	int idleDetectionFailures;
	int32_t cachedRegisters[16];
	bool taintedRegisters[16];

	bool realisticTiming;
	bool hardCrash;
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
void GBASkipBIOS(struct ARMCore* cpu);

void GBATimerUpdateRegister(struct GBA* gba, int timer);
void GBATimerWriteTMCNT_LO(struct GBA* gba, int timer, uint16_t value);
void GBATimerWriteTMCNT_HI(struct GBA* gba, int timer, uint16_t value);

void GBAWriteIE(struct GBA* gba, uint16_t value);
void GBAWriteIME(struct GBA* gba, uint16_t value);
void GBARaiseIRQ(struct GBA* gba, enum GBAIRQ irq);
void GBATestIRQ(struct ARMCore* cpu);
void GBAHalt(struct GBA* gba);
void GBAStop(struct GBA* gba);

void GBAAttachDebugger(struct GBA* gba, struct ARMDebugger* debugger);
void GBADetachDebugger(struct GBA* gba);

void GBASetBreakpoint(struct GBA* gba, struct ARMComponent* component, uint32_t address, enum ExecutionMode mode,
                      uint32_t* opcode);
void GBAClearBreakpoint(struct GBA* gba, uint32_t address, enum ExecutionMode mode, uint32_t opcode);

void GBALoadROM(struct GBA* gba, struct VFile* vf, struct VFile* sav, const char* fname);
void GBAYankROM(struct GBA* gba);
void GBAUnloadROM(struct GBA* gba);
void GBALoadBIOS(struct GBA* gba, struct VFile* vf);
void GBAApplyPatch(struct GBA* gba, struct Patch* patch);

bool GBAIsROM(struct VFile* vf);
bool GBAIsBIOS(struct VFile* vf);
void GBAGetGameCode(struct GBA* gba, char* out);
void GBAGetGameTitle(struct GBA* gba, char* out);

void GBAFrameStarted(struct GBA* gba);
void GBAFrameEnded(struct GBA* gba);

ATTRIBUTE_FORMAT(printf, 3, 4)
void GBALog(struct GBA* gba, enum GBALogLevel level, const char* format, ...);

ATTRIBUTE_FORMAT(printf, 3, 4)
void GBADebuggerLogShim(struct ARMDebugger* debugger, enum DebuggerLogLevel level, const char* format, ...);

#endif
