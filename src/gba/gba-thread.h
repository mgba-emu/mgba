#ifndef GBA_THREAD_H
#define GBA_THREAD_H

#include "common.h"

#include "gba.h"

#include "util/threading.h"
#include "platform/commandline.h"

struct GBAThread;
typedef void (*ThreadCallback)(struct GBAThread* threadContext);

enum ThreadState {
	THREAD_INITIALIZED = -1,
	THREAD_RUNNING = 0,
	THREAD_INTERRUPTED,
	THREAD_INTERRUPTING,
	THREAD_PAUSED,
	THREAD_PAUSING,
	THREAD_EXITING,
	THREAD_SHUTDOWN
};

struct GBASync {
	int videoFramePending;
	int videoFrameWait;
	int videoFrameSkip;
	int videoFrameOn;
	Mutex videoFrameMutex;
	Condition videoFrameAvailableCond;
	Condition videoFrameRequiredCond;

	int audioWait;
	Condition audioRequiredCond;
	Mutex audioBufferMutex;
};

struct GBAThread {
	// Output
	enum ThreadState state;
	struct GBA* gba;
	struct ARMCore* cpu;

	// Input
	struct GBAVideoRenderer* renderer;
	struct GBASIODriverSet sioDrivers;
	struct ARMDebugger* debugger;
	int fd;
	int biosFd;
	int patchFd;
	const char* fname;
	int activeKeys;
	int frameskip;

	// Threading state
	Thread thread;

	Mutex stateMutex;
	Condition stateCond;
	enum ThreadState savedState;

	GBALogHandler logHandler;
	int logLevel;
	ThreadCallback startCallback;
	ThreadCallback cleanCallback;
	ThreadCallback frameCallback;
	void* userData;

	struct GBASync sync;

	int rewindBufferSize;
	int rewindBufferCapacity;
	int rewindBufferInterval;
	int rewindBufferNext;
	struct GBASerializedState** rewindBuffer;
	int rewindBufferWriteOffset;
};

void GBAMapOptionsToContext(struct StartupOptions*, struct GBAThread*);

int GBAThreadStart(struct GBAThread* threadContext);
int GBAThreadHasStarted(struct GBAThread* threadContext);
void GBAThreadEnd(struct GBAThread* threadContext);
void GBAThreadJoin(struct GBAThread* threadContext);

void GBAThreadInterrupt(struct GBAThread* threadContext);
void GBAThreadContinue(struct GBAThread* threadContext);

void GBAThreadPause(struct GBAThread* threadContext);
void GBAThreadUnpause(struct GBAThread* threadContext);
int GBAThreadIsPaused(struct GBAThread* threadContext);
void GBAThreadTogglePause(struct GBAThread* threadContext);
struct GBAThread* GBAThreadGetContext(void);

void GBASyncPostFrame(struct GBASync* sync);
int GBASyncWaitFrameStart(struct GBASync* sync, int frameskip);
void GBASyncWaitFrameEnd(struct GBASync* sync);
int GBASyncDrawingFrame(struct GBASync* sync);

void GBASyncProduceAudio(struct GBASync* sync, int wait);
void GBASyncLockAudio(struct GBASync* sync);
void GBASyncConsumeAudio(struct GBASync* sync);

#endif
