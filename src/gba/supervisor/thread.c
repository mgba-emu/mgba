/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "thread.h"

#include "arm.h"
#include "core/config.h"
#include "gba/gba.h"
#include "gba/cheats.h"
#include "gba/serialize.h"
#include "gba/rr/mgm.h"
#include "gba/rr/vbm.h"

#include "debugger/debugger.h"

#include "util/patch.h"
#include "util/vfs.h"

#include "platform/commandline.h"

#include <signal.h>

static const float _defaultFPSTarget = 60.f;

#ifndef DISABLE_THREADING

static bool _reloadDirectories(struct GBAThread* threadContext) {
	mDirectorySetDetachBase(&threadContext->dirs);

	char basename[PATH_MAX];
	if (threadContext->fname) {
		char dirname[PATH_MAX];
		separatePath(threadContext->fname, dirname, basename, 0);
		mDirectorySetAttachBase(&threadContext->dirs, VDirOpen(dirname));
	} else {
		return false;
	}

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s.sav", basename);
	threadContext->save = threadContext->dirs.save->openFile(threadContext->dirs.save, path, O_CREAT | O_RDWR);

	if (!threadContext->patch) {
		snprintf(path, sizeof(path), "%s.ups", basename);
		threadContext->patch = threadContext->dirs.patch->openFile(threadContext->dirs.patch, path, O_RDONLY);
	}
	if (!threadContext->patch) {
		snprintf(path, sizeof(path), "%s.ips", basename);
		threadContext->patch = threadContext->dirs.patch->openFile(threadContext->dirs.patch, path, O_RDONLY);
	}
	if (!threadContext->patch) {
		snprintf(path, sizeof(path), "%s.bps", basename);
		threadContext->patch = threadContext->dirs.patch->openFile(threadContext->dirs.patch, path, O_RDONLY);
	}
	return true;
}

#ifdef USE_PTHREADS
static pthread_key_t _contextKey;
static pthread_once_t _contextOnce = PTHREAD_ONCE_INIT;

static void _createTLS(void) {
	pthread_key_create(&_contextKey, 0);
}
#elif _WIN32
static DWORD _contextKey;
static INIT_ONCE _contextOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK _createTLS(PINIT_ONCE once, PVOID param, PVOID* context) {
	UNUSED(once);
	UNUSED(param);
	UNUSED(context);
	_contextKey = TlsAlloc();
	return TRUE;
}
#endif

static void _changeState(struct GBAThread* threadContext, enum mCoreThreadState newState, bool broadcast) {
	MutexLock(&threadContext->stateMutex);
	threadContext->state = newState;
	if (broadcast) {
		ConditionWake(&threadContext->stateCond);
	}
	MutexUnlock(&threadContext->stateMutex);
}

static void _waitOnInterrupt(struct GBAThread* threadContext) {
	while (threadContext->state == THREAD_INTERRUPTED) {
		ConditionWait(&threadContext->stateCond, &threadContext->stateMutex);
	}
}

static void _waitUntilNotState(struct GBAThread* threadContext, enum mCoreThreadState oldState) {
	MutexLock(&threadContext->sync.videoFrameMutex);
	bool videoFrameWait = threadContext->sync.videoFrameWait;
	threadContext->sync.videoFrameWait = false;
	MutexUnlock(&threadContext->sync.videoFrameMutex);

	while (threadContext->state == oldState) {
		MutexUnlock(&threadContext->stateMutex);

		if (!MutexTryLock(&threadContext->sync.videoFrameMutex)) {
			ConditionWake(&threadContext->sync.videoFrameRequiredCond);
			MutexUnlock(&threadContext->sync.videoFrameMutex);
		}

		if (!MutexTryLock(&threadContext->sync.audioBufferMutex)) {
			ConditionWake(&threadContext->sync.audioRequiredCond);
			MutexUnlock(&threadContext->sync.audioBufferMutex);
		}

		MutexLock(&threadContext->stateMutex);
		ConditionWake(&threadContext->stateCond);
	}

	MutexLock(&threadContext->sync.videoFrameMutex);
	threadContext->sync.videoFrameWait = videoFrameWait;
	MutexUnlock(&threadContext->sync.videoFrameMutex);
}

static void _pauseThread(struct GBAThread* threadContext, bool onThread) {
	threadContext->state = THREAD_PAUSING;
	if (!onThread) {
		_waitUntilNotState(threadContext, THREAD_PAUSING);
	}
}

