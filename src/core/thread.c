/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/thread.h>

#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba-util/patch.h>
#include <mgba-util/vfs.h>

#include <signal.h>

#ifndef DISABLE_THREADING

static const float _defaultFPSTarget = 60.f;

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

static void _mCoreLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args);

static void _changeState(struct mCoreThreadInternal* threadContext, enum mCoreThreadState newState, bool broadcast) {
	MutexLock(&threadContext->stateMutex);
	threadContext->state = newState;
	if (broadcast) {
		ConditionWake(&threadContext->stateCond);
	}
	MutexUnlock(&threadContext->stateMutex);
}

static void _waitOnInterrupt(struct mCoreThreadInternal* threadContext) {
	while (threadContext->state == THREAD_INTERRUPTED || threadContext->state == THREAD_INTERRUPTING) {
		ConditionWait(&threadContext->stateCond, &threadContext->stateMutex);
	}
}

static void _waitUntilNotState(struct mCoreThreadInternal* threadContext, enum mCoreThreadState oldState) {
	MutexLock(&threadContext->sync.videoFrameMutex);
	bool videoFrameWait = threadContext->sync.videoFrameWait;
	threadContext->sync.videoFrameWait = false;
	MutexUnlock(&threadContext->sync.videoFrameMutex);

	MutexLock(&threadContext->sync.audioBufferMutex);
	bool audioWait = threadContext->sync.audioWait;
	threadContext->sync.audioWait = false;
	MutexUnlock(&threadContext->sync.audioBufferMutex);

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

	MutexLock(&threadContext->sync.audioBufferMutex);
	threadContext->sync.audioWait = audioWait;
	MutexUnlock(&threadContext->sync.audioBufferMutex);

	MutexLock(&threadContext->sync.videoFrameMutex);
	threadContext->sync.videoFrameWait = videoFrameWait;
	MutexUnlock(&threadContext->sync.videoFrameMutex);
}

static void _pauseThread(struct mCoreThreadInternal* threadContext) {
	threadContext->state = THREAD_PAUSING;
	_waitUntilNotState(threadContext, THREAD_PAUSING);
}

void _frameStarted(void* context) {
	struct mCoreThread* thread = context;
	if (!thread) {
		return;
	}
	if (thread->core->opts.rewindEnable && thread->core->opts.rewindBufferCapacity > 0) {
		if (thread->impl->state != THREAD_REWINDING) {
			mCoreRewindAppend(&thread->impl->rewind, thread->core);
		} else if (thread->impl->state == THREAD_REWINDING) {
			if (!mCoreRewindRestore(&thread->impl->rewind, thread->core)) {
				mCoreRewindAppend(&thread->impl->rewind, thread->core);
			}
		}
	}
}

void _frameEnded(void* context) {
	struct mCoreThread* thread = context;
	if (!thread) {
		return;
	}
	if (thread->frameCallback) {
		thread->frameCallback(thread);
	}
}

void _crashed(void* context) {
	struct mCoreThread* thread = context;
	if (!thread) {
		return;
	}
	_changeState(thread->impl, THREAD_CRASHED, true);
}

void _coreSleep(void* context) {
	struct mCoreThread* thread = context;
	if (!thread) {
		return;
	}
	if (thread->sleepCallback) {
		thread->sleepCallback(thread);
	}
}

