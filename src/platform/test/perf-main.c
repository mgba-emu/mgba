/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core/config.h"
#include "gba/core.h"
#include "gba/gba.h"
#include "gba/renderers/video-software.h"
#include "gba/serialize.h"

#include "platform/commandline.h"
#include "util/string.h"
#include "util/vfs.h"

#ifdef _3DS
#include <3ds.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/time.h>

#define PERF_OPTIONS "F:L:NPS:"
#define PERF_USAGE \
	"\nBenchmark options:\n" \
	"  -F FRAMES        Run for the specified number of FRAMES before exiting\n" \
	"  -N               Disable video rendering entirely\n" \
	"  -P               CSV output, useful for parsing\n" \
	"  -S SEC           Run for SEC in-game seconds before exiting\n" \
	"  -L FILE          Load a savestate when starting the test"

struct PerfOpts {
	bool noVideo;
	bool csv;
	unsigned duration;
	unsigned frames;
	char* savestate;
};

#ifdef _3DS
extern bool allocateRomBuffer(void);
FS_Archive sdmcArchive;
#endif

static void _mPerfRunloop(struct mCore* context, int* frames, bool quiet);
static void _mPerfShutdown(int signal);
static bool _parsePerfOpts(struct mSubParser* parser, int option, const char* arg);
static void _log(struct mLogger*, int, enum mLogLevel, const char*, va_list);

static bool _dispatchExiting = false;
static struct VFile* _savestate = 0;

int main(int argc, char** argv) {
#ifdef _3DS
    gfxInitDefault();
    osSetSpeedupEnable(true);
	consoleInit(GFX_BOTTOM, NULL);
	if (!allocateRomBuffer()) {
		return 1;
	}
	sdmcArchive = (FS_Archive) {
		ARCHIVE_SDMC,
		(FS_Path) { PATH_EMPTY, 1, "" },
		0
	};
	FSUSER_OpenArchive(&sdmcArchive);
#else
	signal(SIGINT, _mPerfShutdown);
#endif
	int didFail = 0;

	struct mLogger logger = { .log = _log };
	mLogSetDefaultLogger(&logger);

	struct PerfOpts perfOpts = { false, false, 0, 0, 0 };
	struct mSubParser subparser = {
		.usage = PERF_USAGE,
		.parse = _parsePerfOpts,
		.extraOptions = PERF_OPTIONS,
		.opts = &perfOpts
	};

	struct mArguments args = {};
	bool parsed = parseArguments(&args, argc, argv, &subparser);
	if (!parsed || args.showHelp) {
		usage(argv[0], PERF_USAGE);
		didFail = !parsed;
		goto cleanup;
	}

	if (args.showVersion) {
		version(argv[0]);
		goto cleanup;
	}

	void* outputBuffer = malloc(256 * 256 * 4);

	struct mCore* core = mCoreFind(args.fname);
	if (!core) {
		didFail = 1;
		goto cleanup;
	}

	if (!perfOpts.noVideo) {
		core->setVideoBuffer(core, outputBuffer, 256);
	}
	if (perfOpts.savestate) {
		_savestate = VFileOpen(perfOpts.savestate, O_RDONLY);
		free(perfOpts.savestate);
	}

	// TODO: Put back debugger
	char gameCode[5] = { 0 };

	core->init(core);
	mCoreLoadFile(core, args.fname);
	mCoreConfigInit(&core->config, "perf");
	mCoreConfigLoad(&core->config);

	mCoreConfigSetDefaultIntValue(&core->config, "idleOptimization", IDLE_LOOP_REMOVE);
	struct mCoreOptions opts = {};
	mCoreConfigMap(&core->config, &opts);
	opts.audioSync = false;
	opts.videoSync = false;
	applyArguments(&args, NULL, &core->config);
	mCoreConfigLoadDefaults(&core->config, &opts);
	mCoreLoadConfig(core);

	core->reset(core);
	if (_savestate) {
		core->loadState(core, _savestate, 0);
		_savestate->close(_savestate);
		_savestate = NULL;
	}

	core->getGameCode(core, gameCode);

	int frames = perfOpts.frames;
	if (!frames) {
		frames = perfOpts.duration * 60;
	}
	struct timeval tv;
	gettimeofday(&tv, 0);
	uint64_t start = 1000000LL * tv.tv_sec + tv.tv_usec;
	_mPerfRunloop(core, &frames, perfOpts.csv);
	gettimeofday(&tv, 0);
	uint64_t end = 1000000LL * tv.tv_sec + tv.tv_usec;
	uint64_t duration = end - start;

	core->deinit(core);

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

	mCoreConfigFreeOpts(&opts);
	mCoreConfigDeinit(&core->config);
	free(outputBuffer);

	cleanup:
	freeArguments(&args);

#ifdef _3DS
	gfxExit();
#endif

	return didFail;
}

static void _mPerfRunloop(struct mCore* core, int* frames, bool quiet) {
	struct timeval lastEcho;
	gettimeofday(&lastEcho, 0);
	int duration = *frames;
	*frames = 0;
	int lastFrames = 0;
	while (!_dispatchExiting) {
		core->runFrame(core);
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
		if (duration > 0 && *frames == duration) {
			break;
		}
	}
	if (!quiet) {
		printf("\033[2K\r");
	}
}

static void _mPerfShutdown(int signal) {
	UNUSED(signal);
	_dispatchExiting = true;
}

static bool _parsePerfOpts(struct mSubParser* parser, int option, const char* arg) {
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
	case 'L':
		opts->savestate = strdup(arg);
		return true;
	default:
		return false;
	}
}

static void _log(struct mLogger* log, int category, enum mLogLevel level, const char* format, va_list args) {
	UNUSED(log);
	UNUSED(category);
	UNUSED(level);
	UNUSED(format);
	UNUSED(args);
}
