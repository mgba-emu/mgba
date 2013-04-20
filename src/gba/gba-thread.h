#ifndef GBA_THREAD_H
#define GBA_THREAD_H

#include <pthread.h>

struct GBAThread {
	struct GBA* gba;
	struct ARMDebugger* debugger;
	int fd;
};

int GBAThreadStart(struct GBAThread* threadContext, pthread_t* thread);

#endif
