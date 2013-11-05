#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-software.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void _GBAPerfRunloop(struct GBAThread* context, int* frames);
static void _GBAPerfShutdown(int signal);

static struct GBAThread* _thread;

int main(int argc, char** argv) {
	const char* fname = "test.rom";
	if (argc > 1) {
		fname = argv[1];
	}
	int fd = open(fname, O_RDONLY);
	if (fd < 0) {
		return 1;
	}

	signal(SIGINT, _GBAPerfShutdown);

	struct GBAThread context;
	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	renderer.outputBuffer = malloc(256 * 256 * 4);
	renderer.outputBufferStride = 256;

	context.fd = fd;
	context.fname = fname;
	context.useDebugger = 0;
	context.renderer = &renderer.d;
	context.frameskip = 0;
	context.sync.videoFrameWait = 0;
	context.sync.audioWait = 0;
	context.startCallback = 0;
	context.cleanCallback = 0;
	_thread = &context;
	GBAThreadStart(&context);

	int frames = 0;
	time_t start = time(0);
	_GBAPerfRunloop(&context, &frames);
	time_t end = time(0);
	int duration = end - start;

	GBAThreadJoin(&context);
	close(fd);

	free(renderer.outputBuffer);

	printf("%u frames in %i seconds: %g fps (%gx)\n", frames, duration, frames / (float) duration, frames / (duration * 60.f));

	return 0;
}

static void _GBAPerfRunloop(struct GBAThread* context, int* frames) {
	struct timespec lastEcho;
	clock_gettime(CLOCK_REALTIME, &lastEcho);
	int lastFrames = 0;
	while (context->state < THREAD_EXITING) {
		if (GBASyncWaitFrameStart(&context->sync, 0)) {
			++*frames;
			++lastFrames;
			struct timespec currentTime;
			long timeDiff;
			clock_gettime(CLOCK_REALTIME, &currentTime);
			timeDiff = currentTime.tv_sec - lastEcho.tv_sec;
			timeDiff *= 1000;
			timeDiff += (currentTime.tv_nsec - lastEcho.tv_nsec) / 1000000;
			if (timeDiff >= 1000) {
				printf("\033[2K\rCurrent FPS: %g (%gx)", lastFrames / (timeDiff / 1000.0f), lastFrames / (float) (60 * (timeDiff / 1000.0f)));
				fflush(stdout);
				lastEcho = currentTime;
				lastFrames = 0;
			}
		}
		GBASyncWaitFrameEnd(&context->sync);
	}
}

static void _GBAPerfShutdown(int signal) {
	pthread_mutex_lock(&_thread->stateMutex);
	_thread->state = THREAD_EXITING;
	pthread_mutex_unlock(&_thread->stateMutex);
}