static THREAD_ENTRY _mCoreThreadRun(void* context) {
	struct mCoreThread* threadContext = context;
#ifdef USE_PTHREADS
	pthread_once(&_contextOnce, _createTLS);
	pthread_setspecific(_contextKey, threadContext);
#elif _WIN32
	InitOnceExecuteOnce(&_contextOnce, _createTLS, NULL, 0);
	TlsSetValue(_contextKey, threadContext);
#endif

	ThreadSetName("CPU Thread");

#if !defined(_WIN32) && defined(USE_PTHREADS)
	sigset_t signals;
	sigemptyset(&signals);
	pthread_sigmask(SIG_SETMASK, &signals, 0);
#endif

	struct mCore* core = threadContext->core;
	struct mCoreCallbacks callbacks = {
		.videoFrameStarted = _frameStarted,
		.videoFrameEnded = _frameEnded,
		.coreCrashed = _crashed,
		.sleep = _coreSleep,
		.context = threadContext
	};
	core->addCoreCallbacks(core, &callbacks);
	core->setSync(core, &threadContext->impl->sync);

	struct mLogFilter filter;
	if (!threadContext->logger.d.filter) {
		threadContext->logger.d.filter = &filter;
		mLogFilterInit(threadContext->logger.d.filter);
		mLogFilterLoad(threadContext->logger.d.filter, &core->config);
	}

	mCoreThreadRewindParamsChanged(threadContext);
	if (threadContext->startCallback) {
		threadContext->startCallback(threadContext);
	}

	core->reset(core);
	_changeState(threadContext->impl, THREAD_RUNNING, true);

	if (threadContext->resetCallback) {
		threadContext->resetCallback(threadContext);
	}

	struct mCoreThreadInternal* impl = threadContext->impl;
	while (impl->state < THREAD_EXITING) {
#ifdef USE_DEBUGGERS
		struct mDebugger* debugger = core->debugger;
		if (debugger) {
			mDebuggerRun(debugger);
			if (debugger->state == DEBUGGER_SHUTDOWN) {
				_changeState(impl, THREAD_EXITING, false);
			}
		} else
#endif
		{
			while (impl->state <= THREAD_MAX_RUNNING) {
				core->runLoop(core);
			}
		}

		enum mCoreThreadState deferred = THREAD_RUNNING;
		MutexLock(&impl->stateMutex);
		while (impl->state > THREAD_MAX_RUNNING && impl->state < THREAD_EXITING) {
			deferred = impl->state;

			switch (deferred) {
			case THREAD_INTERRUPTING:
				impl->state = THREAD_INTERRUPTED;
				ConditionWake(&impl->stateCond);
				break;
			case THREAD_PAUSING:
				impl->state = THREAD_PAUSED;
				break;
			case THREAD_RESETING:
				impl->state = THREAD_RUNNING;
				break;
			default:
				break;
			}

			if (deferred >= THREAD_MIN_DEFERRED && deferred <= THREAD_MAX_DEFERRED) {
				break;
			}

			deferred = impl->state;
			if (deferred == THREAD_INTERRUPTED) {
				deferred = impl->savedState;
			}
			while (impl->state >= THREAD_WAITING && impl->state <= THREAD_MAX_WAITING) {
				ConditionWait(&impl->stateCond, &impl->stateMutex);

				if (impl->sync.audioWait) {
					MutexUnlock(&impl->stateMutex);
					mCoreSyncLockAudio(&impl->sync);
					mCoreSyncProduceAudio(&impl->sync, core->getAudioChannel(core, 0), core->getAudioBufferSize(core));
					MutexLock(&impl->stateMutex);
				}
			}
		}
		MutexUnlock(&impl->stateMutex);
		switch (deferred) {
		case THREAD_PAUSING:
			if (threadContext->pauseCallback) {
				threadContext->pauseCallback(threadContext);
			}
			break;
		case THREAD_PAUSED:
			if (threadContext->unpauseCallback) {
				threadContext->unpauseCallback(threadContext);
			}
			break;
		case THREAD_RUN_ON:
			if (threadContext->run) {
				threadContext->run(threadContext);
			}
			threadContext->impl->state = threadContext->impl->savedState;
			break;
		case THREAD_RESETING:
			core->reset(core);
			if (threadContext->resetCallback) {
				threadContext->resetCallback(threadContext);
			}
			break;
		default:
			break;
		}
	}

	while (impl->state < THREAD_SHUTDOWN) {
		_changeState(impl, THREAD_SHUTDOWN, false);
	}

	if (core->opts.rewindEnable) {
		 mCoreRewindContextDeinit(&impl->rewind);
	}

	if (threadContext->cleanCallback) {
		threadContext->cleanCallback(threadContext);
	}
	core->clearCoreCallbacks(core);

	if (threadContext->logger.d.filter == &filter) {
		mLogFilterDeinit(&filter);
	}
	threadContext->logger.d.filter = NULL;

	return 0;
}

bool mCoreThreadStart(struct mCoreThread* threadContext) {
	threadContext->impl = calloc(sizeof(*threadContext->impl), 1);
	threadContext->impl->state = THREAD_INITIALIZED;
	threadContext->logger.p = threadContext;
	if (!threadContext->logger.d.log) {
		threadContext->logger.d.log = _mCoreLog;
		threadContext->logger.d.filter = NULL;
	}

	if (!threadContext->impl->sync.fpsTarget) {
		threadContext->impl->sync.fpsTarget = _defaultFPSTarget;
	}

	MutexInit(&threadContext->impl->stateMutex);
	ConditionInit(&threadContext->impl->stateCond);

	MutexInit(&threadContext->impl->sync.videoFrameMutex);
	ConditionInit(&threadContext->impl->sync.videoFrameAvailableCond);
	ConditionInit(&threadContext->impl->sync.videoFrameRequiredCond);
	MutexInit(&threadContext->impl->sync.audioBufferMutex);
	ConditionInit(&threadContext->impl->sync.audioRequiredCond);

	threadContext->impl->interruptDepth = 0;

#ifdef USE_PTHREADS
	sigset_t signals;
	sigemptyset(&signals);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGTRAP);
	pthread_sigmask(SIG_BLOCK, &signals, 0);
