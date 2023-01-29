/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/thread.h>

#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#ifdef ENABLE_SCRIPTING
#include <mgba/script/context.h>
#include <mgba/core/scripting.h>
#endif
#include <mgba/core/serialize.h>
#include <mgba-util/patch.h>
#include <mgba-util/vfs.h>

#include <signal.h>

#ifndef DISABLE_THREADING

static const float _defaultFPSTarget = 60.f;
static ThreadLocal _contextKey;

#ifdef USE_PTHREADS
static pthread_once_t _contextOnce = PTHREAD_ONCE_INIT;

static void _createTLS(void) {
	ThreadLocalInitKey(&_contextKey);
}
#elif _WIN32
static INIT_ONCE _contextOnce = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK _createTLS(PINIT_ONCE once, PVOID param, PVOID* context) {
	UNUSED(once);
	UNUSED(param);
	UNUSED(context);
	ThreadLocalInitKey(&_contextKey);
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
	while (threadContext->state == mTHREAD_INTERRUPTED || threadContext->state == mTHREAD_INTERRUPTING) {
		ConditionWait(&threadContext->stateCond, &threadContext->stateMutex);
	}
}

static void _pokeRequest(struct mCoreThreadInternal* threadContext) {
	switch (threadContext->state) {
	case mTHREAD_RUNNING:
	case mTHREAD_PAUSED:
	case mTHREAD_CRASHED:
		threadContext->state = mTHREAD_REQUEST;
		break;
	case mTHREAD_INITIALIZED:
	case mTHREAD_REQUEST:
	case mTHREAD_INTERRUPTED:
	case mTHREAD_INTERRUPTING:
	case mTHREAD_EXITING:
	case mTHREAD_SHUTDOWN:
		break;
	}
}

static void _waitPrologue(struct mCoreThreadInternal* threadContext, bool* videoFrameWait, bool* audioWait) {
	MutexLock(&threadContext->sync.videoFrameMutex);
	*videoFrameWait = threadContext->sync.videoFrameWait;
	threadContext->sync.videoFrameWait = false;
	MutexUnlock(&threadContext->sync.videoFrameMutex);
	MutexLock(&threadContext->sync.audioBufferMutex);
	*audioWait = threadContext->sync.audioWait;
	threadContext->sync.audioWait = false;
	MutexUnlock(&threadContext->sync.audioBufferMutex);
}

static void _waitEpilogue(struct mCoreThreadInternal* threadContext, bool videoFrameWait, bool audioWait) {
	MutexLock(&threadContext->sync.audioBufferMutex);
	threadContext->sync.audioWait = audioWait;
	MutexUnlock(&threadContext->sync.audioBufferMutex);
	MutexLock(&threadContext->sync.videoFrameMutex);
	threadContext->sync.videoFrameWait = videoFrameWait;
	MutexUnlock(&threadContext->sync.videoFrameMutex);
}

static void _wait(struct mCoreThreadInternal* threadContext) {
	MutexUnlock(&threadContext->stateMutex);

	if (!MutexTryLock(&threadContext->sync.videoFrameMutex)) {
		ConditionWake(&threadContext->sync.videoFrameRequiredCond);
		MutexUnlock(&threadContext->sync.videoFrameMutex);
	}

	if (!MutexTryLock(&threadContext->sync.audioBufferMutex)) {
		ConditionWake(&threadContext->sync.audioRequiredCond);
		MutexUnlock(&threadContext->sync.audioBufferMutex);
	}

#ifdef USE_DEBUGGERS
	if (threadContext->core && threadContext->core->debugger) {
		struct mDebugger* debugger = threadContext->core->debugger;
		if (debugger->interrupt) {
			debugger->interrupt(debugger);
		}
	}
#endif

	MutexLock(&threadContext->stateMutex);
	ConditionWake(&threadContext->stateCond);
}

static void _waitOnRequest(struct mCoreThreadInternal* threadContext, enum mCoreThreadRequest request) {
	bool videoFrameWait, audioWait;
	_waitPrologue(threadContext, &videoFrameWait, &audioWait);
	while (threadContext->requested & request) {
		_pokeRequest(threadContext);
		_wait(threadContext);
	}
	_waitEpilogue(threadContext, videoFrameWait, audioWait);
}