struct GBAThreadStop {
	struct mStopCallback d;
	struct GBAThread* p;
};

static void _stopCallback(struct mStopCallback* stop) {
	struct GBAThreadStop* callback = (struct GBAThreadStop*) stop;
	if (callback->p->stopCallback(callback->p)) {
		_changeState(callback->p, THREAD_EXITING, false);
	}
}

static THREAD_ENTRY _GBAThreadRun(void* context) {
#ifdef USE_PTHREADS
	pthread_once(&_contextOnce, _createTLS);
#elif _WIN32
	InitOnceExecuteOnce(&_contextOnce, _createTLS, NULL, 0);
#endif

	struct GBA gba;
	struct ARMCore cpu;
	struct Patch patch;
	struct GBACheatDevice cheatDevice;
	struct GBAThread* threadContext = context;
	struct mCPUComponent* components[GBA_COMPONENT_MAX] = { 0 };
	struct GBARRContext* movie = 0;
	int numComponents = GBA_COMPONENT_MAX;

	ThreadSetName("CPU Thread");

#if !defined(_WIN32) && defined(USE_PTHREADS)
	sigset_t signals;
	sigemptyset(&signals);
	pthread_sigmask(SIG_SETMASK, &signals, 0);
#endif

	GBACreate(&gba);
	ARMSetComponents(&cpu, &gba.d, numComponents, components);
	ARMInit(&cpu);
	gba.sync = &threadContext->sync;
	threadContext->gba = &gba;
	threadContext->cpu = &cpu;
	gba.logLevel = threadContext->logLevel;
	gba.logHandler = threadContext->logHandler;
	gba.stream = threadContext->stream;
	gba.video.frameskip = threadContext->frameskip;

	struct GBAThreadStop stop;
	if (threadContext->stopCallback) {
		stop.d.stop = _stopCallback;
		stop.p = threadContext;
		gba.stopCallback = &stop.d;
	}

	gba.idleOptimization = threadContext->idleOptimization;
#ifdef USE_PTHREADS
	pthread_setspecific(_contextKey, threadContext);
#elif _WIN32
	TlsSetValue(_contextKey, threadContext);
#endif

	if (threadContext->audioBuffers) {
		GBAAudioResizeBuffer(&gba.audio, threadContext->audioBuffers);
	} else {
		threadContext->audioBuffers = GBA_AUDIO_SAMPLES;
	}

	if (threadContext->renderer) {
		GBAVideoAssociateRenderer(&gba.video, threadContext->renderer);
	}

	if (threadContext->rom) {
		if (GBAIsMB(threadContext->rom)) {
			GBALoadMB(&gba, threadContext->rom, threadContext->fname);
		} else {
			GBALoadROM(&gba, threadContext->rom, threadContext->save, threadContext->fname);
		}

		struct GBACartridgeOverride override;
		const struct GBACartridge* cart = (const struct GBACartridge*) gba.pristineRom;
		memcpy(override.id, &cart->id, sizeof(override.id));
		if (GBAOverrideFind(threadContext->overrides, &override)) {
			GBAOverrideApply(&gba, &override);
		}
		if (threadContext->hasOverride) {
			GBAOverrideApply(&gba, &threadContext->override);
		}

		if (threadContext->patch && loadPatch(threadContext->patch, &patch)) {
			GBAApplyPatch(&gba, &patch);
		}
	}

	if (threadContext->bios && GBAIsBIOS(threadContext->bios)) {
		GBALoadBIOS(&gba, threadContext->bios);
	}

	if (threadContext->movie) {
		struct VDir* movieDir = VDirOpen(threadContext->movie);
		if (!movieDir) {
			movieDir = VDirOpenArchive(threadContext->movie);
		}
		if (movieDir) {
			struct GBAMGMContext* mgm = malloc(sizeof(*mgm));
			GBAMGMContextCreate(mgm);
			if (!GBAMGMSetStream(mgm, movieDir)) {
				mgm->d.destroy(&mgm->d);
			} else {
				movie = &mgm->d;
			}
		} else {
			struct VFile* movieFile = VFileOpen(threadContext->movie, O_RDONLY);
			if (movieFile) {
				struct GBAVBMContext* vbm = malloc(sizeof(*vbm));
				GBAVBMContextCreate(vbm);
				if (!GBAVBMSetStream(vbm, movieFile)) {
					vbm->d.destroy(&vbm->d);
				} else {
					movie = &vbm->d;
				}
			}
		}
	}

	ARMReset(&cpu);

	if (movie) {
		gba.rr = movie;
		movie->startPlaying(movie, false);
		GBARRInitPlay(&gba);
	}

	if (threadContext->skipBios && gba.pristineRom) {
		GBASkipBIOS(&gba);
	}

	if (!threadContext->cheats) {
		GBACheatDeviceCreate(&cheatDevice);
		threadContext->cheats = &cheatDevice;
	}
	if (threadContext->cheatsFile) {
		GBACheatParseFile(threadContext->cheats, threadContext->cheatsFile);
	}
	GBACheatAttachDevice(&gba, threadContext->cheats);

	if (threadContext->debugger) {
		threadContext->debugger->log = GBADebuggerLogShim;
		GBAAttachDebugger(&gba, threadContext->debugger);
		ARMDebuggerEnter(threadContext->debugger, DEBUGGER_ENTER_ATTACHED, 0);
	}

	GBASIOSetDriverSet(&gba.sio, &threadContext->sioDrivers);

	if (threadContext->volume == 0) {
		threadContext->volume = GBA_AUDIO_VOLUME_MAX;
	}
	if (threadContext->mute) {
		gba.audio.masterVolume = 0;
	} else {
		gba.audio.masterVolume = threadContext->volume;
	}

	gba.keySource = &threadContext->activeKeys;

	if (threadContext->startCallback) {
		threadContext->startCallback(threadContext);
	}

	_changeState(threadContext, THREAD_RUNNING, true);

	while (threadContext->state < THREAD_EXITING) {
		if (threadContext->debugger) {
			struct ARMDebugger* debugger = threadContext->debugger;
			ARMDebuggerRun(debugger);
			if (debugger->state == DEBUGGER_SHUTDOWN) {
				_changeState(threadContext, THREAD_EXITING, false);
			}
		} else {
			while (threadContext->state == THREAD_RUNNING) {
				ARMRunLoop(&cpu);
			}
		}

		int resetScheduled = 0;
		MutexLock(&threadContext->stateMutex);
		while (threadContext->state > THREAD_RUNNING && threadContext->state < THREAD_EXITING) {
			if (threadContext->state == THREAD_PAUSING) {
				threadContext->state = THREAD_PAUSED;
				ConditionWake(&threadContext->stateCond);
			}
			if (threadContext->state == THREAD_INTERRUPTING) {
				threadContext->state = THREAD_INTERRUPTED;
				ConditionWake(&threadContext->stateCond);
			}
			if (threadContext->state == THREAD_RUN_ON) {
				if (threadContext->run) {
					threadContext->run(threadContext);
				}
				threadContext->state = threadContext->savedState;
				ConditionWake(&threadContext->stateCond);
			}
			if (threadContext->state == THREAD_RESETING) {
				threadContext->state = THREAD_RUNNING;
				resetScheduled = 1;
			}
			while (threadContext->state == THREAD_PAUSED || threadContext->state == THREAD_INTERRUPTED) {
				ConditionWait(&threadContext->stateCond, &threadContext->stateMutex);
			}
		}
		MutexUnlock(&threadContext->stateMutex);
		if (resetScheduled) {
			ARMReset(&cpu);
			if (threadContext->skipBios && gba.pristineRom) {
				GBASkipBIOS(&gba);
			}
		}
	}

	while (threadContext->state < THREAD_SHUTDOWN) {
		_changeState(threadContext, THREAD_SHUTDOWN, false);
	}

	if (threadContext->cleanCallback) {
		threadContext->cleanCallback(threadContext);
	}

	threadContext->gba = 0;
	ARMDeinit(&cpu);
	GBADestroy(&gba);
	if (&cheatDevice == threadContext->cheats) {
		GBACheatDeviceDestroy(&cheatDevice);
	}

	if (movie) {
		movie->destroy(movie);
		free(movie);
	}

	threadContext->sync.videoFrameOn = false;
	ConditionWake(&threadContext->sync.videoFrameAvailableCond);
	ConditionWake(&threadContext->sync.audioRequiredCond);

	return 0;
}

