/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef N3DS_THREADING_H
#define N3DS_THREADING_H

#include "util/common.h"

#include <3ds.h>
#include <malloc.h>

#ifdef _3DS
// ctrulib already has a type called Thread
#define Thread CustomThread
#endif

#define THREAD_ENTRY void
typedef ThreadFunc ThreadEntry;

typedef struct {
	Handle handle;
	u8* stack;
} Thread;
typedef Handle Mutex;
typedef struct {
	Mutex mutex;
	Handle semaphore;
	u32 waiting;
} Condition;

static inline int MutexInit(Mutex* mutex) {
	return svcCreateMutex(mutex, false);
}

static inline int MutexDeinit(Mutex* mutex) {
	return svcCloseHandle(*mutex);
}

static inline int MutexLock(Mutex* mutex) {
	return svcWaitSynchronization(*mutex, U64_MAX);
}

static inline int MutexTryLock(Mutex* mutex) {
	return svcWaitSynchronization(*mutex, 10);
}

static inline int MutexUnlock(Mutex* mutex) {
	return svcReleaseMutex(*mutex);
}

static inline int ConditionInit(Condition* cond) {
	Result res = svcCreateMutex(&cond->mutex, false);
	if (res) {
		return res;
	}
	res = svcCreateSemaphore(&cond->semaphore, 0, 1);
	if (res) {
		svcCloseHandle(cond->mutex);
	}
	cond->waiting = 0;
	return res;
}

static inline int ConditionDeinit(Condition* cond) {
	svcCloseHandle(cond->mutex);
	return svcCloseHandle(cond->semaphore);
}

static inline int ConditionWait(Condition* cond, Mutex* mutex) {
	MutexLock(&cond->mutex);
	++cond->waiting;
	MutexUnlock(mutex);
	MutexUnlock(&cond->mutex);
	svcWaitSynchronization(cond->semaphore, U64_MAX);
	MutexLock(mutex);
	return 1;
}

static inline int ConditionWaitTimed(Condition* cond, Mutex* mutex, int32_t timeoutMs) {
	MutexLock(&cond->mutex);
	++cond->waiting;
	MutexUnlock(mutex);
	MutexUnlock(&cond->mutex);
	svcWaitSynchronization(cond->semaphore, timeoutMs * 10000000LL);
	MutexLock(mutex);
	return 1;
}

static inline int ConditionWake(Condition* cond) {
	MutexLock(&cond->mutex);
	if (cond->waiting) {
		--cond->waiting;
		s32 count = 0;
		svcReleaseSemaphore(&count, cond->semaphore, 1);
	}
	MutexUnlock(&cond->mutex);
	return 0;
}

static inline int ThreadCreate(Thread* thread, ThreadEntry entry, void* context) {
	if (!entry || !thread) {
		return 1;
	}
	thread->stack = memalign(8, 0x8000);
	if (!thread->stack) {
		return 1;
	}
	bool isNew3DS;
	APT_CheckNew3DS(&isNew3DS);
	if (isNew3DS && svcCreateThread(&thread->handle, entry, (u32) context, (u32*) &thread->stack[0x8000], 0x18, 2) == 0) {
		return 0;
	}
	return svcCreateThread(&thread->handle, entry, (u32) context, (u32*) &thread->stack[0x8000], 0x18, -1);
}

static inline int ThreadJoin(Thread thread) {
	svcWaitSynchronization(thread.handle, U64_MAX);
	free(thread.stack);
	return 0;
}

static inline void ThreadSetName(const char* name) {
	UNUSED(name);
	// Unimplemented
}

#endif
