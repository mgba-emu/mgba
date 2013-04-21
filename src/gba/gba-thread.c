#include "gba-thread.h"

#include "arm.h"
#include "debugger.h"
#include "gba.h"

#include <signal.h>

static void* _GBAThreadRun(void* context) {
	struct ARMDebugger debugger;
	struct GBA gba;
	struct GBAThread* threadContext = context;

	sigset_t signals;
	sigfillset(&signals);
	pthread_sigmask(SIG_UNBLOCK, &signals, 0);

	GBAInit(&gba);
	if (threadContext->renderer) {
		GBAVideoAssociateRenderer(&gba.video, threadContext->renderer);
	}

	threadContext->gba = &gba;
	threadContext->debugger = &debugger;
	if (threadContext->fd >= 0) {
		GBALoadROM(&gba, threadContext->fd);
	}
	GBAAttachDebugger(&gba, &debugger);

	threadContext->started = 1;
	pthread_mutex_lock(&threadContext->mutex);
	pthread_cond_broadcast(&threadContext->cond);
	pthread_mutex_unlock(&threadContext->mutex);

	ARMDebuggerRun(&debugger);
	threadContext->started = 0;
	GBADeinit(&gba);

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
	threadContext->started = 0;
	pthread_create(&threadContext->thread, 0, _GBAThreadRun, threadContext);
	pthread_cond_wait(&threadContext->cond, &threadContext->mutex);
	pthread_mutex_unlock(&threadContext->mutex);

	return 0;
}

void GBAThreadJoin(struct GBAThread* threadContext) {
	pthread_join(threadContext->thread, 0);
}