void GBAMapOptionsToContext(const struct mCoreOptions* opts, struct GBAThread* threadContext) {
	if (opts->useBios) {
		threadContext->bios = VFileOpen(opts->bios, O_RDONLY);
	} else {
		threadContext->bios = 0;
	}
	threadContext->frameskip = opts->frameskip;
	threadContext->volume = opts->volume;
	threadContext->mute = opts->mute;
	threadContext->logLevel = opts->logLevel;
	if (opts->rewindEnable) {
		threadContext->rewindBufferCapacity = opts->rewindBufferCapacity;
		threadContext->rewindBufferInterval = opts->rewindBufferInterval;
	} else {
		threadContext->rewindBufferCapacity = 0;
	}
	threadContext->skipBios = opts->skipBios;
	threadContext->sync.audioWait = opts->audioSync;
	threadContext->sync.videoFrameWait = opts->videoSync;

	if (opts->fpsTarget) {
		threadContext->fpsTarget = opts->fpsTarget;
	}

	if (opts->audioBuffers) {
		threadContext->audioBuffers = opts->audioBuffers;
	}

	mDirectorySetMapOptions(&threadContext->dirs, opts);
}

void GBAMapArgumentsToContext(const struct mArguments* args, struct GBAThread* threadContext) {
	GBAThreadLoadROM(threadContext, args->fname);
	threadContext->fname = args->fname;
	threadContext->patch = VFileOpen(args->patch, O_RDONLY);
	threadContext->cheatsFile = VFileOpen(args->cheatsFile, O_RDONLY);
	threadContext->movie = args->movie;
}

