#include "gba-thread.h"
#include "gba-config.h"
#include "gba.h"
#include "renderers/video-software.h"

#include "platform/commandline.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/time.h>

#define PERF_OPTIONS "F:NPS:"
#define PERF_USAGE \
	"\nBenchmark options:\n" \
	"  -F FRAMES        Run for the specified number of FRAMES before exiting\n" \
	"  -N               Disable video rendering entirely\n" \
	"  -P               CSV output, useful for parsing\n" \
	"  -S SEC           Run for SEC in-game seconds before exiting"

struct PerfOpts {
	bool noVideo;
	bool csv;
	unsigned duration;
	unsigned frames;
};

static void _GBAPerfRunloop(struct GBAThread* context, int* frames, bool quiet);
static void _GBAPerfShutdown(int signal);
static bool _parsePerfOpts(struct SubParser* parser, struct GBAOptions* gbaOpts, int option, const char* arg);

static struct GBAThread* _thread;

int main(int argc, char** argv) {
	signal(SIGINT, _GBAPerfShutdown);

	struct GBAVideoSoftwareRenderer renderer;
	GBAVideoSoftwareRendererCreate(&renderer);

	struct PerfOpts perfOpts = { false, false, 0, 0 };
	struct SubParser subparser = {
		.usage = PERF_USAGE,
		.parse = _parsePerfOpts,
		.extraOptions = PERF_OPTIONS,
		.opts = &perfOpts
	};

	struct GBAOptions opts = {};
	struct GBAArguments args = {};
	if (!parseArguments(&args, &opts, argc, argv, &subparser)) {
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

	context.debugger = createDebugger(&args);
	char gameCode[5] = { 0 };

	GBAMapArgumentsToContext(&args, &context);
	GBAMapOptionsToContext(&opts, &context);

	GBAThreadStart(&context);
	GBAGetGameCode(context.gba, gameCode);

	int frames = perfOpts.frames;
	if (!frames) {
		frames = perfOpts.duration * 60;
	}
	struct timeval tv;
	gettimeofday(&tv, 0);
	uint64_t start = 1000000LL * tv.tv_sec + tv.tv_usec;
	_GBAPerfRunloop(&context, &frames, perfOpts.csv);
	gettimeofday(&tv, 0);
	uint64_t end = 1000000LL * tv.tv_sec + tv.tv_usec;
	uint64_t duration = end - start;

	GBAThreadJoin(&context);
	GBAConfigFreeOpts(&opts);
	freeArguments(&args);
	free(context.debugger);

	free(renderer.outputBuffer);

	float scaledFrames = frames * 1000000.f;
	if (perfOpts.csv) {
		puts("game_code,frames,duration,renderer");
		const char* rendererName;
		if (perfOpts.noVideo) {
			rendererName = "none";
		} else {
			rendererName = "software";
		}
		printf("%s,%i,%" PRIu64 ",%s\n", gameCode, frames, duration, rendererName);
	} else {
		printf("%u frames in %" PRIu64 " microseconds: %g fps (%gx)\n", frames, duration, scaledFrames / duration, scaledFrames / (duration * 60.f));
	}

	return 0;
}

static void _GBAPerfRunloop(struct GBAThread* context, int* frames, bool quiet) {
	struct timeval lastEcho;
	gettimeofday(&lastEcho, 0);
	int duration = *frames;
	*frames = 0;
	int lastFrames = 0;
	while (context->state < THREAD_EXITING) {
		if (GBASyncWaitFrameStart(&context->sync, 0)) {
			++*frames;
			++lastFrames;
			if (!quiet) {
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
		}
		GBASyncWaitFrameEnd(&context->sync);
		if (*frames == duration) {
			_GBAPerfShutdown(0);
		}
	}
	if (!quiet) {
		printf("\033[2K\r");
	}
}

static void _GBAPerfShutdown(int signal) {
	UNUSED(signal);
	pthread_mutex_lock(&_thread->stateMutex);
	_thread->state = THREAD_EXITING;
	pthread_mutex_unlock(&_thread->stateMutex);
}

static bool _parsePerfOpts(struct SubParser* parser, struct GBAOptions* gbaOpts, int option, const char* arg) {
	UNUSED(gbaOpts);
	struct PerfOpts* opts = parser->opts;
	errno = 0;
	switch (option) {
	case 'F':
		opts->frames = strtoul(arg, 0, 10);
		return !errno;
	case 'N':
		opts->noVideo = true;
		return true;
	case 'P':
		opts->csv = true;
		return true;
	case 'S':
		opts->duration = strtoul(arg, 0, 10);
		return !errno;
	default:
		return false;
	}
}
