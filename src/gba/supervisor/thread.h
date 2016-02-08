/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_THREAD_H
#define GBA_THREAD_H

#include "util/common.h"

#include "core/directories.h"
#include "core/sync.h"
#include "core/thread.h"
#include "gba/gba.h"
#include "gba/input.h"
#include "gba/context/overrides.h"

#include "util/threading.h"

struct GBAThread;
struct mArguments;
struct GBACheatSet;
struct mCoreOptions;

typedef void (*GBAThreadCallback)(struct GBAThread* threadContext);
typedef bool (*ThreadStopCallback)(struct GBAThread* threadContext);

struct GBAThread {
	// Output
	enum mCoreThreadState state;
	struct GBA* gba;
	struct ARMCore* cpu;

	// Input
	struct GBAVideoRenderer* renderer;
	struct GBASIODriverSet sioDrivers;
	struct Debugger* debugger;
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	struct mDirectorySet dirs;
#endif
	struct VFile* rom;
	struct VFile* save;
	struct VFile* bios;
	struct VFile* patch;
	struct VFile* cheatsFile;
	const char* fname;
	const char* movie;
	int activeKeys;
	struct mAVStream* stream;
	struct Configuration* overrides;
	enum GBAIdleLoopOptimization idleOptimization;
	bool bootBios;

	bool hasOverride;
	struct GBACartridgeOverride override;

	// Run-time options
	int frameskip;
	float fpsTarget;
	size_t audioBuffers;
	bool skipBios;
	int volume;
	bool mute;

	// Threading state
	Thread thread;

	Mutex stateMutex;
	Condition stateCond;
	enum mCoreThreadState savedState;
	int interruptDepth;
	bool frameWasOn;

	GBALogHandler logHandler;
	int logLevel;
	GBAThreadCallback startCallback;
	GBAThreadCallback cleanCallback;
	GBAThreadCallback frameCallback;
	ThreadStopCallback stopCallback;
	void* userData;
	void (*run)(struct GBAThread*);

	struct mCoreSync sync;

	int rewindBufferSize;
	int rewindBufferCapacity;
	int rewindBufferInterval;
	int rewindBufferNext;
	struct GBASerializedState** rewindBuffer;
	int rewindBufferWriteOffset;
	uint8_t* rewindScreenBuffer;

	struct GBACheatDevice* cheats;
};

void GBAMapOptionsToContext(const struct mCoreOptions*, struct GBAThread*);
void GBAMapArgumentsToContext(const struct mArguments*, struct GBAThread*);

bool GBAThreadStart(struct GBAThread* threadContext);
bool GBAThreadHasStarted(struct GBAThread* threadContext);
bool GBAThreadHasExited(struct GBAThread* threadContext);
bool GBAThreadHasCrashed(struct GBAThread* threadContext);
void GBAThreadEnd(struct GBAThread* threadContext);
void GBAThreadReset(struct GBAThread* threadContext);
void GBAThreadJoin(struct GBAThread* threadContext);

bool GBAThreadIsActive(struct GBAThread* threadContext);
void GBAThreadInterrupt(struct GBAThread* threadContext);
void GBAThreadContinue(struct GBAThread* threadContext);

void GBARunOnThread(struct GBAThread* threadContext, void (*run)(struct GBAThread*));

void GBAThreadPause(struct GBAThread* threadContext);
void GBAThreadUnpause(struct GBAThread* threadContext);
bool GBAThreadIsPaused(struct GBAThread* threadContext);
void GBAThreadTogglePause(struct GBAThread* threadContext);
void GBAThreadPauseFromThread(struct GBAThread* threadContext);
struct GBAThread* GBAThreadGetContext(void);

void GBAThreadLoadROM(struct GBAThread* threadContext, const char* fname);
void GBAThreadReplaceROM(struct GBAThread* threadContext, const char* fname);

#ifdef USE_PNG
void GBAThreadTakeScreenshot(struct GBAThread* threadContext);
#endif

#endif