bool GBAThreadStart(struct GBAThread* threadContext) {
	// TODO: error check
	threadContext->activeKeys = 0;
	threadContext->state = THREAD_INITIALIZED;
	threadContext->sync.videoFrameOn = true;

	threadContext->rewindBuffer = 0;
	threadContext->rewindScreenBuffer = 0;
	int newCapacity = threadContext->rewindBufferCapacity;
	int newInterval = threadContext->rewindBufferInterval;
	threadContext->rewindBufferCapacity = 0;
	threadContext->rewindBufferInterval = 0;
	GBARewindSettingsChanged(threadContext, newCapacity, newInterval);

	if (!threadContext->fpsTarget) {
		threadContext->fpsTarget = _defaultFPSTarget;
	}

	bool bootBios = threadContext->bootBios && threadContext->bios;

	if (threadContext->rom && (!GBAIsROM(threadContext->rom) || bootBios)) {
		threadContext->rom->close(threadContext->rom);
		threadContext->rom = 0;
	}

	if (!threadContext->rom && !bootBios) {
		threadContext->state = THREAD_SHUTDOWN;
		return false;
	}

	_reloadDirectories(threadContext);

	MutexInit(&threadContext->stateMutex);
	ConditionInit(&threadContext->stateCond);

	MutexInit(&threadContext->sync.videoFrameMutex);
	ConditionInit(&threadContext->sync.videoFrameAvailableCond);
	ConditionInit(&threadContext->sync.videoFrameRequiredCond);
	MutexInit(&threadContext->sync.audioBufferMutex);
	ConditionInit(&threadContext->sync.audioRequiredCond);

	threadContext->interruptDepth = 0;

#ifdef USE_PTHREADS
	sigset_t signals;
	sigemptyset(&signals);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTRAP);
	pthread_sigmask(SIG_BLOCK, &signals, 0);
#endif

	MutexLock(&threadContext->stateMutex);
	ThreadCreate(&threadContext->thread, _GBAThreadRun, threadContext);
	while (threadContext->state < THREAD_RUNNING) {
		ConditionWait(&threadContext->stateCond, &threadContext->stateMutex);
	}
	MutexUnlock(&threadContext->stateMutex);

	return true;
}

bool GBAThreadHasStarted(struct GBAThread* threadContext) {
	bool hasStarted;
	MutexLock(&threadContext->stateMutex);
	hasStarted = threadContext->state > THREAD_INITIALIZED;
	MutexUnlock(&threadContext->stateMutex);
	return hasStarted;
}

bool GBAThreadHasExited(struct GBAThread* threadContext) {
	bool hasExited;
	MutexLock(&threadContext->stateMutex);
	hasExited = threadContext->state > THREAD_EXITING;
	MutexUnlock(&threadContext->stateMutex);
	return hasExited;
}

bool GBAThreadHasCrashed(struct GBAThread* threadContext) {
	bool hasExited;
	MutexLock(&threadContext->stateMutex);
	hasExited = threadContext->state == THREAD_CRASHED;
	MutexUnlock(&threadContext->stateMutex);
	return hasExited;
}

