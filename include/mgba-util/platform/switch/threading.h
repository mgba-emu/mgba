/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SWITCH_THREADING_H
#define SWITCH_THREADING_H

#include <mgba-util/common.h>

#include <switch.h>

#define THREAD_ENTRY void
typedef ThreadFunc ThreadEntry;
typedef CondVar Condition;

static inline int MutexInit(Mutex* mutex) {
	mutexInit(mutex);
	return 0;
}

static inline int MutexDeinit(Mutex* mutex) {
	UNUSED(mutex);
	return 0;
}

static inline int MutexLock(Mutex* mutex) {
	mutexLock(mutex);
	return 0;
}

static inline int MutexTryLock(Mutex* mutex) {
	return mutexTryLock(mutex);
}

static inline int MutexUnlock(Mutex* mutex) {
	mutexUnlock(mutex);
	return 0;
}

static inline int ConditionInit(Condition* cond) {
	condvarInit(cond);
	return 0;
}

static inline int ConditionDeinit(Condition* cond) {
	UNUSED(cond);
	return 0;
}

static inline int ConditionWait(Condition* cond, Mutex* mutex) {
	return condvarWait(cond, mutex);
}

static inline int ConditionWaitTimed(Condition* cond, Mutex* mutex, int32_t timeoutMs) {
	return condvarWaitTimeout(cond, mutex, timeoutMs * 1000000LL);
}

static inline int ConditionWake(Condition* cond) {
	return condvarWakeOne(cond);
}

static inline int ThreadCreate(Thread* thread, ThreadEntry entry, void* context) {
	if (!entry || !thread) {
		return 1;
	}
	int res = threadCreate(thread, entry, context, NULL, 0x8000, 0x3B, 1);
	if(R_FAILED(res)) {
		return res;
	}
	return threadStart(thread);
}

static inline int ThreadJoin(Thread* thread) {
	int res = threadWaitForExit(thread);
	if(R_FAILED(res)) {
		return res;
	}
	return threadClose(thread);
}

static inline void ThreadSetName(const char* name) {
	UNUSED(name);
	// Unimplemented
}

#endif
