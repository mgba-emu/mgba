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
#ifdef HAVE_PTHREAD_NP_H
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

static inline int ThreadJoin(Thread* thread) {
	return pthread_join(*thread, 0);
}

static inline int ThreadSetName(const char* name) {
#if defined(__APPLE__) && defined(HAVE_PTHREAD_SETNAME_NP)
	return pthread_setname_np(name);
#elif defined(HAVE_PTHREAD_SET_NAME_NP)
	pthread_set_name_np(pthread_self(), name);
	return 0;
#elif defined(__HAIKU__)
	rename_thread(find_thread(NULL), name);
	return 0;
#elif defined(__NetBSD__)
	return pthread_setname_np(pthread_self(), "%s", (void *) name);
#elif defined(HAVE_PTHREAD_SETNAME_NP)
	return pthread_setname_np(pthread_self(), name);
#else
	UNUSED(name);
	return 0;
#endif
}

#if (__STDC_VERSION__ < 201112L) || (__STDC_NO_THREADS__ == 1)
typedef pthread_key_t ThreadLocal;

static inline void ThreadLocalInitKey(ThreadLocal* key) {
	pthread_key_create(key, 0);
}

static inline void ThreadLocalSetKey(ThreadLocal key, void* value) {
	pthread_setspecific(key, value);
}

static inline void* ThreadLocalGetValue(ThreadLocal key) {
	return pthread_getspecific(key);
}
#endif

CXX_GUARD_END

#endif