void GBAThreadEnd(struct GBAThread* threadContext) {
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	threadContext->state = THREAD_EXITING;
	if (threadContext->gba) {
		threadContext->gba->cpu->halted = false;
	}
	ConditionWake(&threadContext->stateCond);
	MutexUnlock(&threadContext->stateMutex);
	MutexLock(&threadContext->sync.audioBufferMutex);
	threadContext->sync.audioWait = 0;
	ConditionWake(&threadContext->sync.audioRequiredCond);
	MutexUnlock(&threadContext->sync.audioBufferMutex);

	MutexLock(&threadContext->sync.videoFrameMutex);
	threadContext->sync.videoFrameWait = false;
	threadContext->sync.videoFrameOn = false;
	ConditionWake(&threadContext->sync.videoFrameRequiredCond);
	ConditionWake(&threadContext->sync.videoFrameAvailableCond);
	MutexUnlock(&threadContext->sync.videoFrameMutex);
}

void GBAThreadReset(struct GBAThread* threadContext) {
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	threadContext->state = THREAD_RESETING;
	ConditionWake(&threadContext->stateCond);
	MutexUnlock(&threadContext->stateMutex);
}

void GBAThreadJoin(struct GBAThread* threadContext) {
	ThreadJoin(threadContext->thread);

	MutexDeinit(&threadContext->stateMutex);
	ConditionDeinit(&threadContext->stateCond);

	MutexDeinit(&threadContext->sync.videoFrameMutex);
	ConditionWake(&threadContext->sync.videoFrameAvailableCond);
	ConditionDeinit(&threadContext->sync.videoFrameAvailableCond);
	ConditionWake(&threadContext->sync.videoFrameRequiredCond);
	ConditionDeinit(&threadContext->sync.videoFrameRequiredCond);

	ConditionWake(&threadContext->sync.audioRequiredCond);
	ConditionDeinit(&threadContext->sync.audioRequiredCond);
	MutexDeinit(&threadContext->sync.audioBufferMutex);

	int i;
	for (i = 0; i < threadContext->rewindBufferCapacity; ++i) {
		if (threadContext->rewindBuffer[i]) {
			GBADeallocateState(threadContext->rewindBuffer[i]);
		}
	}
	free(threadContext->rewindBuffer);
	free(threadContext->rewindScreenBuffer);

	if (threadContext->rom) {
		threadContext->rom->close(threadContext->rom);
		threadContext->rom = 0;
	}

	if (threadContext->save) {
		threadContext->save->close(threadContext->save);
		threadContext->save = 0;
	}

	if (threadContext->bios) {
		threadContext->bios->close(threadContext->bios);
		threadContext->bios = 0;
	}

	if (threadContext->patch) {
		threadContext->patch->close(threadContext->patch);
		threadContext->patch = 0;
	}
}

bool GBAThreadIsActive(struct GBAThread* threadContext) {
	return threadContext->state >= THREAD_RUNNING && threadContext->state < THREAD_EXITING;
}

void GBAThreadInterrupt(struct GBAThread* threadContext) {
	MutexLock(&threadContext->stateMutex);
	++threadContext->interruptDepth;
	if (threadContext->interruptDepth > 1 || !GBAThreadIsActive(threadContext)) {
		MutexUnlock(&threadContext->stateMutex);
		return;
	}
	threadContext->savedState = threadContext->state;
	_waitOnInterrupt(threadContext);
	threadContext->state = THREAD_INTERRUPTING;
	threadContext->gba->cpu->nextEvent = 0;
	ConditionWake(&threadContext->stateCond);
	_waitUntilNotState(threadContext, THREAD_INTERRUPTING);
	MutexUnlock(&threadContext->stateMutex);
}

void GBAThreadContinue(struct GBAThread* threadContext) {
	MutexLock(&threadContext->stateMutex);
	--threadContext->interruptDepth;
	if (threadContext->interruptDepth < 1 && GBAThreadIsActive(threadContext)) {
		threadContext->state = threadContext->savedState;
		ConditionWake(&threadContext->stateCond);
	}
	MutexUnlock(&threadContext->stateMutex);
}

