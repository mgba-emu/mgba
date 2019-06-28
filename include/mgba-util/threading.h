/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef THREADING_H
#define THREADING_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifndef DISABLE_THREADING
#ifdef USE_PTHREADS
#include <mgba-util/platform/posix/threading.h>
#elif defined(_WIN32)
#include <mgba-util/platform/windows/threading.h>
#elif defined(PSP2)
#include <mgba-util/platform/psp2/threading.h>
#elif defined(_3DS)
#include <mgba-util/platform/3ds/threading.h>
#elif defined(__SWITCH__)
#include <mgba-util/platform/switch/threading.h>
#else
#define DISABLE_THREADING
#endif
#endif
#ifdef DISABLE_THREADING
#ifdef _3DS
// ctrulib already has a type called Thread
#include <3ds/thread.h>
#else
typedef void* Thread;
#endif
typedef void* Mutex;
typedef void* Condition;

static inline int MutexInit(Mutex* mutex) {
	UNUSED(mutex);
	return 0;
}

static inline int MutexDeinit(Mutex* mutex) {
	UNUSED(mutex);
	return 0;
}

static inline int MutexLock(Mutex* mutex) {
	UNUSED(mutex);
	return 0;
}

static inline int MutexTryLock(Mutex* mutex) {
	UNUSED(mutex);
	return 0;
}

static inline int MutexUnlock(Mutex* mutex) {
	UNUSED(mutex);
	return 0;
}

static inline int ConditionInit(Condition* cond) {
	UNUSED(cond);
	return 0;
}

static inline int ConditionDeinit(Condition* cond) {
	UNUSED(cond);
	return 0;
}

static inline int ConditionWait(Condition* cond, Mutex* mutex) {
	UNUSED(cond);
	UNUSED(mutex);
	return 0;
}

static inline int ConditionWaitTimed(Condition* cond, Mutex* mutex, int32_t timeoutMs) {
	UNUSED(cond);
	UNUSED(mutex);
	UNUSED(timeoutMs);
	return 0;
}

static inline int ConditionWake(Condition* cond) {
	UNUSED(cond);
	return 0;
}
#endif

CXX_GUARD_END

#endif
