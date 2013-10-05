#ifndef GBA_THREAD_H
#define GBA_THREAD_H

#include <pthread.h>

struct GBAThread {
	// Output
	int started;
	int useDebugger;
	struct GBA* gba;
	struct ARMDebugger* debugger;

	// Input
	struct GBAVideoRenderer* renderer;
	int fd;
	const char* fname;
	int activeKeys;
	int frameskip;

	// Threading state
	pthread_t thread;

	pthread_mutex_t startMutex;
	pthread_cond_t startCond;

	struct GBASync {
		int videoFramePending;
		int videoFrameWait;
		int videoFrameSkip;
		pthread_mutex_t videoFrameMutex;
		pthread_cond_t videoFrameAvailableCond;
		pthread_cond_t videoFrameRequiredCond;
	} sync;
};

int GBAThreadStart(struct GBAThread* threadContext);
void GBAThreadJoin(struct GBAThread* threadContext);
struct GBAThread* GBAThreadGetContext(void);

void GBASyncPostFrame(struct GBASync* sync);
void GBASyncWaitFrameStart(struct GBASync* sync, int frameskip);
void GBASyncWaitFrameEnd(struct GBASync* sync);
int GBASyncDrawingFrame(struct GBASync* sync);

#endif
