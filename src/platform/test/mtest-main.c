/* Copyright (c) 2022 Felix Jones
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mgba/core/cheats.h>
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gba/gba.h>

#include <mgba/feature/commandline.h>
#include <mgba-util/vfs.h>

#include <signal.h>

static const char * rom_path = "C:\\Users\\Administrator\\OneDrive - Microsoft\\gba-toolchain-3\\samples\\7 Overlays\\cmake-build-debug-mingw\\helloOverlay.gba";

#define MTEST_OPTIONS "F:N"
#define MTEST_USAGE \
	"\nAdditional options:\n" \
	"  -F FRAMES        Run for the specified number of FRAMES before exiting\n" \
	"  -N               Disable video rendering entirely\n" \

struct MTestOpts {
	unsigned long frames;
	bool noVideo;
};

static int _mTestRunloop(struct mCore* core, unsigned long frames);
static void _mTestShutdown(int signal);
static bool _parseMTestOpts(struct mSubParser* parser, int option, const char* arg);

static bool _dispatchExiting = false;

int main(int argc, char * argv[]) {
	signal(SIGINT, _mTestShutdown);

	struct MTestOpts mTestOpts = { 0 };
	struct mSubParser subparser = {
		.usage = MTEST_USAGE,
		.parse = _parseMTestOpts,
		.extraOptions = MTEST_OPTIONS,
		.opts = &mTestOpts
	};

	struct mArguments args;
	bool parsed = parseArguments(&args, argc, argv, &subparser);
	if (!args.fname) {
		parsed = false;
	}
	if (!parsed || args.showHelp) {
		usage(argv[0], MTEST_USAGE);
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
	mCoreInitConfig(core, "mTest");
	applyArguments(&args, NULL, &core->config);

	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "remove");

	void* outputBuffer;
	outputBuffer = 0;

	if (!mTestOpts.noVideo) {
		outputBuffer = malloc(256 * 256 * 4);
		core->setVideoBuffer(core, outputBuffer, 256);
	}

#ifdef M_CORE_GBA
	if (core->platform(core) == mPLATFORM_GBA) {
		((struct GBA*) core->board)->hardCrash = false;
	}
#endif

	bool cleanExit = true;
	if (!mCoreLoadFile(core, args.fname)) {
		cleanExit = false;
		goto loadError;
	}
	if (args.patch) {
		core->loadPatch(core, VFileOpen(args.patch, O_RDONLY));
	}

	struct VFile* savestate = NULL;

	if (args.savestate) {
		savestate = VFileOpen(args.savestate, O_RDONLY);
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
		mCoreLoadStateNamed(core, savestate, 0);
		savestate->close(savestate);
		savestate = 0;
	}

	_mTestRunloop(core, mTestOpts.frames);

	core->unloadROM(core);

	if (savestate) {
		savestate->close(savestate);
	}

loadError:
	freeArguments(&args);
	if (outputBuffer) {
		free(outputBuffer);
	}
	mCoreConfigDeinit(&core->config);
	core->deinit(core);

	return !cleanExit; // TODO : Return code from ROM
}

static int _mTestRunloop(struct mCore* core, unsigned long frames) {
	do {
		core->runFrame(core);
		--frames;
	} while (frames > 0 && !_dispatchExiting);
	return 0;
}

static void _mTestShutdown(int signal) {
	UNUSED(signal);
	_dispatchExiting = true;
}

static bool _parseMTestOpts(struct mSubParser* parser, int option, const char* arg) {
	struct MTestOpts* opts = parser->opts;
	errno = 0;
	switch (option) {
	case 'F':
		opts->frames = strtoul(arg, 0, 10);
		return !errno;
	case 'N':
		opts->noVideo = true;
		return true;
	default:
		return false;
	}
}
