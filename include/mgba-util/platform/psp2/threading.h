/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SCE_THREADING_H
#define SCE_THREADING_H

#include <psp2/kernel/threadmgr.h>

typedef SceUID Thread;
typedef SceUID Mutex;
typedef struct {
	Mutex mutex;
	SceUID semaphore;
	int waiting;
} Condition;
#define THREAD_ENTRY int
typedef THREAD_ENTRY (*ThreadEntry)(void*);

static inline int MutexInit(Mutex* mutex) {
	Mutex id = sceKernelCreateMutex("mutex", 0, 0, 0);
	if (id < 0) {
		return id;
	}
	*mutex = id;
	return 0;
}

static inline int MutexDeinit(Mutex* mutex) {
	return sceKernelDeleteMutex(*mutex);
}

static inline int MutexLock(Mutex* mutex) {
	return sceKernelLockMutex(*mutex, 1, 0);
}

static inline int MutexTryLock(Mutex* mutex) {
	return sceKernelTryLockMutex(*mutex, 1);
}

static inline int MutexUnlock(Mutex* mutex) {
	return sceKernelUnlockMutex(*mutex, 1);
}

static inline int ConditionInit(Condition* cond) {
	int res = MutexInit(&cond->mutex);
	if (res < 0) {
		return res;
	}
	cond->semaphore = sceKernelCreateSema("SceCondSema", 0, 0, 1, 0);
	if (cond->semaphore < 0) {
		MutexDeinit(&cond->mutex);
		res = cond->semaphore;
	}
	cond->waiting = 0;
	return res;
}

static inline int ConditionDeinit(Condition* cond) {
	MutexDeinit(&cond->mutex);
	return sceKernelDeleteSema(cond->semaphore);
}

static inline int ConditionWait(Condition* cond, Mutex* mutex) {
	int ret = MutexLock(&cond->mutex);
	if (ret < 0) {
		return ret;
	}
	++cond->waiting;
	MutexUnlock(mutex);
	MutexUnlock(&cond->mutex);
	ret = sceKernelWaitSema(cond->semaphore, 1, 0);
	if (ret < 0) {
		printf("Premature wakeup: %08X", ret);
	}
	MutexLock(mutex);
	return ret;
}

static inline int ConditionWaitTimed(Condition* cond, Mutex* mutex, int32_t timeoutMs) {
	int ret = MutexLock(&cond->mutex);
	if (ret < 0) {
		return ret;
	}
	++cond->waiting;
	MutexUnlock(mutex);
	MutexUnlock(&cond->mutex);
	SceUInt timeout = 0;
	if (timeoutMs > 0) {
		timeout = timeoutMs;
	}
	ret = sceKernelWaitSema(cond->semaphore, 1, &timeout);
	if (ret < 0) {
		printf("Premature wakeup: %08X", ret);
	}
	MutexLock(mutex);
	return ret;
}

static inline int ConditionWake(Condition* cond) {
	MutexLock(&cond->mutex);
	if (cond->waiting) {
		--cond->waiting;
		sceKernelSignalSema(cond->semaphore, 1);
	}
	MutexUnlock(&cond->mutex);
	return 0;
}

struct SceThreadEntryArgs {
	void* context;
	ThreadEntry entry;
};

static inline int _sceThreadEntry(SceSize args, void* argp) {
	UNUSED(args);
	struct SceThreadEntryArgs* arg = argp;
	return arg->entry(arg->context);
}

static inline int ThreadCreate(Thread* thread, ThreadEntry entry, void* context) {
	Thread id = sceKernelCreateThread("SceThread", _sceThreadEntry, 0x10000100, 0x10000, 0, 0, 0);
	if (id < 0) {
		*thread = 0;
		return id;
	}
	*thread = id;
	struct SceThreadEntryArgs args = { context, entry };
	sceKernelStartThread(id, sizeof(args), &args);
	return 0;
}

static inline int ThreadJoin(Thread* thread) {
	int res = sceKernelWaitThreadEnd(*thread, 0, 0);
	if (res < 0) {
		return res;
	}
	return sceKernelDeleteThread(*thread);
}

static inline int ThreadSetName(const char* name) {
	UNUSED(name);
	return -1;
}

#if (__STDC_VERSION__ < 201112L) || (__STDC_NO_THREADS__ == 1)
typedef int ThreadLocal;

static inline void ThreadLocalInitKey(ThreadLocal* key) {
	static int base = 0x90;
	*key = __atomic_fetch_add(&base, 1, __ATOMIC_SEQ_CST);
}

static inline void ThreadLocalSetKey(ThreadLocal key, void* value) {
	void** tls = sceKernelGetTLSAddr(key);
	*tls = value;
}

static inline void* ThreadLocalGetValue(ThreadLocal key) {
	void** tls = sceKernelGetTLSAddr(key);
	return *tls;
}
#endif
#endif