void GBARunOnThread(struct GBAThread* threadContext, void (*run)(struct GBAThread*)) {
	MutexLock(&threadContext->stateMutex);
	threadContext->run = run;
	_waitOnInterrupt(threadContext);
	threadContext->savedState = threadContext->state;
	threadContext->state = THREAD_RUN_ON;
	threadContext->gba->cpu->nextEvent = 0;
	ConditionWake(&threadContext->stateCond);
	_waitUntilNotState(threadContext, THREAD_RUN_ON);
	MutexUnlock(&threadContext->stateMutex);
}

void GBAThreadPause(struct GBAThread* threadContext) {
	bool frameOn = threadContext->sync.videoFrameOn;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	if (threadContext->state == THREAD_RUNNING) {
		_pauseThread(threadContext, false);
		threadContext->frameWasOn = frameOn;
		frameOn = false;
	}
	MutexUnlock(&threadContext->stateMutex);

	mCoreSyncSetVideoSync(&threadContext->sync, frameOn);
}

void GBAThreadUnpause(struct GBAThread* threadContext) {
	bool frameOn = threadContext->sync.videoFrameOn;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	if (threadContext->state == THREAD_PAUSED || threadContext->state == THREAD_PAUSING) {
		threadContext->state = THREAD_RUNNING;
		ConditionWake(&threadContext->stateCond);
		frameOn = threadContext->frameWasOn;
	}
	MutexUnlock(&threadContext->stateMutex);

	mCoreSyncSetVideoSync(&threadContext->sync, frameOn);
}

bool GBAThreadIsPaused(struct GBAThread* threadContext) {
	bool isPaused;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	isPaused = threadContext->state == THREAD_PAUSED;
	MutexUnlock(&threadContext->stateMutex);
	return isPaused;
}

void GBAThreadTogglePause(struct GBAThread* threadContext) {
	bool frameOn = threadContext->sync.videoFrameOn;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	if (threadContext->state == THREAD_PAUSED || threadContext->state == THREAD_PAUSING) {
		threadContext->state = THREAD_RUNNING;
		ConditionWake(&threadContext->stateCond);
		frameOn = threadContext->frameWasOn;
	} else if (threadContext->state == THREAD_RUNNING) {
		_pauseThread(threadContext, false);
		threadContext->frameWasOn = frameOn;
		frameOn = false;
	}
	MutexUnlock(&threadContext->stateMutex);

	mCoreSyncSetVideoSync(&threadContext->sync, frameOn);
}

void GBAThreadPauseFromThread(struct GBAThread* threadContext) {
	bool frameOn = true;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	if (threadContext->state == THREAD_RUNNING) {
		_pauseThread(threadContext, true);
		frameOn = false;
	}
	MutexUnlock(&threadContext->stateMutex);

	mCoreSyncSetVideoSync(&threadContext->sync, frameOn);
}

void GBAThreadLoadROM(struct GBAThread* threadContext, const char* fname) {
	threadContext->rom = mDirectorySetOpenPath(&threadContext->dirs, fname, GBAIsROM);
}

void GBAThreadReplaceROM(struct GBAThread* threadContext, const char* fname) {
	GBAUnloadROM(threadContext->gba);

	if (threadContext->rom) {
		threadContext->rom->close(threadContext->rom);
		threadContext->rom = 0;
	}

	if (threadContext->save) {
		threadContext->save->close(threadContext->save);
		threadContext->save = 0;
	}

	if (threadContext->dirs.archive) {
		threadContext->dirs.archive->close(threadContext->dirs.archive);
		threadContext->dirs.archive = 0;
	}

	GBAThreadLoadROM(threadContext, fname);

	threadContext->fname = fname;
	_reloadDirectories(threadContext);

	GBARaiseIRQ(threadContext->gba, IRQ_GAMEPAK);
	GBALoadROM(threadContext->gba, threadContext->rom, threadContext->save, threadContext->fname);
}

#ifdef USE_PTHREADS
struct GBAThread* GBAThreadGetContext(void) {
	pthread_once(&_contextOnce, _createTLS);
	return pthread_getspecific(_contextKey);
}
#elif _WIN32
struct GBAThread* GBAThreadGetContext(void) {
	InitOnceExecuteOnce(&_contextOnce, _createTLS, NULL, 0);
	return TlsGetValue(_contextKey);
}
#endif

void GBAThreadTakeScreenshot(struct GBAThread* threadContext) {
	GBATakeScreenshot(threadContext->gba, threadContext->dirs.screenshot);
}

#else
struct GBAThread* GBAThreadGetContext(void) {
	return 0;
}
#endif
