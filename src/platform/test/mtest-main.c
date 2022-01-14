/* Copyright (c) 2022 Jeffrey Pfau, Felix Jones
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

#define MTEST_OPTIONS "S:R:"
#define MTEST_USAGE \
	"\nAdditional options:\n" \
	"  -S SWI           Run until specified SWI call before exiting\n" \
	"  -R REGISTER      General purpose register to return as exit code\n" \

struct MTestOpts {
	int exitSwiImmediate;
	int returnCodeRegister;
};

static void _mTestShutdown(int signal);
static bool _parseMTestOpts(struct mSubParser* parser, int option, const char* arg);
static int _parseNamedRegister(const char* regStr);

static bool _dispatchExiting = false;
static int _exitCode = 0;

#ifdef M_CORE_GBA
static void _mTestSwi3Callback(struct mCore* core);

static void _mTestSwi16(struct ARMCore* cpu, int immediate);
static void _mTestSwi32(struct ARMCore* cpu, int immediate);

static int _exitSwiImmediate = -1;
static int _returnCodeRegister;

void (*_armSwi16)(struct ARMCore* cpu, int immediate);
void (*_armSwi32)(struct ARMCore* cpu, int immediate);
#endif

int main(int argc, char * argv[]) {
	signal(SIGINT, _mTestShutdown);

	struct MTestOpts mTestOpts = { 3, -1 };
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

#ifdef M_CORE_GBA
	if (core->platform(core) == mPLATFORM_GBA) {
		((struct GBA*) core->board)->hardCrash = false;

		_exitSwiImmediate = mTestOpts.exitSwiImmediate;
		_returnCodeRegister = mTestOpts.returnCodeRegister;

		if (_exitSwiImmediate == 3) {
			// Hook into SWI 3 (shutdown)
			struct mCoreCallbacks callbacks = {0};
			callbacks.context = core;
			callbacks.shutdown = (void(*)(void*)) _mTestSwi3Callback;
			core->addCoreCallbacks(core, &callbacks);
		} else {
			// Custom SWI hooks
			_armSwi16 = ((struct GBA*) core->board)->cpu->irqh.swi16;
			((struct GBA*) core->board)->cpu->irqh.swi16 = _mTestSwi16;
			_armSwi32 = ((struct GBA*) core->board)->cpu->irqh.swi32;
			((struct GBA*) core->board)->cpu->irqh.swi32 = _mTestSwi32;
		}
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
	}

	do {
		core->runLoop(core);
	} while (!_dispatchExiting);

	core->unloadROM(core);

loadError:
	freeArguments(&args);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);

	return cleanExit ? _exitCode : 1;
}

static void _mTestShutdown(int signal) {
	UNUSED(signal);
	_dispatchExiting = true;
}

#ifdef M_CORE_GBA
static void _mTestSwi3Callback(struct mCore* core) {
	_exitCode = ((struct GBA*) core->board)->cpu->regs.gprs[_returnCodeRegister];
	_dispatchExiting = true;
}

static void _mTestSwi16(struct ARMCore* cpu, int immediate) {
	if (immediate == _exitSwiImmediate) {
		_exitCode = cpu->regs.gprs[_returnCodeRegister];
		_dispatchExiting = true;
		return;
	}
	_armSwi16(cpu, immediate);
}

static void _mTestSwi32(struct ARMCore* cpu, int immediate) {
	if (immediate == _exitSwiImmediate) {
		_exitCode = cpu->regs.gprs[_returnCodeRegister];
		_dispatchExiting = true;
		return;
	}
	_armSwi32(cpu, immediate);
}
#endif

static bool _parseMTestOpts(struct mSubParser* parser, int option, const char* arg) {
	struct MTestOpts* opts = parser->opts;
	errno = 0;
	switch (option) {
	case 'S':
		opts->exitSwiImmediate = strtol(arg, NULL, 0);
		return !errno;
	case 'R':
		opts->returnCodeRegister = _parseNamedRegister(arg);
		return !errno;
	default:
		return false;
	}
}

static int _parseNamedRegister(const char* regStr) {
	if (regStr[0] == 'r' || regStr[0] == 'R') {
		++regStr;
	}
	return strtol(regStr, NULL, 10);
}
