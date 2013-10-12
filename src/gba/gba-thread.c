#include "gba-thread.h"

#include "arm.h"
#include "debugger.h"
#include "gba.h"

#include <stdlib.h>
#include <signal.h>

static pthread_key_t contextKey;

static void _createTLS(void) {
	pthread_key_create(&contextKey, 0);
}

static void* _GBAThreadRun(void* context) {
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, _createTLS);

#ifdef USE_DEBUGGER
	struct ARMDebugger debugger;
#endif
	struct GBA gba;
	struct GBAThread* threadContext = context;
	char* savedata = 0;

	sigset_t signals;
	sigfillset(&signals);
	pthread_sigmask(SIG_UNBLOCK, &signals, 0);

	GBAInit(&gba);
	threadContext->gba = &gba;
	gba.sync = &threadContext->sync;
	pthread_setspecific(contextKey, threadContext);
	if (threadContext->renderer) {
		GBAVideoAssociateRenderer(&gba.video, threadContext->renderer);
	}

	if (threadContext->fd >= 0) {
		if (threadContext->fname) {
			char* dotPoint = strrchr(threadContext->fname, '.');
			if (dotPoint > strrchr(threadContext->fname, '/') && dotPoint[1] && dotPoint[2] && dotPoint[3]) {
				savedata = strdup(threadContext->fname);
				dotPoint = strrchr(savedata, '.');
				dotPoint[1] = 's';
				dotPoint[2] = 'a';
				dotPoint[3] = 'v';
				dotPoint[4] = '\0';
			} else if (dotPoint) {
				savedata = malloc((dotPoint - threadContext->fname + 5) * sizeof(char));
				strncpy(savedata, threadContext->fname, dotPoint - threadContext->fname + 1);
				strcat(savedata, "sav");
			} else {
				savedata = malloc(strlen(threadContext->fname + 5));
				strcpy(savedata, threadContext->fname);
				strcat(savedata, "sav");
			}
		}
		GBALoadROM(&gba, threadContext->fd, threadContext->fname);
		gba.savefile = savedata;
	}

#ifdef USE_DEBUGGER
	if (threadContext->useDebugger) {
		threadContext->debugger = &debugger;
		GBAAttachDebugger(&gba, &debugger);
	} else {
		threadContext->debugger = 0;
	}
#else
	threadContext->debugger = 0;
#endif

	gba.keySource = &threadContext->activeKeys;

	if (threadContext->startCallback) {
		threadContext->startCallback(threadContext);
	}

	threadContext->started = 1;
	pthread_mutex_lock(&threadContext->startMutex);
	pthread_cond_broadcast(&threadContext->startCond);
	pthread_mutex_unlock(&threadContext->startMutex);

#ifdef USE_DEBUGGER
	if (threadContext->useDebugger) {
		ARMDebuggerRun(&debugger);
		threadContext->started = 0;
	} else {
#endif
		while (threadContext->started) {
			ARMRun(&gba.cpu);
		}
#ifdef USE_DEBUGGER
	}
#endif

	if (threadContext->cleanCallback) {
		threadContext->cleanCallback(threadContext);
	}

	GBADeinit(&gba);

	pthread_cond_broadcast(&threadContext->sync.videoFrameAvailableCond);
	pthread_cond_broadcast(&threadContext->sync.audioRequiredCond);
	free(savedata);

	return 0;
}

int GBAThreadStart(struct GBAThread* threadContext) {
	// TODO: error check
	pthread_mutex_init(&threadContext->startMutex, 0);
	pthread_cond_init(&threadContext->startCond, 0);

	pthread_mutex_init(&threadContext->sync.videoFrameMutex, 0);
	pthread_cond_init(&threadContext->sync.videoFrameAvailableCond, 0);
	pthread_cond_init(&threadContext->sync.videoFrameRequiredCond, 0);
	pthread_cond_init(&threadContext->sync.audioRequiredCond, 0);

	pthread_mutex_lock(&threadContext->startMutex);
	threadContext->activeKeys = 0;
	threadContext->started = 0;
	pthread_create(&threadContext->thread, 0, _GBAThreadRun, threadContext);
	pthread_cond_wait(&threadContext->startCond, &threadContext->startMutex);
	pthread_mutex_unlock(&threadContext->startMutex);

	return 0;
}

void GBAThreadJoin(struct GBAThread* threadContext) {
	pthread_mutex_lock(&threadContext->sync.videoFrameMutex);
	threadContext->sync.videoFrameWait = 0;
	pthread_cond_broadcast(&threadContext->sync.videoFrameRequiredCond);
	pthread_mutex_unlock(&threadContext->sync.videoFrameMutex);

	pthread_join(threadContext->thread, 0);

	pthread_mutex_destroy(&threadContext->startMutex);
	pthread_cond_destroy(&threadContext->startCond);

	pthread_mutex_destroy(&threadContext->sync.videoFrameMutex);
	pthread_cond_broadcast(&threadContext->sync.videoFrameAvailableCond);
	pthread_cond_destroy(&threadContext->sync.videoFrameAvailableCond);
	pthread_cond_broadcast(&threadContext->sync.videoFrameRequiredCond);
	pthread_cond_destroy(&threadContext->sync.videoFrameRequiredCond);

	pthread_cond_broadcast(&threadContext->sync.audioRequiredCond);
	pthread_cond_destroy(&threadContext->sync.audioRequiredCond);
}

struct GBAThread* GBAThreadGetContext(void) {
	return pthread_getspecific(contextKey);
}

void GBASyncPostFrame(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	pthread_mutex_lock(&sync->videoFrameMutex);
	++sync->videoFramePending;
	--sync->videoFrameSkip;
	pthread_cond_broadcast(&sync->videoFrameAvailableCond);
	if (sync->videoFrameWait) {
		pthread_cond_wait(&sync->videoFrameRequiredCond, &sync->videoFrameMutex);
	}
	pthread_mutex_unlock(&sync->videoFrameMutex);
}

void GBASyncWaitFrameStart(struct GBASync* sync, int frameskip) {
	if (!sync) {
		return;
	}

	pthread_mutex_lock(&sync->videoFrameMutex);
	pthread_cond_broadcast(&sync->videoFrameRequiredCond);
	pthread_cond_wait(&sync->videoFrameAvailableCond, &sync->videoFrameMutex);
	sync->videoFramePending = 0;
	sync->videoFrameSkip = frameskip;
}

void GBASyncWaitFrameEnd(struct GBASync* sync) {
	if (!sync) {
		return;
	}

	pthread_mutex_unlock(&sync->videoFrameMutex);
}

int GBASyncDrawingFrame(struct GBASync* sync) {
	return sync->videoFrameSkip <= 0;
}

void GBASyncProduceAudio(struct GBASync* sync, pthread_mutex_t* mutex) {
	if (&sync->audioWait) {
		pthread_cond_wait(&sync->audioRequiredCond, mutex);
	}
}

void GBASyncConsumeAudio(struct GBASync* sync) {
	pthread_cond_broadcast(&sync->audioRequiredCond);
}