static void _waitUntilNotState(struct mCoreThreadInternal* threadContext, enum mCoreThreadState state) {
	bool videoFrameWait, audioWait;
	_waitPrologue(threadContext, &videoFrameWait, &audioWait);
	while (threadContext->state == state) {
		_wait(threadContext);
	}
	_waitEpilogue(threadContext, videoFrameWait, audioWait);
}

static void _sendRequest(struct mCoreThreadInternal* threadContext, enum mCoreThreadRequest request) {
	threadContext->requested |= request;
	_pokeRequest(threadContext);
}

static void _cancelRequest(struct mCoreThreadInternal* threadContext, enum mCoreThreadRequest request) {
	threadContext->requested &= ~request;
	_pokeRequest(threadContext);
	ConditionWake(&threadContext->stateCond);
}

void _frameStarted(void* context) {
	struct mCoreThread* thread = context;
	if (!thread) {
		return;
	}
	if (thread->core->opts.rewindEnable && thread->core->opts.rewindBufferCapacity > 0) {
		if (!thread->impl->rewinding || !mCoreRewindRestore(&thread->impl->rewind, thread->core)) {
			mCoreRewindAppend(&thread->impl->rewind, thread->core);
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
	_changeState(thread->impl, mTHREAD_CRASHED, true);
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

void _coreShutdown(void* context) {
	struct mCoreThread* thread = context;
	if (!thread) {
		return;
	}
	_changeState(thread->impl, mTHREAD_EXITING, true);
}

#ifdef ENABLE_SCRIPTING
#define ADD_CALLBACK(NAME) \
void _script_ ## NAME(void* context) { \
	struct mCoreThread* threadContext = context; \
	if (!threadContext->scriptContext) { \
		return; \
	} \
	mScriptContextTriggerCallback(threadContext->scriptContext, #NAME, NULL); \
}

ADD_CALLBACK(frame)
ADD_CALLBACK(crashed)
ADD_CALLBACK(sleep)
ADD_CALLBACK(stop)
ADD_CALLBACK(keysRead)
ADD_CALLBACK(savedataUpdated)
ADD_CALLBACK(alarm)

#undef ADD_CALLBACK
#define CALLBACK(NAME) _script_ ## NAME

static void _mCoreThreadAddCallbacks(struct mCoreThread* threadContext) {
	struct mCoreCallbacks callbacks = {
		.videoFrameEnded = CALLBACK(frame),
		.coreCrashed = CALLBACK(crashed),
		.sleep = CALLBACK(sleep),
		.shutdown = CALLBACK(stop),
		.keysRead = CALLBACK(keysRead),
		.savedataUpdated = CALLBACK(savedataUpdated),
		.alarm = CALLBACK(alarm),
		.context = threadContext
	};
	threadContext->core->addCoreCallbacks(threadContext->core, &callbacks);
}
#endif

static THREAD_ENTRY _mCoreThreadRun(void* context) {
	struct mCoreThread* threadContext = context;
#ifdef USE_PTHREADS
	pthread_once(&_contextOnce, _createTLS);
#elif _WIN32
	InitOnceExecuteOnce(&_contextOnce, _createTLS, NULL, 0);
#endif

	ThreadLocalSetKey(_contextKey, threadContext);
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
		.shutdown = _coreShutdown,
		.context = threadContext
	};
	core->addCoreCallbacks(core, &callbacks);
	core->setSync(core, &threadContext->impl->sync);

	struct mLogFilter filter;
	struct mLogger* logger = &threadContext->logger.d;
	if (threadContext->logger.logger) {
		logger->filter = threadContext->logger.logger->filter;
	} else {
		logger->filter = &filter;
		mLogFilterInit(logger->filter);
		mLogFilterLoad(logger->filter, &core->config);
	}

#ifdef ENABLE_SCRIPTING
	struct mScriptContext* scriptContext = threadContext->scriptContext;
	if (scriptContext) {
		mScriptContextAttachCore(scriptContext, core);
		_mCoreThreadAddCallbacks(threadContext);
	}
#endif

	mCoreThreadRewindParamsChanged(threadContext);
	if (threadContext->startCallback) {
		threadContext->startCallback(threadContext);
	}
#ifdef ENABLE_SCRIPTING
	// startCallback could add a script context
	if (scriptContext != threadContext->scriptContext) {
		scriptContext = threadContext->scriptContext;
		if (scriptContext) {
			_mCoreThreadAddCallbacks(threadContext);
		}
	}
	if (scriptContext) {
		mScriptContextTriggerCallback(scriptContext, "start", NULL);
	}
#endif

	core->reset(core);
	threadContext->impl->core = core;
	_changeState(threadContext->impl, mTHREAD_RUNNING, true);

	if (threadContext->resetCallback) {
		threadContext->resetCallback(threadContext);
	}

#ifdef ENABLE_SCRIPTING
	// resetCallback could add a script context
	if (scriptContext != threadContext->scriptContext) {
		scriptContext = threadContext->scriptContext;
		if (scriptContext) {
			_mCoreThreadAddCallbacks(threadContext);
		}
	}
	if (scriptContext) {
		mScriptContextTriggerCallback(scriptContext, "reset", NULL);
	}
#endif

	struct mCoreThreadInternal* impl = threadContext->impl;
	bool wasPaused = false;
	int pendingRequests = 0;

	while (impl->state < mTHREAD_EXITING) {
#ifdef USE_DEBUGGERS
		struct mDebugger* debugger = core->debugger;
		if (debugger) {
			mDebuggerRun(debugger);
			if (debugger->state == DEBUGGER_SHUTDOWN) {
				_changeState(impl, mTHREAD_EXITING, false);
			}
		} else
#endif
		{
			while (impl->state == mTHREAD_RUNNING) {
				core->runLoop(core);
			}
		}

		MutexLock(&impl->stateMutex);
		while (impl->state >= mTHREAD_MIN_WAITING && impl->state < mTHREAD_EXITING) {
			if (impl->state == mTHREAD_INTERRUPTING) {
				impl->state = mTHREAD_INTERRUPTED;
				ConditionWake(&impl->stateCond);
			}

			while (impl->state >= mTHREAD_MIN_WAITING && impl->state <= mTHREAD_MAX_WAITING) {
#ifdef USE_DEBUGGERS
				if (debugger && debugger->update && debugger->state != DEBUGGER_SHUTDOWN) {
					debugger->update(debugger);
					ConditionWaitTimed(&impl->stateCond, &impl->stateMutex, 10);
				} else
#endif
				{
					ConditionWait(&impl->stateCond, &impl->stateMutex);
				}

				if (impl->sync.audioWait) {
					MutexUnlock(&impl->stateMutex);
					mCoreSyncLockAudio(&impl->sync);
					mCoreSyncProduceAudio(&impl->sync, core->getAudioChannel(core, 0), core->getAudioBufferSize(core));
					MutexLock(&impl->stateMutex);
				}
			}
#ifdef ENABLE_SCRIPTING
			if (scriptContext != threadContext->scriptContext) {
				scriptContext = threadContext->scriptContext;
				if (scriptContext) {
					_mCoreThreadAddCallbacks(threadContext);
				}
			}
#endif
			if (wasPaused && !(impl->requested & mTHREAD_REQ_PAUSE)) {
				break;
			}
		}

		impl->requested &= ~pendingRequests | mTHREAD_REQ_PAUSE | mTHREAD_REQ_WAIT;
		pendingRequests = impl->requested;

		if (impl->state == mTHREAD_REQUEST) {
			if (pendingRequests) {
				if (pendingRequests & mTHREAD_REQ_PAUSE) {
					impl->state = mTHREAD_PAUSED;
				}
				if (pendingRequests & mTHREAD_REQ_WAIT) {
					impl->state = mTHREAD_PAUSED;
				}
			} else {
				impl->state = mTHREAD_RUNNING;
				ConditionWake(&threadContext->impl->stateCond);
			}
		}
		MutexUnlock(&impl->stateMutex);

		// Deferred callbacks can't be run inside of the critical section
		if (!wasPaused && (pendingRequests & mTHREAD_REQ_PAUSE)) {
			wasPaused = true;
			if (threadContext->pauseCallback) {
				threadContext->pauseCallback(threadContext);
			}
		}
		if (wasPaused && !(pendingRequests & mTHREAD_REQ_PAUSE)) {
			wasPaused = false;
			if (threadContext->unpauseCallback) {
				threadContext->unpauseCallback(threadContext);
			}
		}
		if (pendingRequests & mTHREAD_REQ_RESET) {
			core->reset(core);
			if (threadContext->resetCallback) {
				threadContext->resetCallback(threadContext);
			}
#ifdef ENABLE_SCRIPTING
			if (scriptContext) {
				mScriptContextTriggerCallback(scriptContext, "reset", NULL);
			}
#endif
		}
		if (pendingRequests & mTHREAD_REQ_RUN_ON) {
			if (threadContext->run) {
				threadContext->run(threadContext);
			}
		}
	}

	while (impl->state < mTHREAD_SHUTDOWN) {
		_changeState(impl, mTHREAD_SHUTDOWN, false);
	}

	if (core->opts.rewindEnable) {
		 mCoreRewindContextDeinit(&impl->rewind);
	}

	if (threadContext->cleanCallback) {
		threadContext->cleanCallback(threadContext);
	}
#ifdef ENABLE_SCRIPTING
	if (scriptContext) {
		mScriptContextTriggerCallback(scriptContext, "shutdown", NULL);
		mScriptContextDetachCore(scriptContext);
	}
#endif
	core->clearCoreCallbacks(core);

	if (logger->filter == &filter) {
		mLogFilterDeinit(&filter);
	}
	logger->filter = NULL;

	return 0;
}

bool mCoreThreadStart(struct mCoreThread* threadContext) {
	threadContext->impl = calloc(sizeof(*threadContext->impl), 1);
	threadContext->impl->state = mTHREAD_INITIALIZED;
	threadContext->impl->requested = 0;
	threadContext->logger.p = threadContext;
	threadContext->logger.d.log = _mCoreLog;
	threadContext->logger.d.filter = NULL;

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
	while (threadContext->impl->state < mTHREAD_RUNNING) {
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
	hasStarted = threadContext->impl->state > mTHREAD_INITIALIZED;
	MutexUnlock(&threadContext->impl->stateMutex);
	return hasStarted;
}

bool mCoreThreadHasExited(struct mCoreThread* threadContext) {
	if (!threadContext->impl) {
		return false;
	}
	bool hasExited;
	MutexLock(&threadContext->impl->stateMutex);
	hasExited = threadContext->impl->state > mTHREAD_EXITING;
	MutexUnlock(&threadContext->impl->stateMutex);
	return hasExited;
}

bool mCoreThreadHasCrashed(struct mCoreThread* threadContext) {
	if (!threadContext->impl) {
		return false;
	}
	bool hasExited;
	MutexLock(&threadContext->impl->stateMutex);
	hasExited = threadContext->impl->state == mTHREAD_CRASHED;
	MutexUnlock(&threadContext->impl->stateMutex);
	return hasExited;
}

void mCoreThreadMarkCrashed(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	threadContext->impl->state = mTHREAD_CRASHED;
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadClearCrashed(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	if (threadContext->impl->state == mTHREAD_CRASHED) {
		threadContext->impl->state = mTHREAD_REQUEST;
		ConditionWake(&threadContext->impl->stateCond);
	}
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadEnd(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	threadContext->impl->state = mTHREAD_EXITING;
	ConditionWake(&threadContext->impl->stateCond);
	MutexUnlock(&threadContext->impl->stateMutex);
	MutexLock(&threadContext->impl->sync.audioBufferMutex);
	threadContext->impl->sync.audioWait = 0;
	ConditionWake(&threadContext->impl->sync.audioRequiredCond);
	MutexUnlock(&threadContext->impl->sync.audioBufferMutex);

	MutexLock(&threadContext->impl->sync.videoFrameMutex);
	threadContext->impl->sync.videoFrameWait = false;
	ConditionWake(&threadContext->impl->sync.videoFrameRequiredCond);
	ConditionWake(&threadContext->impl->sync.videoFrameAvailableCond);
	MutexUnlock(&threadContext->impl->sync.videoFrameMutex);
}

void mCoreThreadReset(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	_sendRequest(threadContext->impl, mTHREAD_REQ_RESET);
	_waitOnRequest(threadContext->impl, mTHREAD_REQ_RESET);
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
	return threadContext->impl->state >= mTHREAD_RUNNING && threadContext->impl->state < mTHREAD_EXITING && threadContext->impl->state != mTHREAD_CRASHED;
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
	threadContext->impl->state = mTHREAD_INTERRUPTING;
	_waitUntilNotState(threadContext->impl, mTHREAD_INTERRUPTING);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadInterruptFromThread(struct mCoreThread* threadContext) {
	if (!threadContext) {
		return;
	}
	MutexLock(&threadContext->impl->stateMutex);
	++threadContext->impl->interruptDepth;
	if (threadContext->impl->interruptDepth > 1 || !mCoreThreadIsActive(threadContext)) {
		if (threadContext->impl->state == mTHREAD_INTERRUPTING) {
			threadContext->impl->state = mTHREAD_INTERRUPTED;
		}
		MutexUnlock(&threadContext->impl->stateMutex);
		return;
	}
	threadContext->impl->state = mTHREAD_INTERRUPTING;
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
		if (threadContext->impl->requested) {
			threadContext->impl->state = mTHREAD_REQUEST;
		} else {
			threadContext->impl->state = mTHREAD_RUNNING;			
		}
		ConditionWake(&threadContext->impl->stateCond);
	}
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadRunFunction(struct mCoreThread* threadContext, void (*run)(struct mCoreThread*)) {
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	threadContext->run = run;
	_sendRequest(threadContext->impl, mTHREAD_REQ_RUN_ON);
	_waitOnRequest(threadContext->impl, mTHREAD_REQ_RUN_ON);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadPause(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	_sendRequest(threadContext->impl, mTHREAD_REQ_PAUSE);
	_waitUntilNotState(threadContext->impl, mTHREAD_REQUEST);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadUnpause(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	_cancelRequest(threadContext->impl, mTHREAD_REQ_PAUSE);
	_waitUntilNotState(threadContext->impl, mTHREAD_REQUEST);
	MutexUnlock(&threadContext->impl->stateMutex);
}

bool mCoreThreadIsPaused(struct mCoreThread* threadContext) {
	bool isPaused;
	MutexLock(&threadContext->impl->stateMutex);
	isPaused = !!(threadContext->impl->requested & mTHREAD_REQ_PAUSE);
	MutexUnlock(&threadContext->impl->stateMutex);
	return isPaused;
}

void mCoreThreadTogglePause(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	_waitOnInterrupt(threadContext->impl);
	if (threadContext->impl->requested & mTHREAD_REQ_PAUSE) {
		_cancelRequest(threadContext->impl, mTHREAD_REQ_PAUSE);
	} else {
		_sendRequest(threadContext->impl, mTHREAD_REQ_PAUSE);
	}
	_waitUntilNotState(threadContext->impl, mTHREAD_REQUEST);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadPauseFromThread(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	_sendRequest(threadContext->impl, mTHREAD_REQ_PAUSE);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadSetRewinding(struct mCoreThread* threadContext, bool rewinding) {
	MutexLock(&threadContext->impl->stateMutex);
	threadContext->impl->rewinding = rewinding;
	if (rewinding && threadContext->impl->state == mTHREAD_CRASHED) {
		threadContext->impl->state = mTHREAD_REQUEST;
		ConditionWake(&threadContext->impl->stateCond);
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
	_sendRequest(threadContext->impl, mTHREAD_REQ_WAIT);
	MutexUnlock(&threadContext->impl->stateMutex);
}

void mCoreThreadStopWaiting(struct mCoreThread* threadContext) {
	MutexLock(&threadContext->impl->stateMutex);
	_cancelRequest(threadContext->impl, mTHREAD_REQ_WAIT);
	MutexUnlock(&threadContext->impl->stateMutex);
}

struct mCoreThread* mCoreThreadGet(void) {
#ifdef USE_PTHREADS
	pthread_once(&_contextOnce, _createTLS);
#elif _WIN32
	InitOnceExecuteOnce(&_contextOnce, _createTLS, NULL, 0);
#endif
	return ThreadLocalGetValue(_contextKey);
}

static void _mCoreLog(struct mLogger* logger, int category, enum mLogLevel level, const char* format, va_list args) {
	struct mThreadLogger* threadLogger = (struct mThreadLogger*) logger;
	if (level == mLOG_FATAL) {
		mCoreThreadMarkCrashed(threadLogger->p);
	}
	if (!threadLogger->p->logger.logger) {
		printf("%s: ", mLogCategoryName(category));
		vprintf(format, args);
		printf("\n");
	} else {
		logger = threadLogger->p->logger.logger;
		logger->log(logger, category, level, format, args);
	}
}
#else
struct mCoreThread* mCoreThreadGet(void) {
	return NULL;
}
#endif

struct mLogger* mCoreThreadLogger(void) {
	struct mCoreThread* thread = mCoreThreadGet();
	if (thread) {
		return &thread->logger.d;
	}
	return NULL;
}

