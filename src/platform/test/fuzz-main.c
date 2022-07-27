/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/blip_buf.h>
#include <mgba/core/cheats.h>
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/gb/core.h>
#include <mgba/gba/core.h>
#include <mgba/internal/gba/gba.h>

#include <mgba/feature/commandline.h>
#include <mgba-util/memory.h>
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#include <errno.h>
#include <signal.h>

#define FUZZ_OPTIONS "F:NO:S:V:"
#define FUZZ_USAGE \
	"Additional options:\n" \
	"  -F FRAMES        Run for the specified number of FRAMES before exiting\n" \
	"  -N               Disable video rendering entirely\n" \
	"  -O OFFSET        Offset to apply savestate overlay\n" \
	"  -V FILE          Overlay a second savestate over the loaded savestate\n" \

struct FuzzOpts {
	bool noVideo;
	int frames;
	size_t overlayOffset;
	char* ssOverlay;
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
	if (args.patch) {
		core->loadPatch(core, VFileOpen(args.patch, O_RDONLY));
	}

	struct VFile* savestate = 0;
	struct VFile* savestateOverlay = 0;
	size_t overlayOffset;

	if (args.savestate) {
		savestate = VFileOpen(args.savestate, O_RDONLY);
	}
	if (fuzzOpts.ssOverlay) {
		overlayOffset = fuzzOpts.overlayOffset;
		if (overlayOffset < core->stateSize(core)) {
			savestateOverlay = VFileOpen(fuzzOpts.ssOverlay, O_RDONLY);
		}
		free(fuzzOpts.ssOverlay);
	}

	core->reset(core);

	struct mCheatDevice* device;
	if (args.cheatsFile && (device = core->cheatDevice(core))) {
		struct VFile* vf = VFileOpen(args.cheatsFile, O_RDONLY);
		if (vf) {
			mCheatDeviceClear(device);
			mCheatParseFile(device, vf);
			vf->close(vf);
		}
	}

	if (savestate) {
		if (!savestateOverlay) {
			mCoreLoadStateNamed(core, savestate, 0);
		} else {
			size_t size = core->stateSize(core);
			uint8_t* state = malloc(size);
			savestate->read(savestate, state, size);
			savestateOverlay->read(savestateOverlay, state + overlayOffset, size - overlayOffset);
			core->loadState(core, state);
			free(state);
			savestateOverlay->close(savestateOverlay);
			savestateOverlay = 0;
		}
		savestate->close(savestate);
		savestate = 0;
	}

	blip_set_rates(core->getAudioChannel(core, 0), GBA_ARM7TDMI_FREQUENCY, 0x8000);
	blip_set_rates(core->getAudioChannel(core, 1), GBA_ARM7TDMI_FREQUENCY, 0x8000);

	_fuzzRunloop(core, fuzzOpts.frames);

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
		blip_clear(core->getAudioChannel(core, 0));
		blip_clear(core->getAudioChannel(core, 1));
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
