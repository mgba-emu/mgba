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
	ARMDebuggerRun(&debugger);
	GBADeinit(&gba);

	return 0;
}

int GBAThreadStart(struct GBAThread* threadContext, pthread_t* thread) {
	return pthread_create(thread, 0, _GBAThreadRun, threadContext);
}
