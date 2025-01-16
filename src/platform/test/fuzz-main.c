/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/cheats.h>
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/debugger/debugger.h>
#include <mgba/internal/debugger/access-logger.h>
#include <mgba/internal/gba/gba.h>

#include <mgba/feature/commandline.h>
#include <mgba-util/memory.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#include <errno.h>
#include <signal.h>

#define FUZZ_OPTIONS "F:M:NO:S:V:"
#define FUZZ_USAGE \
	"Additional options:\n" \
	"  -F FRAMES        Run for the specified number of FRAMES before exiting\n" \
	"  -N               Disable video rendering entirely\n" \
	"  -O OFFSET        Offset to apply savestate overlay\n" \
	"  -V FILE          Overlay a second savestate over the loaded savestate\n" \
	"  -M FILE          Attach a memory access log file\n" \

struct FuzzOpts {
	bool noVideo;
	int frames;
	size_t overlayOffset;
	char* ssOverlay;
	char* accessLog;
};

static void _fuzzRunloop(struct mCore* core, int frames);
static void _fuzzShutdown(int signal);
static bool _parseFuzzOpts(struct mSubParser* parser, int option, const char* arg);

static bool _dispatchExiting = false;

int main(int argc, char** argv) {
	signal(SIGINT, _fuzzShutdown);

	struct FuzzOpts fuzzOpts = { false, 0, 0, 0 };
	struct mSubParser subparser = {
		.usage = FUZZ_USAGE,
		.parse = _parseFuzzOpts,
		.extraOptions = FUZZ_OPTIONS,
		.opts = &fuzzOpts
	};

	struct mArguments args;
	bool parsed = mArgumentsParse(&args, argc, argv, &subparser, 1);
	if (!args.fname) {
		parsed = false;
	}
	if (!parsed || args.showHelp) {
		usage(argv[0], NULL, NULL, &subparser, 1);
		return !parsed;
	}
	if (args.showVersion) {
		version(argv[0]);
		return 0;
	}
	struct mCore* core = mCoreFind(args.fname);
	if (!core) {
		return 1;
	}
	core->init(core);
	mCoreInitConfig(core, "fuzz");
	mArgumentsApply(&args, NULL, 0, &core->config);

	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "remove");

	void* outputBuffer;
	outputBuffer = 0;

	if (!fuzzOpts.noVideo) {
		outputBuffer = malloc(256 * 256 * 4);
		core->setVideoBuffer(core, outputBuffer, 256);
	}

#ifdef M_CORE_GBA
	if (core->platform(core) == mPLATFORM_GBA) {
		((struct GBA*) core->board)->hardCrash = false;
	}
#endif

#ifdef __AFL_HAVE_MANUAL_CONTROL
	__AFL_INIT();
#endif

	bool cleanExit = true;
	if (!mCoreLoadFile(core, args.fname)) {
		cleanExit = false;
		goto loadError;
	}

	struct VFile* savestate = 0;
	struct VFile* savestateOverlay = 0;
	size_t overlayOffset;

	if (args.savestate) {
		savestate = VFileOpen(args.savestate, O_RDONLY);
	}
	if (fuzzOpts.ssOverlay) {
		overlayOffset = fuzzOpts.overlayOffset;
		if (overlayOffset <= core->stateSize(core)) {
			savestateOverlay = VFileOpen(fuzzOpts.ssOverlay, O_RDONLY);
		}
		free(fuzzOpts.ssOverlay);
	}

	core->reset(core);

	struct mDebugger debugger;
	struct mDebuggerAccessLogger accessLog;
	bool hasDebugger = false;

	mDebuggerInit(&debugger);

	if (fuzzOpts.accessLog) {
		mDebuggerAttach(&debugger, core);

		struct VFile* vf = VFileOpen(fuzzOpts.accessLog, O_RDWR);
		mDebuggerAccessLoggerInit(&accessLog);
		mDebuggerAttachModule(&debugger, &accessLog.d);
		mDebuggerAccessLoggerOpen(&accessLog, vf, O_RDWR);
		mDebuggerAccessLoggerStart(&accessLog);
		hasDebugger = true;
	}

	mArgumentsApplyFileLoads(&args, core);

	if (savestate) {
		if (!savestateOverlay) {
			mCoreLoadStateNamed(core, savestate, SAVESTATE_ALL);
		} else {
			size_t size = savestate->size(savestate);
			void* mapped = savestate->map(savestate, size, MAP_READ);
			struct VFile* newState = VFileMemChunk(mapped, size);
			savestate->unmap(savestate, mapped, size);
			newState->seek(newState, overlayOffset, SEEK_SET);
			uint8_t buffer[2048];
			int read;
			while ((read = savestateOverlay->read(savestateOverlay, buffer, sizeof(buffer))) > 0) {
				newState->write(newState, buffer, read);
			}
			savestateOverlay->close(savestateOverlay);
			savestateOverlay = NULL;
			mCoreLoadStateNamed(core, newState, SAVESTATE_ALL);
			newState->close(newState);
		}
		savestate->close(savestate);
		savestate = NULL;
	}

	_fuzzRunloop(core, fuzzOpts.frames);

	if (hasDebugger) {
		core->detachDebugger(core);
		mDebuggerAccessLoggerDeinit(&accessLog);
		mDebuggerDeinit(&debugger);
	}

	core->unloadROM(core);

	if (savestate) {
		savestate->close(savestate);
	}
	if (savestateOverlay) {
		savestateOverlay->close(savestateOverlay);
	}

loadError:
	mArgumentsDeinit(&args);
	if (outputBuffer) {
		free(outputBuffer);
	}
	mCoreConfigDeinit(&core->config);
	core->deinit(core);

	return !cleanExit;
}

static void _fuzzRunloop(struct mCore* core, int frames) {
	do {
		core->runFrame(core);
		--frames;
		mAudioBufferClear(core->getAudioBuffer(core));
	} while (frames > 0 && !_dispatchExiting);
}

static void _fuzzShutdown(int signal) {
	UNUSED(signal);
	_dispatchExiting = true;
}

static bool _parseFuzzOpts(struct mSubParser* parser, int option, const char* arg) {
	struct FuzzOpts* opts = parser->opts;
	errno = 0;
	switch (option) {
	case 'F':
		opts->frames = strtoul(arg, 0, 10);
		return !errno;
	case 'M':
		opts->accessLog = strdup(arg);
		return true;
	case 'N':
		opts->noVideo = true;
		return true;
	case 'O':
		opts->overlayOffset = strtoul(arg, 0, 10);
		return !errno;
	case 'V':
		opts->ssOverlay = strdup(arg);
		return true;
	default:
		return false;
	}
}
