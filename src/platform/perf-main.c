#include "gba-thread.h"
#include "gba.h"
#include "renderers/video-software.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>

#define PERF_OPTIONS "NS:"
#define PERF_USAGE \
	"\nBenchmark options:\n" \
	"  -N               Disable video rendering entirely" \
	"  -S SEC           Run for SEC in-game seconds before exiting"

struct PerfOpts {
	bool noVideo;
	int duration;
};

static void _GBAPerfRunloop(struct GBAThread* context, int* frames);
static void _GBAPerfShutdown(int signal);
static bool _parsePerfOpts(struct SubParser* parser, int option, const char* arg);

static struct GBAThread* _thread;

int main(int argc, char** argv) {
	signal(SIGINT, _GBAPerfShutdown);

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	struct PerfOpts perfOpts = { false, 0 };
	struct SubParser subparser = {
		.usage = PERF_USAGE,
		.parse = _parsePerfOpts,
		.extraOptions = PERF_OPTIONS,
		.opts = &perfOpts
	};

	struct StartupOptions opts;
	if (!parseCommandArgs(&opts, argc, argv, &subparser)) {
		usage(argv[0], PERF_USAGE);
		return 1;
	}

	renderer.outputBuffer = malloc(256 * 256 * 4);
	renderer.outputBufferStride = 256;

	struct GBAThread context = {
		.sync.videoFrameWait = 0,
		.sync.audioWait = 0
	};
	_thread = &context;

	if (!perfOpts.noVideo) {
		context.renderer = &renderer.d;
	}

	context.debugger = createDebugger(&opts);

	GBAMapOptionsToContext(&opts, &context);

	GBAThreadStart(&context);

	int frames = perfOpts.duration;
	time_t start = time(0);
	_GBAPerfRunloop(&context, &frames);
	time_t end = time(0);
	int duration = end - start;

	GBAThreadJoin(&context);
	freeOptions(&opts);
	free(context.debugger);

	free(renderer.outputBuffer);

	printf("%u frames in %i seconds: %g fps (%gx)\n", frames, duration, frames / (float) duration, frames / (duration * 60.f));

	return 0;
}

static void _GBAPerfRunloop(struct GBAThread* context, int* frames) {
	struct timeval lastEcho;
	gettimeofday(&lastEcho, 0);
	int duration = *frames;
	*frames = 0;
	int lastFrames = 0;
	while (context->state < THREAD_EXITING) {
		if (GBASyncWaitFrameStart(&context->sync, 0)) {
			++*frames;
			++lastFrames;
			struct timeval currentTime;
			long timeDiff;
			gettimeofday(&currentTime, 0);
			timeDiff = currentTime.tv_sec - lastEcho.tv_sec;
			timeDiff *= 1000;
			timeDiff += (currentTime.tv_usec - lastEcho.tv_usec) / 1000;
			if (timeDiff >= 1000) {
				printf("\033[2K\rCurrent FPS: %g (%gx)", lastFrames / (timeDiff / 1000.0f), lastFrames / (float) (60 * (timeDiff / 1000.0f)));
				fflush(stdout);
				lastEcho = currentTime;
				lastFrames = 0;
			}
		}
		GBASyncWaitFrameEnd(&context->sync);
		if (*frames == duration * 60) {
			_GBAPerfShutdown(0);
		}
	}
	printf("\033[2K\r");
}

static void _GBAPerfShutdown(int signal) {
	UNUSED(signal);
	pthread_mutex_lock(&_thread->stateMutex);
	_thread->state = THREAD_EXITING;
	pthread_mutex_unlock(&_thread->stateMutex);
}

static bool _parsePerfOpts(struct SubParser* parser, int option, const char* arg) {
	struct PerfOpts* opts = parser->opts;
	switch (option) {
	case 'N':
		opts->noVideo = true;
		return true;
	case 'S':
		opts->duration = strtol(arg, 0, 10);
		return !errno;
	default:
		return false;
	}
}
