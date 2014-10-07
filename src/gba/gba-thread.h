#ifndef GBA_THREAD_H
#define GBA_THREAD_H

#include "common.h"

#include "gba.h"
#include "gba-input.h"

#include "util/threading.h"
#include "platform/commandline.h"

struct GBAThread;
typedef void (*ThreadCallback)(struct GBAThread* threadContext);
typedef void (*LogHandler)(struct GBAThread*, enum GBALogLevel, const char* format, va_list args);

enum ThreadState {
	THREAD_INITIALIZED = -1,
	THREAD_RUNNING = 0,
	THREAD_INTERRUPTED,
	THREAD_INTERRUPTING,
	THREAD_PAUSED,
	THREAD_PAUSING,
	THREAD_RESETING,
	THREAD_EXITING,
	THREAD_SHUTDOWN
};

struct GBASync {
	int videoFramePending;
	int videoFrameWait;
	int videoFrameSkip;
	bool videoFrameOn;
	Mutex videoFrameMutex;
	Condition videoFrameAvailableCond;
	Condition videoFrameRequiredCond;

	int audioWait;
	Condition audioRequiredCond;
	Mutex audioBufferMutex;
};

struct GBAAVStream {
	void (*postVideoFrame)(struct GBAAVStream*, struct GBAVideoRenderer* renderer);
	void (*postAudioFrame)(struct GBAAVStream*, int32_t left, int32_t right);
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
	struct VDir* gameDir;
	struct VDir* stateDir;
	struct VFile* rom;
	struct VFile* save;
	struct VFile* bios;
	struct VFile* patch;
	const char* fname;
	int activeKeys;
	struct GBAInputMap inputMap;
	struct GBAAVStream* stream;

	// Run-time options
	int frameskip;
	float fpsTarget;
	size_t audioBuffers;

	// Threading state
	Thread thread;

	Mutex stateMutex;
	Condition stateCond;
	enum ThreadState savedState;

	LogHandler logHandler;
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

bool GBAThreadStart(struct GBAThread* threadContext);
bool GBAThreadHasStarted(struct GBAThread* threadContext);
void GBAThreadEnd(struct GBAThread* threadContext);
void GBAThreadReset(struct GBAThread* threadContext);
void GBAThreadJoin(struct GBAThread* threadContext);

void GBAThreadInterrupt(struct GBAThread* threadContext);
void GBAThreadContinue(struct GBAThread* threadContext);

void GBAThreadPause(struct GBAThread* threadContext);
void GBAThreadUnpause(struct GBAThread* threadContext);
bool GBAThreadIsPaused(struct GBAThread* threadContext);
void GBAThreadTogglePause(struct GBAThread* threadContext);
void GBAThreadPauseFromThread(struct GBAThread* threadContext);
struct GBAThread* GBAThreadGetContext(void);

#ifdef USE_PNG
void GBAThreadTakeScreenshot(struct GBAThread* threadContext);
#endif

void GBASyncPostFrame(struct GBASync* sync);
bool GBASyncWaitFrameStart(struct GBASync* sync, int frameskip);
void GBASyncWaitFrameEnd(struct GBASync* sync);
bool GBASyncDrawingFrame(struct GBASync* sync);

void GBASyncProduceAudio(struct GBASync* sync, int wait);
void GBASyncLockAudio(struct GBASync* sync);
void GBASyncUnlockAudio(struct GBASync* sync);
void GBASyncConsumeAudio(struct GBASync* sync);

#endif
