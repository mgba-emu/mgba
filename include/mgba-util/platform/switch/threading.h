/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SWITCH_THREADING_H
#define SWITCH_THREADING_H

#include <mgba-util/common.h>

#include <malloc.h>
#include <switch.h>

// to be implemented

#define THREAD_ENTRY void
typedef ThreadFunc ThreadEntry;

typedef struct {
	CondVar condVar;
	Mutex mutex;
} Condition;

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
	mutexInit(&cond->mutex);
	condvarInit(&cond->condVar, &cond->mutex);
	return 0;
}

static inline int ConditionDeinit(Condition* cond) {
	UNUSED(cond);
	return 0;
}

static inline int ConditionWait(Condition* cond, Mutex* mutex) {
	int retval;

	condvarInit(&cond->condVar, mutex);

	mutexUnlock(mutex);
	retval = condvarWait(&cond->condVar);

	mutexLock(mutex);

	return retval;
}

static inline int ConditionWaitTimed(Condition* cond, Mutex* mutex, int32_t timeoutMs) {
	int retval;

	condvarInit(&cond->condVar, mutex);

	mutexUnlock(mutex);
	retval = condvarWaitTimeout(&cond->condVar, timeoutMs * 10000000LL);

	mutexLock(mutex);

	return retval;
}

static inline int ConditionWake(Condition* cond) {
	return condvarWakeOne(&cond->condVar);
}

static inline int ThreadCreate(Thread* thread, ThreadEntry entry, void* context) {
	if (!entry || !thread) {
		return 1;
	}
	Result r = threadCreate(thread, entry, context, 0x10000, 0x2B, -2);
	if(R_FAILED(r)) return 1;
	return threadStart(thread);
}

static inline int ThreadJoin(Thread thread) {
	return threadWaitForExit(&thread);
}

static inline void ThreadSetName(const char* name) {
	UNUSED(name);
	// Unimplemented
}

#endif