#endif

	threadContext->impl->sync.audioWait = threadContext->core->opts.audioSync;
	threadContext->impl->sync.videoFrameWait = threadContext->core->opts.videoSync;
	threadContext->impl->sync.fpsTarget = threadContext->core->opts.fpsTarget;

	MutexLock(&threadContext->impl->stateMutex);
	ThreadCreate(&threadContext->impl->thread, _mCoreThreadRun, threadContext);
	while (threadContext->impl->state < THREAD_RUNNING) {
		ConditionWait(&threadContext->impl->stateCond, &threadContext->impl->stateMutex);
	}
	MutexUnlock(&threadContext->impl->stateMutex);

	return true;
}

bool mCoreThreadHasStarted(struct mCoreThread* threadContext) {
	if (!threadContext->impl) {
		return false;
	}
	bool hasStarted;
	MutexLock(&threadContext->impl->stateMutex);
	hasStarted = threadContext->impl->state > THREAD_INITIALIZED;
	MutexUnlock(&threadContext->impl->stateMutex);
	return hasStarted;
}

bool mCoreThreadHasExited(struct mCoreThread* threadContext) {
	if (!threadContext->impl) {
		return false;
	}
	bool hasExited;
	MutexLock(&threadContext->impl->stateMutex);
	hasExited = threadContext->impl->state > THREAD_EXITING;
	MutexUnlock(&threadContext->impl->stateMutex);
	return hasExited;
}

bool mCoreThreadHasCrashed(struct mCoreThread* threadContext) {
	if (!threadContext->impl) {
		return false;
	}
	bool hasExited;
	MutexLock(&threadContext->impl->stateMutex);
	hasExited = threadContext->impl->state == THREAD_CRASHED;
	MutexUnlock(&threadContext->impl->stateMutex);
	return hasExited;
}

