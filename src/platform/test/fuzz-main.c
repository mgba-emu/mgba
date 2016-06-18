/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core/config.h"
#include "core/core.h"
#include "gba/core.h"
#include "gba/gba.h"
#include "gba/serialize.h"

#include "feature/commandline.h"
#include "util/memory.h"
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
	int frames;
	size_t overlayOffset;
	char* savestate;
	char* ssOverlay;
};

static void _GBAFuzzRunloop(struct mCore* core, int frames);
static void _GBAFuzzShutdown(int signal);
static bool _parseFuzzOpts(struct mSubParser* parser, int option, const char* arg);

static bool _dispatchExiting = false;

int main(int argc, char** argv) {
	signal(SIGINT, _GBAFuzzShutdown);

	struct FuzzOpts fuzzOpts = { false, 0, 0, 0, 0 };
	struct mSubParser subparser = {
		.usage = FUZZ_USAGE,
		.parse = _parseFuzzOpts,
		.extraOptions = FUZZ_OPTIONS,
		.opts = &fuzzOpts
	};

	struct mCore* core = GBACoreCreate();
	core->init(core);
	mCoreInitConfig(core, "fuzz");
	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "remove");

	struct mArguments args;
	bool parsed = parseArguments(&args, argc, argv, &subparser);
	if (!parsed || args.showHelp) {
		usage(argv[0], FUZZ_USAGE);
		core->deinit(core);
		return !parsed;
	}
	if (args.showVersion) {
		version(argv[0]);
		core->deinit(core);
		return 0;
	}
	applyArguments(&args, NULL, &core->config);

	void* outputBuffer;
	outputBuffer = 0;

	if (!fuzzOpts.noVideo) {
		outputBuffer = malloc(256 * 256 * 4);
		core->setVideoBuffer(core, outputBuffer, 256);
	}

#ifdef __AFL_HAVE_MANUAL_CONTROL
	__AFL_INIT();
#endif

	((struct GBA*) core->board)->hardCrash = false;
	mCoreLoadFile(core, args.fname);

	struct VFile* savestate = 0;
	struct VFile* savestateOverlay = 0;
	size_t overlayOffset;

	if (fuzzOpts.savestate) {
		savestate = VFileOpen(fuzzOpts.savestate, O_RDONLY);
		free(fuzzOpts.savestate);
	}
	if (fuzzOpts.ssOverlay) {
		overlayOffset = fuzzOpts.overlayOffset;
		if (overlayOffset < sizeof(struct GBASerializedState)) {
			savestateOverlay = VFileOpen(fuzzOpts.ssOverlay, O_RDONLY);
		}
		free(fuzzOpts.ssOverlay);
	}
	if (savestate) {
		if (!savestateOverlay) {
			mCoreLoadStateNamed(core, savestate, 0);
		} else {
			struct GBASerializedState* state = GBAAllocateState();
			savestate->read(savestate, state, sizeof(*state));
			savestateOverlay->read(savestateOverlay, (uint8_t*) state + overlayOffset, sizeof(*state) - overlayOffset);
			GBADeserialize(core->board, state);
			GBADeallocateState(state);
			savestateOverlay->close(savestateOverlay);
			savestateOverlay = 0;
		}
		savestate->close(savestate);
		savestate = 0;
	}

	blip_set_rates(core->getAudioChannel(core, 0), GBA_ARM7TDMI_FREQUENCY, 0x8000);
	blip_set_rates(core->getAudioChannel(core, 1), GBA_ARM7TDMI_FREQUENCY, 0x8000);

	core->reset(core);

	_GBAFuzzRunloop(core, fuzzOpts.frames);

	core->unloadROM(core);

	if (savestate) {
		savestate->close(savestate);
	}
	if (savestateOverlay) {
		savestateOverlay->close(savestateOverlay);
	}

	freeArguments(&args);
	if (outputBuffer) {
		free(outputBuffer);
	}
	core->deinit(core);

	return 0;
}

static void _GBAFuzzRunloop(struct mCore* core, int frames) {
	do {
		core->runFrame(core);
		blip_clear(core->getAudioChannel(core, 0));
		blip_clear(core->getAudioChannel(core, 1));
	} while (core->frameCounter(core) < frames && !_dispatchExiting);
}

static void _GBAFuzzShutdown(int signal) {
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
