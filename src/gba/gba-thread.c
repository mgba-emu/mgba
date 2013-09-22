#include "gba-thread.h"

#include "arm.h"
#include "debugger.h"
#include "gba.h"

#include <stdlib.h>
#include <signal.h>

static void* _GBAThreadRun(void* context) {
	struct ARMDebugger debugger;
	struct GBA gba;
	struct GBAThread* threadContext = context;
	char* savedata = 0;

	sigset_t signals;
	sigfillset(&signals);
	pthread_sigmask(SIG_UNBLOCK, &signals, 0);

	GBAInit(&gba);
	if (threadContext->renderer) {
		GBAVideoAssociateRenderer(&gba.video, threadContext->renderer);
	}

	threadContext->gba = &gba;
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
	if (threadContext->useDebugger) {
		threadContext->debugger = &debugger;
		GBAAttachDebugger(&gba, &debugger);
	} else {
		threadContext->debugger = 0;
	}
	gba.keySource = &threadContext->activeKeys;

	threadContext->started = 1;
	pthread_mutex_lock(&threadContext->mutex);
	pthread_cond_broadcast(&threadContext->cond);
	pthread_mutex_unlock(&threadContext->mutex);

	if (threadContext->useDebugger) {
		ARMDebuggerRun(&debugger);
		threadContext->started = 0;
	} else {
		while (threadContext->started) {
			ARMRun(&gba.cpu);
		}
	}
	GBADeinit(&gba);
	free(savedata);

	return 0;
}

int GBAThreadStart(struct GBAThread* threadContext) {
	// TODO: error check
	{
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
		threadContext->mutex = mutex;
		pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
		threadContext->cond = cond;
	}
	pthread_mutex_init(&threadContext->mutex, 0);
	pthread_cond_init(&threadContext->cond, 0);

	pthread_mutex_lock(&threadContext->mutex);
	threadContext->activeKeys = 0;
	threadContext->started = 0;
	pthread_create(&threadContext->thread, 0, _GBAThreadRun, threadContext);
	pthread_cond_wait(&threadContext->cond, &threadContext->mutex);
	pthread_mutex_unlock(&threadContext->mutex);

	return 0;
}

void GBAThreadJoin(struct GBAThread* threadContext) {
	pthread_join(threadContext->thread, 0);

	pthread_mutex_destroy(&threadContext->mutex);
	pthread_cond_destroy(&threadContext->cond);
}