void mCoreThreadMarkCrashed(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	threadContext->impl->state = THREAD_CRASHED;
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadEnd(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	threadContext->impl->state = THREAD_EXITING;
	ConditionWake(&threadContext->impl->stateCond);
	MutexUnlock(&threadContext->impl->stateMutex);
	MutexLock(&threadContext->impl->sync.audioBufferMutex);
	threadContext->impl->sync.audioWait = 0;
	ConditionWake(&threadContext->impl->sync.audioRequiredCond);
	MutexUnlock(&threadContext->impl->sync.audioBufferMutex);

	MutexLock(&threadContext->impl->sync.videoFrameMutex);
	threadContext->impl->sync.videoFrameWait = false;
	threadContext->impl->sync.videoFrameOn = false;
	ConditionWake(&threadContext->impl->sync.videoFrameRequiredCond);
	ConditionWake(&threadContext->impl->sync.videoFrameAvailableCond);
	MutexUnlock(&threadContext->impl->sync.videoFrameMutex);
}

void mCoreThreadReset(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	if (threadContext->impl->state == THREAD_INTERRUPTED || threadContext->impl->state == THREAD_INTERRUPTING) {
		threadContext->impl->savedState = THREAD_RESETING;
	} else {
		threadContext->impl->state = THREAD_RESETING;
	}
	ConditionWake(&threadContext->impl->stateCond);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadJoin(struct mCoreThread* threadContext) {
	if (!threadContext->impl) {
		return;
	}
	ThreadJoin(&threadContext->impl->thread);

	MutexDeinit(&threadContext->impl->stateMutex);
	ConditionDeinit(&threadContext->impl->stateCond);

	MutexDeinit(&threadContext->impl->sync.videoFrameMutex);
	ConditionWake(&threadContext->impl->sync.videoFrameAvailableCond);
	ConditionDeinit(&threadContext->impl->sync.videoFrameAvailableCond);
	ConditionWake(&threadContext->impl->sync.videoFrameRequiredCond);
	ConditionDeinit(&threadContext->impl->sync.videoFrameRequiredCond);

	ConditionWake(&threadContext->impl->sync.audioRequiredCond);
	ConditionDeinit(&threadContext->impl->sync.audioRequiredCond);
	MutexDeinit(&threadContext->impl->sync.audioBufferMutex);

	free(threadContext->impl);
	threadContext->impl = NULL;
}

bool mCoreThreadIsActive(struct mCoreThread* threadContext) {
	if (!threadContext->impl) {
		return false;
	}
	return threadContext->impl->state >= THREAD_RUNNING && threadContext->impl->state < THREAD_EXITING;
}

void mCoreThreadInterrupt(struct mCoreThread* threadContext) {
	if (!threadContext) {
		return;
	}
	MutexLock(&threadContext->impl->stateMutex);
	++threadContext->impl->interruptDepth;
	if (threadContext->impl->interruptDepth > 1 || !mCoreThreadIsActive(threadContext)) {
		MutexUnlock(&threadContext->impl->stateMutex);
		return;
	}
	threadContext->impl->savedState = threadContext->impl->state;
	_waitOnInterrupt(threadContext->impl);
	threadContext->impl->state = THREAD_INTERRUPTING;
	ConditionWake(&threadContext->impl->stateCond);
	_waitUntilNotState(threadContext->impl, THREAD_INTERRUPTING);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadInterruptFromThread(struct mCoreThread* threadContext) {
	if (!threadContext) {
		return;
	}
	MutexLock(&threadContext->impl->stateMutex);
	++threadContext->impl->interruptDepth;
	if (threadContext->impl->interruptDepth > 1 || !mCoreThreadIsActive(threadContext)) {
		if (threadContext->impl->state == THREAD_INTERRUPTING) {
			threadContext->impl->state = THREAD_INTERRUPTED;
		}
		MutexUnlock(&threadContext->impl->stateMutex);
		return;
	}
	threadContext->impl->savedState = threadContext->impl->state;
	threadContext->impl->state = THREAD_INTERRUPTING;
	ConditionWake(&threadContext->impl->stateCond);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadContinue(struct mCoreThread* threadContext) {
	if (!threadContext) {
		return;
	}
	MutexLock(&threadContext->impl->stateMutex);
	--threadContext->impl->interruptDepth;
	if (threadContext->impl->interruptDepth < 1 && mCoreThreadIsActive(threadContext)) {
		threadContext->impl->state = threadContext->impl->savedState;
		ConditionWake(&threadContext->impl->stateCond);
	}
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadRunFunction(struct mCoreThread* threadContext, void (*run)(struct mCoreThread*)) {
	MutexLock(&threadContext->impl->stateMutex);
	threadContext->run = run;
	_waitOnInterrupt(threadContext->impl);
	threadContext->impl->savedState = threadContext->impl->state;
	threadContext->impl->state = THREAD_RUN_ON;
	ConditionWake(&threadContext->impl->stateCond);
	_waitUntilNotState(threadContext->impl, THREAD_RUN_ON);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadPause(struct mCoreThread* threadContext) {
	bool frameOn = threadContext->impl->sync.videoFrameOn;
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	if (threadContext->impl->state == THREAD_RUNNING) {
		_pauseThread(threadContext->impl);
		threadContext->impl->frameWasOn = frameOn;
		frameOn = false;
	}
	MutexUnlock(&threadContext->impl->stateMutex);

	mCoreSyncSetVideoSync(&threadContext->impl->sync, frameOn);
}

void mCoreThreadUnpause(struct mCoreThread* threadContext) {
	bool frameOn = threadContext->impl->sync.videoFrameOn;
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	if (threadContext->impl->state == THREAD_PAUSED || threadContext->impl->state == THREAD_PAUSING) {
		threadContext->impl->state = THREAD_RUNNING;
		ConditionWake(&threadContext->impl->stateCond);
		frameOn = threadContext->impl->frameWasOn;
	}
	MutexUnlock(&threadContext->impl->stateMutex);

	mCoreSyncSetVideoSync(&threadContext->impl->sync, frameOn);
}

bool mCoreThreadIsPaused(struct mCoreThread* threadContext) {
	bool isPaused;
	MutexLock(&threadContext->impl->stateMutex);
	if (threadContext->impl->interruptDepth) {
		isPaused = threadContext->impl->savedState == THREAD_PAUSED;
	} else {
		isPaused = threadContext->impl->state == THREAD_PAUSED;
	}
	MutexUnlock(&threadContext->impl->stateMutex);
	return isPaused;
}

void mCoreThreadTogglePause(struct mCoreThread* threadContext) {
	bool frameOn = threadContext->impl->sync.videoFrameOn;
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	if (threadContext->impl->state == THREAD_PAUSED || threadContext->impl->state == THREAD_PAUSING) {
		threadContext->impl->state = THREAD_RUNNING;
		ConditionWake(&threadContext->impl->stateCond);
		frameOn = threadContext->impl->frameWasOn;
	} else if (threadContext->impl->state == THREAD_RUNNING) {
		_pauseThread(threadContext->impl);
		threadContext->impl->frameWasOn = frameOn;
		frameOn = false;
	}
	MutexUnlock(&threadContext->impl->stateMutex);

	mCoreSyncSetVideoSync(&threadContext->impl->sync, frameOn);
}

void mCoreThreadPauseFromThread(struct mCoreThread* threadContext) {
	bool frameOn = true;
	MutexLock(&threadContext->impl->stateMutex);
	if (threadContext->impl->state == THREAD_RUNNING || (threadContext->impl->interruptDepth && threadContext->impl->savedState == THREAD_RUNNING)) {
		threadContext->impl->state = THREAD_PAUSING;
		frameOn = false;
	}
	MutexUnlock(&threadContext->impl->stateMutex);

	mCoreSyncSetVideoSync(&threadContext->impl->sync, frameOn);
}

void mCoreThreadSetRewinding(struct mCoreThread* threadContext, bool rewinding) {
	MutexLock(&threadContext->impl->stateMutex);
	if (rewinding && (threadContext->impl->state == THREAD_REWINDING || (threadContext->impl->interruptDepth && threadContext->impl->savedState == THREAD_REWINDING))) {
		MutexUnlock(&threadContext->impl->stateMutex);
		return;
	}
	if (!rewinding && ((!threadContext->impl->interruptDepth && threadContext->impl->state != THREAD_REWINDING) || (threadContext->impl->interruptDepth && threadContext->impl->savedState != THREAD_REWINDING))) {
		MutexUnlock(&threadContext->impl->stateMutex);
		return;
	}
	_waitOnInterrupt(threadContext->impl);
	if (rewinding && threadContext->impl->state == THREAD_RUNNING) {
		threadContext->impl->state = THREAD_REWINDING;
	}
	if (!rewinding && threadContext->impl->state == THREAD_REWINDING) {
		threadContext->impl->state = THREAD_RUNNING;
	}
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadRewindParamsChanged(struct mCoreThread* threadContext) {
	struct mCore* core = threadContext->core;
	if (core->opts.rewindEnable && core->opts.rewindBufferCapacity > 0) {
		 mCoreRewindContextInit(&threadContext->impl->rewind, core->opts.rewindBufferCapacity, true);
	} else {
		 mCoreRewindContextDeinit(&threadContext->impl->rewind);
	}
}

void mCoreThreadWaitFromThread(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	if (threadContext->impl->interruptDepth && threadContext->impl->savedState == THREAD_RUNNING) {
		threadContext->impl->savedState = THREAD_WAITING;
	} else if (threadContext->impl->state == THREAD_RUNNING) {
		threadContext->impl->state = THREAD_WAITING;
	}
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadStopWaiting(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	if (threadContext->impl->interruptDepth && threadContext->impl->savedState == THREAD_WAITING) {
		threadContext->impl->savedState = THREAD_RUNNING;
	} else if (threadContext->impl->state == THREAD_WAITING) {
		threadContext->impl->state = THREAD_RUNNING;
		ConditionWake(&threadContext->impl->stateCond);
	}
	MutexUnlock(&threadContext->impl->stateMutex);
}

#ifdef USE_PTHREADS
struct mCoreThread* mCoreThreadGet(void) {
	pthread_once(&_contextOnce, _createTLS);
	return pthread_getspecific(_contextKey);
}
#elif _WIN32
struct mCoreThread* mCoreThreadGet(void) {
	InitOnceExecuteOnce(&_contextOnce, _createTLS, NULL, 0);
	return TlsGetValue(_contextKey);
}
#else
struct mCoreThread* mCoreThreadGet(void) {
	return NULL;
}
#endif

#else
struct mCoreThread* mCoreThreadGet(void) {
	return NULL;
}
#endif

static void _mCoreLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	UNUSED(logger);
	UNUSED(level);
	printf("%s: ", mLogCategoryName(category));
	vprintf(format, args);
	printf("\n");
	struct mCoreThread* thread = mCoreThreadGet();
	if (thread && level == mLOG_FATAL) {
#ifndef DISABLE_THREADING
		mCoreThreadMarkCrashed(thread);
#endif
	}
}

struct mLogger* mCoreThreadLogger(void) {
	struct mCoreThread* thread = mCoreThreadGet();
	if (thread) {
		return &thread->logger.d;
	}
	return NULL;
}

