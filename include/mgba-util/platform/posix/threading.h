/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef POSIX_THREADING_H
#define POSIX_THREADING_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <pthread.h>
#include <sys/time.h>
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#elif defined(__HAIKU__)
#include <OS.h>
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

static inline int MutexTryLock(Mutex* mutex) {
	return pthread_mutex_trylock(mutex);
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
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060
	return pthread_setname_np(name);
#else
	UNUSED(name);
	return 0;
#endif
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
	pthread_set_name_np(pthread_self(), name);
	return 0;
#elif defined(__HAIKU__)
	rename_thread(find_thread(NULL), name);
	return 0;
#elif !defined(BUILD_PANDORA) // Pandora's glibc is too old
	return pthread_setname_np(pthread_self(), name);
#else
	UNUSED(name);
	return 0;
#endif
}

CXX_GUARD_END

#endif
