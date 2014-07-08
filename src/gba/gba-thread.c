#include "gba-thread.h"

#include "arm.h"
#include "gba.h"
#include "gba-serialize.h"

#include "debugger/debugger.h"

#include "util/patch.h"

#include <signal.h>

#ifdef USE_PTHREADS
static pthread_key_t _contextKey;
static pthread_once_t _contextOnce = PTHREAD_ONCE_INIT;

static void _createTLS(void) {
	pthread_key_create(&_contextKey, 0);
}
#else
static DWORD _contextKey;
static INIT_ONCE _contextOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK _createTLS(PINIT_ONCE once, PVOID param, PVOID* context) {
	(void) (once);
	(void) (param);
	(void) (context);
	_contextKey = TlsAlloc();
	return TRUE;
}
#endif

static void _changeState(struct GBAThread* threadContext, enum ThreadState newState, int broadcast) {
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

static THREAD_ENTRY _GBAThreadRun(void* context) {
#ifdef USE_PTHREADS
	pthread_once(&_contextOnce, _createTLS);
#else
	InitOnceExecuteOnce(&_contextOnce, _createTLS, NULL, 0);
#endif

	struct GBA gba;
	struct ARMCore cpu;
	struct Patch patch;
	struct GBAThread* threadContext = context;
	struct ARMComponent* components[1] = {};
	int numComponents = 0;
	char* savedata = 0;

	if (threadContext->debugger) {
		components[numComponents] = &threadContext->debugger->d;
		++numComponents;
	}

#if !defined(_WIN32) && defined(USE_PTHREADS)
	sigset_t signals;
	sigemptyset(&signals);
	pthread_sigmask(SIG_SETMASK, &signals, 0);
#endif

	gba.logHandler = threadContext->logHandler;
	GBACreate(&gba);
	ARMSetComponents(&cpu, &gba.d, numComponents, components);
	ARMInit(&cpu);
	ARMReset(&cpu);
	threadContext->gba = &gba;
	gba.sync = &threadContext->sync;
	gba.logLevel = threadContext->logLevel;
#ifdef USE_PTHREADS
	pthread_setspecific(_contextKey, threadContext);
#else
	TlsSetValue(_contextKey, threadContext);
#endif
	if (threadContext->renderer) {
		GBAVideoAssociateRenderer(&gba.video, threadContext->renderer);
	}

	if (threadContext->fd >= 0) {
		if (threadContext->fname) {
			char* dotPoint = strrchr(threadContext->fname, '.');
			if (dotPoint > strrchr(threadContext->fname, '/') && dotPoint[1] && dotPoint[2] && dotPoint[3]) {
				savedata = strdup(threadContext->fname);
				dotPoint = strrchr(savedata, '.');
				dotPoint[1] = 's';
				dotPoint[2] = 'a';
				dotPoint[3] = 'v';
				dotPoint[4] = '\0';
			} else if (dotPoint) {
				savedata = malloc((dotPoint - threadContext->fname + 5) * sizeof(char));
				strncpy(savedata, threadContext->fname, dotPoint - threadContext->fname + 1);
				strcat(savedata, "sav");
			} else {
				savedata = malloc(strlen(threadContext->fname + 5));
				strcpy(savedata, threadContext->fname);
				strcat(savedata, "sav");
			}
		}
		gba.savefile = savedata;
		GBALoadROM(&gba, threadContext->fd, threadContext->fname);
		if (threadContext->biosFd >= 0) {
			GBALoadBIOS(&gba, threadContext->biosFd);
		}

		if (threadContext->patchFd >= 0 && loadPatch(threadContext->patchFd, &patch)) {
			GBAApplyPatch(&gba, &patch);
		}
	}

	if (threadContext->debugger) {
		GBAAttachDebugger(&gba, threadContext->debugger);
		ARMDebuggerEnter(threadContext->debugger, DEBUGGER_ENTER_ATTACHED);
	}

	GBASIOSetDriverSet(&gba.sio, &threadContext->sioDrivers);

	gba.keySource = &threadContext->activeKeys;

	if (threadContext->startCallback) {
		threadContext->startCallback(threadContext);
	}

	_changeState(threadContext, THREAD_RUNNING, 1);

	while (threadContext->state < THREAD_EXITING) {
		if (threadContext->debugger) {
			struct ARMDebugger* debugger = threadContext->debugger;
			ARMDebuggerRun(debugger);
			if (debugger->state == DEBUGGER_SHUTDOWN) {
				_changeState(threadContext, THREAD_EXITING, 0);
			}
		} else {
			while (threadContext->state == THREAD_RUNNING) {
				ARMRun(&cpu);
			}
		}
		MutexLock(&threadContext->stateMutex);
		if (threadContext->state == THREAD_INTERRUPTED) {
			threadContext->state = THREAD_PAUSED;
			ConditionWake(&threadContext->stateCond);
		}
		while (threadContext->state == THREAD_PAUSED) {
			ConditionWait(&threadContext->stateCond, &threadContext->stateMutex);
		}
		MutexUnlock(&threadContext->stateMutex);
	}

	while (threadContext->state != THREAD_SHUTDOWN) {
		_changeState(threadContext, THREAD_SHUTDOWN, 0);
	}

	if (threadContext->cleanCallback) {
		threadContext->cleanCallback(threadContext);
	}

	threadContext->gba = 0;
	ARMDeinit(&cpu);
	GBADestroy(&gba);

	ConditionWake(&threadContext->sync.videoFrameAvailableCond);
	ConditionWake(&threadContext->sync.audioRequiredCond);
	free(savedata);

	return 0;
}

void GBAMapOptionsToContext(struct StartupOptions* opts, struct GBAThread* threadContext) {
	threadContext->fd = opts->fd;
	threadContext->fname = opts->fname;
	threadContext->biosFd = opts->biosFd;
	threadContext->patchFd = opts->patchFd;
	threadContext->frameskip = opts->frameskip;
	threadContext->logLevel = opts->logLevel;
	threadContext->rewindBufferCapacity = opts->rewindBufferCapacity;
	threadContext->rewindBufferInterval = opts->rewindBufferInterval;
}

int GBAThreadStart(struct GBAThread* threadContext) {
	// TODO: error check
	threadContext->activeKeys = 0;
	threadContext->state = THREAD_INITIALIZED;
	threadContext->sync.videoFrameOn = 1;
	threadContext->sync.videoFrameSkip = 0;

	threadContext->rewindBufferNext = threadContext->rewindBufferInterval;
	threadContext->rewindBufferSize = 0;
	if (threadContext->rewindBufferCapacity) {
		threadContext->rewindBuffer = calloc(threadContext->rewindBufferCapacity, sizeof(void*));
	} else {
		threadContext->rewindBuffer = 0;
	}

	MutexInit(&threadContext->stateMutex);
	ConditionInit(&threadContext->stateCond);

	MutexInit(&threadContext->sync.videoFrameMutex);
	ConditionInit(&threadContext->sync.videoFrameAvailableCond);
	ConditionInit(&threadContext->sync.videoFrameRequiredCond);
	MutexInit(&threadContext->sync.audioBufferMutex);
	ConditionInit(&threadContext->sync.audioRequiredCond);

#ifndef _WIN32
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

	return 0;
}

int GBAThreadHasStarted(struct GBAThread* threadContext) {
	int hasStarted;
	MutexLock(&threadContext->stateMutex);
	hasStarted = threadContext->state > THREAD_INITIALIZED;
	MutexUnlock(&threadContext->stateMutex);
	return hasStarted;
}

void GBAThreadEnd(struct GBAThread* threadContext) {
	MutexLock(&threadContext->stateMutex);
	if (threadContext->debugger && threadContext->debugger->state == DEBUGGER_RUNNING) {
		threadContext->debugger->state = DEBUGGER_EXITING;
	}
	threadContext->state = THREAD_EXITING;
	MutexUnlock(&threadContext->stateMutex);
	MutexLock(&threadContext->sync.audioBufferMutex);
	threadContext->sync.audioWait = 0;
	ConditionWake(&threadContext->sync.audioRequiredCond);
	MutexUnlock(&threadContext->sync.audioBufferMutex);
}

void GBAThreadJoin(struct GBAThread* threadContext) {
	MutexLock(&threadContext->sync.videoFrameMutex);
	threadContext->sync.videoFrameWait = 0;
	ConditionWake(&threadContext->sync.videoFrameRequiredCond);
	MutexUnlock(&threadContext->sync.videoFrameMutex);

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
}

void GBAThreadTryPause(struct GBAThread* threadContext) {
	MutexLock(&threadContext->stateMutex);
	threadContext->savedState = threadContext->state;
	threadContext->state = THREAD_INTERRUPTED;
	_waitOnInterrupt(threadContext);
	threadContext->state = THREAD_PAUSED;
	if (threadContext->debugger && threadContext->debugger->state == DEBUGGER_RUNNING) {
		threadContext->debugger->state = DEBUGGER_EXITING;
	}
	MutexUnlock(&threadContext->stateMutex);
}

void GBAThreadContinue(struct GBAThread* threadContext) {
	_changeState(threadContext, threadContext->savedState, 1);
}

void GBAThreadPause(struct GBAThread* threadContext) {
	int frameOn = 1;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	if (threadContext->state == THREAD_RUNNING) {
		if (threadContext->debugger && threadContext->debugger->state == DEBUGGER_RUNNING) {
			threadContext->debugger->state = DEBUGGER_EXITING;
		}
		threadContext->state = THREAD_PAUSED;
		frameOn = 0;
	}
	MutexUnlock(&threadContext->stateMutex);
	MutexLock(&threadContext->sync.videoFrameMutex);
	if (frameOn != threadContext->sync.videoFrameOn) {
		threadContext->sync.videoFrameOn = frameOn;
		ConditionWake(&threadContext->sync.videoFrameAvailableCond);
	}
	MutexUnlock(&threadContext->sync.videoFrameMutex);
}

void GBAThreadUnpause(struct GBAThread* threadContext) {
	int frameOn = 1;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	if (threadContext->state == THREAD_PAUSED) {
		threadContext->state = THREAD_RUNNING;
		ConditionWake(&threadContext->stateCond);
	}
	MutexUnlock(&threadContext->stateMutex);
	MutexLock(&threadContext->sync.videoFrameMutex);
	if (frameOn != threadContext->sync.videoFrameOn) {
		threadContext->sync.videoFrameOn = frameOn;
		ConditionWake(&threadContext->sync.videoFrameAvailableCond);
	}
	MutexUnlock(&threadContext->sync.videoFrameMutex);
}

int GBAThreadIsPaused(struct GBAThread* threadContext) {
	int isPaused;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	isPaused = threadContext->state == THREAD_PAUSED;
	MutexUnlock(&threadContext->stateMutex);
	return isPaused;
}

void GBAThreadTogglePause(struct GBAThread* threadContext) {
	int frameOn = 1;
	MutexLock(&threadContext->stateMutex);
	_waitOnInterrupt(threadContext);
	if (threadContext->state == THREAD_PAUSED) {
		threadContext->state = THREAD_RUNNING;
		ConditionWake(&threadContext->stateCond);
	} else if (threadContext->state == THREAD_RUNNING) {
		if (threadContext->debugger && threadContext->debugger->state == DEBUGGER_RUNNING) {
			threadContext->debugger->state = DEBUGGER_EXITING;
		}
		threadContext->state = THREAD_PAUSED;
		frameOn = 0;
	}
	MutexUnlock(&threadContext->stateMutex);
	MutexLock(&threadContext->sync.videoFrameMutex);
	if (frameOn != threadContext->sync.videoFrameOn) {
		threadContext->sync.videoFrameOn = frameOn;
		ConditionWake(&threadContext->sync.videoFrameAvailableCond);
	}
	MutexUnlock(&threadContext->sync.videoFrameMutex);
}

#ifdef USE_PTHREADS
struct GBAThread* GBAThreadGetContext(void) {
	pthread_once(&_contextOnce, _createTLS);
	return pthread_getspecific(_contextKey);
}
#else
struct GBAThread* GBAThreadGetContext(void) {
	InitOnceExecuteOnce(&_contextOnce, _createTLS, NULL, 0);
	return TlsGetValue(_contextKey);
}
#endif

void GBASyncPostFrame(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	MutexLock(&sync->videoFrameMutex);
	++sync->videoFramePending;
	--sync->videoFrameSkip;
	if (sync->videoFrameSkip < 0) {
		ConditionWake(&sync->videoFrameAvailableCond);
		while (sync->videoFrameWait && sync->videoFramePending) {
			ConditionWait(&sync->videoFrameRequiredCond, &sync->videoFrameMutex);
		}
	}
	MutexUnlock(&sync->videoFrameMutex);

	struct GBAThread* thread = GBAThreadGetContext();
	if (thread->rewindBuffer) {
		--thread->rewindBufferNext;
		if (thread->rewindBufferNext <= 0) {
			thread->rewindBufferNext = thread->rewindBufferInterval;
			GBARecordFrame(thread);
		}
	}
	if (thread->frameCallback) {
		thread->frameCallback(thread);
	}
}

int GBASyncWaitFrameStart(struct GBASync* sync, int frameskip) {
	if (!sync) {
		return 1;
	}

	MutexLock(&sync->videoFrameMutex);
	ConditionWake(&sync->videoFrameRequiredCond);
	if (!sync->videoFrameOn) {
		return 0;
	}
	ConditionWait(&sync->videoFrameAvailableCond, &sync->videoFrameMutex);
	sync->videoFramePending = 0;
	sync->videoFrameSkip = frameskip;
	return 1;
}

void GBASyncWaitFrameEnd(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	MutexUnlock(&sync->videoFrameMutex);
}

int GBASyncDrawingFrame(struct GBASync* sync) {
	return sync->videoFrameSkip <= 0;
}

void GBASyncProduceAudio(struct GBASync* sync, int wait) {
	if (sync->audioWait && wait) {
		// TODO loop properly in event of spurious wakeups
		ConditionWait(&sync->audioRequiredCond, &sync->audioBufferMutex);
	}
	MutexUnlock(&sync->audioBufferMutex);
}

void GBASyncLockAudio(struct GBASync* sync) {
	MutexLock(&sync->audioBufferMutex);
}

void GBASyncConsumeAudio(struct GBASync* sync) {
	ConditionWake(&sync->audioRequiredCond);
	MutexUnlock(&sync->audioBufferMutex);
}
