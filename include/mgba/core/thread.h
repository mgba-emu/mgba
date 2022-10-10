/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_CORE_THREAD_H
#define M_CORE_THREAD_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

struct mCoreThread;
struct mCore;

typedef void (*ThreadCallback)(struct mCoreThread* threadContext);

struct mCoreThread;
struct mThreadLogger {
	struct mLogger d;
	struct mCoreThread* p;
	struct mLogger* logger;
};

#ifdef ENABLE_SCRIPTING
struct mScriptContext;
#endif
struct mCoreThreadInternal;
struct mCoreThread {
	// Input
	struct mCore* core;

	struct mThreadLogger logger;
	ThreadCallback startCallback;
	ThreadCallback resetCallback;
	ThreadCallback cleanCallback;
	ThreadCallback frameCallback;
	ThreadCallback sleepCallback;
	ThreadCallback pauseCallback;
	ThreadCallback unpauseCallback;
	void* userData;
	void (*run)(struct mCoreThread*);

#ifdef ENABLE_SCRIPTING
	struct mScriptContext* scriptContext;
#endif

	struct mCoreThreadInternal* impl;
};

#ifndef OPAQUE_THREADING
#include <mgba/core/rewind.h>
#include <mgba/core/sync.h>
#include <mgba-util/threading.h>

enum mCoreThreadState {
	mTHREAD_INITIALIZED = -1,
	mTHREAD_RUNNING = 0,
	mTHREAD_REQUEST,

	mTHREAD_INTERRUPTED,
	mTHREAD_PAUSED,
	mTHREAD_CRASHED,

	mTHREAD_INTERRUPTING,
	mTHREAD_EXITING,

	mTHREAD_SHUTDOWN,

	mTHREAD_MIN_WAITING = mTHREAD_INTERRUPTED,
	mTHREAD_MAX_WAITING = mTHREAD_CRASHED
};

enum mCoreThreadRequest {
	mTHREAD_REQ_PAUSE = 1, // User-set pause
	mTHREAD_REQ_WAIT = 2, // Core-set pause
	mTHREAD_REQ_RESET = 4,
	mTHREAD_REQ_RUN_ON = 8,
};

struct mCoreThreadInternal {
	Thread thread;
	enum mCoreThreadState state;
	bool rewinding;
	int requested;

	Mutex stateMutex;
	Condition stateCond;
	int interruptDepth;
	bool frameWasOn;

	struct mCoreSync sync;
	struct mCoreRewindContext rewind;
	struct mCore* core;
};

#endif

bool mCoreThreadStart(struct mCoreThread* threadContext);
bool mCoreThreadHasStarted(struct mCoreThread* threadContext);
bool mCoreThreadHasExited(struct mCoreThread* threadContext);
void mCoreThreadEnd(struct mCoreThread* threadContext);
void mCoreThreadReset(struct mCoreThread* threadContext);
void mCoreThreadJoin(struct mCoreThread* threadContext);

bool mCoreThreadIsActive(struct mCoreThread* threadContext);
void mCoreThreadInterrupt(struct mCoreThread* threadContext);
void mCoreThreadInterruptFromThread(struct mCoreThread* threadContext);
void mCoreThreadContinue(struct mCoreThread* threadContext);

void mCoreThreadRunFunction(struct mCoreThread* threadContext, void (*run)(struct mCoreThread*));

void mCoreThreadPause(struct mCoreThread* threadContext);
void mCoreThreadUnpause(struct mCoreThread* threadContext);
bool mCoreThreadIsPaused(struct mCoreThread* threadContext);
void mCoreThreadTogglePause(struct mCoreThread* threadContext);
void mCoreThreadPauseFromThread(struct mCoreThread* threadContext);
void mCoreThreadWaitFromThread(struct mCoreThread* threadContext);
void mCoreThreadStopWaiting(struct mCoreThread* threadContext);

bool mCoreThreadHasCrashed(struct mCoreThread* threadContext);
void mCoreThreadMarkCrashed(struct mCoreThread* threadContext);
void mCoreThreadClearCrashed(struct mCoreThread* threadContext);

void mCoreThreadSetRewinding(struct mCoreThread* threadContext, bool);
void mCoreThreadRewindParamsChanged(struct mCoreThread* threadContext);

struct mCoreThread* mCoreThreadGet(void);
struct mLogger* mCoreThreadLogger(void);

CXX_GUARD_END

#endif
