/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/supervisor/thread.h"
#include "gba/supervisor/config.h"
#include "gba/gba.h"
#include "gba/renderers/video-software.h"
#include "gba/serialize.h"

#include "platform/commandline.h"
#include "util/string.h"
#include "util/vfs.h"

#include <errno.h>
#include <signal.h>

#define FUZZ_OPTIONS "F:NO:S:V:"
#define FUZZ_USAGE \
	"\nAdditional options:\n" \
	"  -F FRAMES        Run for the specified number of FRAMES before exiting\n" \
	"  -N               Disable video rendering entirely\n" \
	"  -O OFFSET        Offset to apply savestate overlay\n" \
	"  -S FILE          Load a savestate when starting the test\n" \
	"  -V FILE          Overlay a second savestate over the loaded savestate\n" \

struct FuzzOpts {
	bool noVideo;
	unsigned frames;
	size_t overlayOffset;
	char* savestate;
	char* ssOverlay;
};

static void _GBAFuzzRunloop(struct GBAThread* context, unsigned frames);
static void _GBAFuzzShutdown(int signal);
static bool _parseFuzzOpts(struct SubParser* parser, struct GBAConfig* config, int option, const char* arg);
static void _loadSavestate(struct GBAThread* context);

static struct GBAThread* _thread;
static bool _dispatchExiting = false;
static struct VFile* _savestate = 0;
static struct VFile* _savestateOverlay = 0;
static size_t _overlayOffset;

int main(int argc, char** argv) {
	signal(SIGINT, _GBAFuzzShutdown);


	struct FuzzOpts fuzzOpts = { false, 0, 0, 0, 0 };
	struct SubParser subparser = {
		.usage = FUZZ_USAGE,
		.parse = _parseFuzzOpts,
		.extraOptions = FUZZ_OPTIONS,
		.opts = &fuzzOpts
	};

	struct GBAConfig config;
	GBAConfigInit(&config, "fuzz");
	GBAConfigLoad(&config);

	struct GBAOptions opts = {
		.idleOptimization = IDLE_LOOP_DETECT
	};
	GBAConfigLoadDefaults(&config, &opts);

	struct GBAArguments args;
	bool parsed = parseArguments(&args, &config, argc, argv, &subparser);
	if (!parsed || args.showHelp) {
		usage(argv[0], FUZZ_USAGE);
		freeArguments(&args);
		GBAConfigFreeOpts(&opts);
		GBAConfigDeinit(&config);
		return !parsed;
	}

	struct GBAVideoSoftwareRenderer renderer;
	renderer.outputBuffer = 0;

	struct GBAThread context = {};
	_thread = &context;

	if (!fuzzOpts.noVideo) {
		GBAVideoSoftwareRendererCreate(&renderer);
		renderer.outputBuffer = malloc(256 * 256 * 4);
		renderer.outputBufferStride = 256;
		context.renderer = &renderer.d;
	}
	if (fuzzOpts.savestate) {
		_savestate = VFileOpen(fuzzOpts.savestate, O_RDONLY);
		free(fuzzOpts.savestate);
	}
	if (fuzzOpts.ssOverlay) {
		_overlayOffset = fuzzOpts.overlayOffset;
		if (_overlayOffset < sizeof(struct GBASerializedState)) {
			_savestateOverlay = VFileOpen(fuzzOpts.ssOverlay, O_RDONLY);
		}
		free(fuzzOpts.ssOverlay);
	}
	if (_savestate) {
		context.startCallback = _loadSavestate;
	}

	context.debugger = createDebugger(&args, &context);
	context.overrides = GBAConfigGetOverrides(&config);

	GBAConfigMap(&config, &opts);
	opts.audioSync = false;
	opts.videoSync = false;
	GBAMapArgumentsToContext(&args, &context);
	GBAMapOptionsToContext(&opts, &context);

	int didStart = GBAThreadStart(&context);

	if (!didStart) {
		goto cleanup;
	}
	GBAThreadInterrupt(&context);
	if (GBAThreadHasCrashed(&context)) {
		GBAThreadJoin(&context);
		goto cleanup;
	}

	GBAThreadContinue(&context);

	_GBAFuzzRunloop(&context, fuzzOpts.frames);
	GBAThreadJoin(&context);

cleanup:
	if (_savestate) {
		_savestate->close(_savestate);
	}
	if (_savestateOverlay) {
		_savestateOverlay->close(_savestateOverlay);
	}
	GBAConfigFreeOpts(&opts);
	freeArguments(&args);
	GBAConfigDeinit(&config);
	free(context.debugger);
	if (renderer.outputBuffer) {
		free(renderer.outputBuffer);
	}

	return !didStart || GBAThreadHasCrashed(&context);
}

static void _GBAFuzzRunloop(struct GBAThread* context, unsigned duration) {
	unsigned frames = 0;
	while (context->state < THREAD_EXITING) {
		if (GBASyncWaitFrameStart(&context->sync, 0)) {
			++frames;
		}
		GBASyncWaitFrameEnd(&context->sync);
		if (frames >= duration) {
			_GBAFuzzShutdown(0);
		}
		if (_dispatchExiting) {
			GBAThreadEnd(context);
		}
	}
}

static void _GBAFuzzShutdown(int signal) {
	UNUSED(signal);
	// This will come in ON the GBA thread, so we have to handle it carefully
	_dispatchExiting = true;
	ConditionWake(&_thread->sync.videoFrameAvailableCond);
}

static bool _parseFuzzOpts(struct SubParser* parser, struct GBAConfig* config, int option, const char* arg) {
	UNUSED(config);
	struct FuzzOpts* opts = parser->opts;
	errno = 0;
	switch (option) {
	case 'F':
		opts->frames = strtoul(arg, 0, 10);
		return !errno;
	case 'N':
		opts->noVideo = true;
		return true;
	case 'O':
		opts->overlayOffset = strtoul(arg, 0, 10);
		return !errno;
	case 'S':
		opts->savestate = strdup(arg);
		return true;
	case 'V':
		opts->ssOverlay = strdup(arg);
		return true;
	default:
		return false;
	}
}

static void _loadSavestate(struct GBAThread* context) {
	if (!_savestateOverlay) {
		GBALoadStateNamed(context->gba, _savestate);
	} else {
		struct GBASerializedState* state = GBAAllocateState();
		_savestate->read(_savestate, state, sizeof(*state));
		_savestateOverlay->read(_savestateOverlay, (uint8_t*) state + _overlayOffset, sizeof(*state) - _overlayOffset);
		GBADeserialize(context->gba, state);
		GBADeallocateState(state);
		_savestateOverlay->close(_savestateOverlay);
		_savestateOverlay = 0;
	}
	_savestate->close(_savestate);
	_savestate = 0;
}
