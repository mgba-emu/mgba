/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/context/config.h"
#include "gba/context/context.h"
#include "gba/gba.h"
#include "gba/renderers/video-software.h"
#include "gba/serialize.h"

#include "platform/commandline.h"
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

static void _GBAFuzzRunloop(struct GBAContext* context, int frames);
static void _GBAFuzzShutdown(int signal);
static bool _parseFuzzOpts(struct SubParser* parser, struct GBAConfig* config, int option, const char* arg);

static bool _dispatchExiting = false;

int main(int argc, char** argv) {
	signal(SIGINT, _GBAFuzzShutdown);

	struct FuzzOpts fuzzOpts = { false, 0, 0, 0, 0 };
	struct SubParser subparser = {
		.usage = FUZZ_USAGE,
		.parse = _parseFuzzOpts,
		.extraOptions = FUZZ_OPTIONS,
		.opts = &fuzzOpts
	};

	struct GBAContext context;
	GBAContextInit(&context, "fuzz");
	struct GBAOptions opts = {
		.idleOptimization = IDLE_LOOP_DETECT
	};
	GBAConfigLoadDefaults(&context.config, &opts);
	GBAConfigFreeOpts(&opts);

	struct GBAArguments args;
	bool parsed = parseArguments(&args, &context.config, argc, argv, &subparser);
	if (!parsed || args.showHelp) {
		usage(argv[0], FUZZ_USAGE);
		freeArguments(&args);
		GBAContextDeinit(&context);
		return !parsed;
	}

	struct VFile* rom = VFileOpen(args.fname, O_RDONLY);

	context.gba->hardCrash = false;
	GBAContextLoadROMFromVFile(&context, rom, 0);

	struct GBAVideoSoftwareRenderer renderer;
	renderer.outputBuffer = 0;

	struct VFile* savestate = 0;
	struct VFile* savestateOverlay = 0;
	size_t overlayOffset;

	if (!fuzzOpts.noVideo) {
		GBAVideoSoftwareRendererCreate(&renderer);
		renderer.outputBuffer = malloc(256 * 256 * 4);
		renderer.outputBufferStride = 256;
		GBAVideoAssociateRenderer(&context.gba->video, &renderer.d);
	}

	GBAContextStart(&context);

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
			GBALoadStateNamed(context.gba, savestate);
		} else {
			struct GBASerializedState* state = GBAAllocateState();
			savestate->read(savestate, state, sizeof(*state));
			savestateOverlay->read(savestateOverlay, (uint8_t*) state + overlayOffset, sizeof(*state) - overlayOffset);
			GBADeserialize(context.gba, state);
			GBADeallocateState(state);
			savestateOverlay->close(savestateOverlay);
			savestateOverlay = 0;
		}
		savestate->close(savestate);
		savestate = 0;
	}

	blip_set_rates(context.gba->audio.left, GBA_ARM7TDMI_FREQUENCY, 0x8000);
	blip_set_rates(context.gba->audio.right, GBA_ARM7TDMI_FREQUENCY, 0x8000);

	_GBAFuzzRunloop(&context, fuzzOpts.frames);

	if (savestate) {
		savestate->close(savestate);
	}
	if (savestateOverlay) {
		savestateOverlay->close(savestateOverlay);
	}
	GBAContextStop(&context);
	GBAContextDeinit(&context);
	freeArguments(&args);
	if (renderer.outputBuffer) {
		free(renderer.outputBuffer);
	}

	return 0;
}

static void _GBAFuzzRunloop(struct GBAContext* context, int frames) {
	do {
		GBAContextFrame(context, 0);
	} while (context->gba->video.frameCounter < frames && !_dispatchExiting);
}

static void _GBAFuzzShutdown(int signal) {
	UNUSED(signal);
	_dispatchExiting = true;
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
