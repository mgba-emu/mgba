/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef THREADING_H
#define THREADING_H

#include "util/common.h"

#ifndef DISABLE_THREADING
#ifdef USE_PTHREADS
#include <pthread.h>
#include <sys/time.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif

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

static inline int ConditionWaitTimed(Condition* cond, Mutex* mutex, int32_t timeoutMs) {
	struct timespec ts;
	struct timeval tv;

	gettimeofday(&tv, 0);
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = (tv.tv_usec + timeoutMs * 1000L) * 1000L;
	if (ts.tv_nsec >= 1000000000L) {
		ts.tv_nsec -= 1000000000L;
		++ts.tv_sec;
	}

	return pthread_cond_timedwait(cond, mutex, &ts);
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

static inline int ThreadSetName(const char* name) {
#ifdef __APPLE__
	return pthread_setname_np(name);
#elif defined(__FreeBSD__)
	pthread_set_name_np(pthread_self(), name);
	return 0;
#else
	return pthread_setname_np(pthread_self(), name);
#endif
}

#elif _WIN32
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

static inline int ConditionWaitTimed(Condition* cond, Mutex* mutex, int32_t timeoutMs) {
	SleepConditionVariableCS(cond, mutex, timeoutMs);
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

static inline int ThreadSetName(const char* name) {
	UNUSED(name);
	return -1;
}
#else
#define DISABLE_THREADING
#endif
#endif
#ifdef DISABLE_THREADING
typedef void* Thread;
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

#endif
