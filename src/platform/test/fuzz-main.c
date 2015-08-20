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

static void _GBAFuzzRunloop(struct GBA* gba, int frames);
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

	struct GBA* gba = anonymousMemoryMap(sizeof(struct GBA));
	struct ARMCore* cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	struct VFile* rom = VFileOpen(args.fname, O_RDONLY);

	GBACreate(gba);
	ARMSetComponents(cpu, &gba->d, 0, 0);
	ARMInit(cpu);
	gba->sync = 0;
	gba->hardCrash = false;

	GBALoadROM(gba, rom, 0, 0);
	ARMReset(cpu);

	struct GBACartridgeOverride override;
	const struct GBACartridge* cart = (const struct GBACartridge*) gba->memory.rom;
	memcpy(override.id, &cart->id, sizeof(override.id));
	if (GBAOverrideFind(GBAConfigGetOverrides(&config), &override)) {
		GBAOverrideApply(gba, &override);
	}

	struct GBAVideoSoftwareRenderer renderer;
	renderer.outputBuffer = 0;

	struct VFile* savestate = 0;
	struct VFile* savestateOverlay = 0;
	size_t overlayOffset;

	if (!fuzzOpts.noVideo) {
		GBAVideoSoftwareRendererCreate(&renderer);
		renderer.outputBuffer = malloc(256 * 256 * 4);
		renderer.outputBufferStride = 256;
		GBAVideoAssociateRenderer(&gba->video, &renderer.d);
	}

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
			GBALoadStateNamed(gba, savestate);
		} else {
			struct GBASerializedState* state = GBAAllocateState();
			savestate->read(savestate, state, sizeof(*state));
			savestateOverlay->read(savestateOverlay, (uint8_t*) state + overlayOffset, sizeof(*state) - overlayOffset);
			GBADeserialize(gba, state);
			GBADeallocateState(state);
			savestateOverlay->close(savestateOverlay);
			savestateOverlay = 0;
		}
		savestate->close(savestate);
		savestate = 0;
	}

	GBAConfigMap(&config, &opts);

	blip_set_rates(gba->audio.left, GBA_ARM7TDMI_FREQUENCY, 0x8000);
	blip_set_rates(gba->audio.right, GBA_ARM7TDMI_FREQUENCY, 0x8000);

	_GBAFuzzRunloop(gba, fuzzOpts.frames);

	if (savestate) {
		savestate->close(savestate);
	}
	if (savestateOverlay) {
		savestateOverlay->close(savestateOverlay);
	}
	GBAConfigFreeOpts(&opts);
	freeArguments(&args);
	GBAConfigDeinit(&config);
	if (renderer.outputBuffer) {
		free(renderer.outputBuffer);
	}

	return 0;
}

static void _GBAFuzzRunloop(struct GBA* gba, int frames) {
	do {
		ARMRunLoop(gba->cpu);
	} while (gba->video.frameCounter < frames && !_dispatchExiting);
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
