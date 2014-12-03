/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef THREADING_H
#define THREADING_H

#include "util/common.h"

#ifdef USE_PTHREADS
#include <pthread.h>

#define THREAD_ENTRY void*
typedef THREAD_ENTRY (*ThreadEntry)(void*);

typedef pthread_t Thread;
typedef pthread_mutex_t Mutex;
typedef pthread_cond_t Condition;

static inline int MutexInit(Mutex* mutex) {
	return pthread_mutex_init(mutex, 0);
}

static inline int MutexDeinit(Mutex* mutex) {
	return pthread_mutex_destroy(mutex);
}

static inline int MutexLock(Mutex* mutex) {
	return pthread_mutex_lock(mutex);
}

static inline int MutexUnlock(Mutex* mutex) {
	return pthread_mutex_unlock(mutex);
}

static inline int ConditionInit(Condition* cond) {
	return pthread_cond_init(cond, 0);
}

static inline int ConditionDeinit(Condition* cond) {
	return pthread_cond_destroy(cond);
}

static inline int ConditionWait(Condition* cond, Mutex* mutex) {
	return pthread_cond_wait(cond, mutex);
}

static inline int ConditionWake(Condition* cond) {
	return pthread_cond_broadcast(cond);
}

static inline int ThreadCreate(Thread* thread, ThreadEntry entry, void* context) {
	return pthread_create(thread, 0, entry, context);
}

static inline int ThreadJoin(Thread thread) {
	return pthread_join(thread, 0);
}

#else
#define _WIN32_WINNT 0x0600
#include <windows.h>
#define THREAD_ENTRY DWORD WINAPI
typedef THREAD_ENTRY ThreadEntry(LPVOID);

typedef HANDLE Thread;
typedef CRITICAL_SECTION Mutex;
typedef CONDITION_VARIABLE Condition;

static inline int MutexInit(Mutex* mutex) {
	InitializeCriticalSection(mutex);
	return GetLastError();
}

static inline int MutexDeinit(Mutex* mutex) {
	DeleteCriticalSection(mutex);
	return GetLastError();
}

static inline int MutexLock(Mutex* mutex) {
	EnterCriticalSection(mutex);
	return GetLastError();
}

static inline int MutexUnlock(Mutex* mutex) {
	LeaveCriticalSection(mutex);
	return GetLastError();
}

static inline int ConditionInit(Condition* cond) {
	InitializeConditionVariable(cond);
	return GetLastError();
}

static inline int ConditionDeinit(Condition* cond) {
	// This is a no-op on Windows
	UNUSED(cond);
	return 0;
}

static inline int ConditionWait(Condition* cond, Mutex* mutex) {
	SleepConditionVariableCS(cond, mutex, INFINITE);
	return GetLastError();
}

static inline int ConditionWake(Condition* cond) {
	WakeAllConditionVariable(cond);
	return GetLastError();
}

static inline int ThreadCreate(Thread* thread, ThreadEntry entry, void* context) {
	*thread = CreateThread(NULL, 0, entry, context, 0, 0);
	return GetLastError();
}

static inline int ThreadJoin(Thread thread) {
	DWORD error = WaitForSingleObject(thread, INFINITE);
	if (error == WAIT_FAILED) {
		return GetLastError();
	}
	return 0;
}
#endif

#endif
