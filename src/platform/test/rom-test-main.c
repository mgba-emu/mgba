/* Copyright (c) 2013-2022 Jeffrey Pfau
*  Copyright (c) 2022 Felix Jones
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mgba/core/cheats.h>
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#ifdef M_CORE_GBA
#include <mgba/internal/gba/gba.h>
#endif
#ifdef M_CORE_GB
#include <mgba/internal/sm83/sm83.h>
#endif

#include <mgba/feature/commandline.h>
#include <mgba-util/vfs.h>

#include <errno.h>
#include <signal.h>

#define ROM_TEST_OPTIONS "S:R:"
#define ROM_TEST_USAGE \
	"Additional options:\n" \
	"  -S SWI           Run until specified SWI call before exiting\n" \
	"  -R REGISTER      General purpose register to return as exit code\n" \

struct RomTestOpts {
	int exitSwiImmediate;
	unsigned int returnCodeRegister;
};

static void _romTestShutdown(int signal);
static bool _parseRomTestOpts(struct mSubParser* parser, int option, const char* arg);
static bool _parseSwi(const char* regStr, int* oSwi);
static bool _parseNamedRegister(const char* regStr, unsigned int* oRegister);

static bool _dispatchExiting = false;
static int _exitCode = 0;

#ifdef M_CORE_GBA
static void _romTestSwi3Callback(void* context);

static void _romTestSwi16(struct ARMCore* cpu, int immediate);
static void _romTestSwi32(struct ARMCore* cpu, int immediate);

static int _exitSwiImmediate;
static unsigned int _returnCodeRegister;

void (*_armSwi16)(struct ARMCore* cpu, int immediate);
void (*_armSwi32)(struct ARMCore* cpu, int immediate);
#endif

#ifdef M_CORE_GB
enum GBReg {
	GB_REG_A = 16,
	GB_REG_F,
	GB_REG_B,
	GB_REG_C,
	GB_REG_D,
	GB_REG_E,
	GB_REG_H,
	GB_REG_L
};

static void _romTestGBCallback(void* context);
#endif

int main(int argc, char * argv[]) {
	signal(SIGINT, _romTestShutdown);

	struct RomTestOpts romTestOpts = { 3, 0 };
	struct mSubParser subparser = {
		.usage = ROM_TEST_USAGE,
		.parse = _parseRomTestOpts,
		.extraOptions = ROM_TEST_OPTIONS,
		.opts = &romTestOpts
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
	mCoreInitConfig(core, "romTest");
	mArgumentsApply(&args, NULL, 0, &core->config);

	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "remove");

	bool cleanExit = false;
	struct mCoreCallbacks callbacks = {0};
	callbacks.context = core;
	switch (core->platform(core)) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		((struct GBA*) core->board)->hardCrash = false;
		if (romTestOpts.returnCodeRegister >= 16) {
			goto loadError;
		}

		_exitSwiImmediate = romTestOpts.exitSwiImmediate;
		_returnCodeRegister = romTestOpts.returnCodeRegister;

		if (_exitSwiImmediate == 3) {
			// Hook into SWI 3 (shutdown)
			callbacks.shutdown = _romTestSwi3Callback;
			core->addCoreCallbacks(core, &callbacks);
		} else {
			// Custom SWI hooks
			_armSwi16 = ((struct GBA*) core->board)->cpu->irqh.swi16;
			((struct GBA*) core->board)->cpu->irqh.swi16 = _romTestSwi16;
			_armSwi32 = ((struct GBA*) core->board)->cpu->irqh.swi32;
			((struct GBA*) core->board)->cpu->irqh.swi32 = _romTestSwi32;
		}
		break;
#endif
#ifdef M_CORE_GB
	case mPLATFORM_GB:
		if (romTestOpts.returnCodeRegister < GB_REG_A) {
			goto loadError;
		}

		_returnCodeRegister = romTestOpts.returnCodeRegister;

		callbacks.shutdown = _romTestGBCallback;
		core->addCoreCallbacks(core, &callbacks);
		break;
#endif
	default:
		goto loadError;
	}

	if (!mCoreLoadFile(core, args.fname)) {
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
	cleanExit = true;

loadError:
	mArgumentsDeinit(&args);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);

	return cleanExit ? _exitCode : 1;
}

static void _romTestShutdown(int signal) {
	UNUSED(signal);
	_dispatchExiting = true;
}

#ifdef M_CORE_GBA
static void _romTestSwi3Callback(void* context) {
	struct mCore* core = context;
	_exitCode = ((struct GBA*) core->board)->cpu->regs.gprs[_returnCodeRegister];
	_dispatchExiting = true;
}

static void _romTestSwi16(struct ARMCore* cpu, int immediate) {
	if (immediate == _exitSwiImmediate) {
		_exitCode = cpu->regs.gprs[_returnCodeRegister];
		_dispatchExiting = true;
		return;
	}
	_armSwi16(cpu, immediate);
}

static void _romTestSwi32(struct ARMCore* cpu, int immediate) {
	if (immediate == _exitSwiImmediate) {
		_exitCode = cpu->regs.gprs[_returnCodeRegister];
		_dispatchExiting = true;
		return;
	}
	_armSwi32(cpu, immediate);
}
#endif

#ifdef M_CORE_GB
static void _romTestGBCallback(void* context) {
	struct mCore* core = context;
	struct SM83Core* cpu = core->cpu;

	switch (_returnCodeRegister) {
	case GB_REG_A:
		_exitCode = cpu->a;
		break;
	case GB_REG_B:
		_exitCode = cpu->b;
		break;
	case GB_REG_C:
		_exitCode = cpu->c;
		break;
	case GB_REG_D:
		_exitCode = cpu->d;
		break;
	case GB_REG_E:
		_exitCode = cpu->e;
		break;
	case GB_REG_F:
		_exitCode = cpu->f.packed;
		break;
	case GB_REG_H:
		_exitCode = cpu->h;
		break;
	case GB_REG_L:
		_exitCode = cpu->l;
		break;
	}
	_dispatchExiting = true;
}
#endif

static bool _parseRomTestOpts(struct mSubParser* parser, int option, const char* arg) {
	struct RomTestOpts* opts = parser->opts;
	errno = 0;
	switch (option) {
	case 'S':
		return _parseSwi(arg, &opts->exitSwiImmediate);
	case 'R':
		return _parseNamedRegister(arg, &opts->returnCodeRegister);
	default:
		return false;
	}
}

static bool _parseSwi(const char* swiStr, int* oSwi) {
	char* parseEnd;
	long swi = strtol(swiStr, &parseEnd, 0);
	if (errno || swi > UINT8_MAX || *parseEnd) {
		return false;
	}
	*oSwi = swi;
	return true;
}

static bool _parseNamedRegister(const char* regStr, unsigned int* oRegister) {
#ifdef M_CORE_GB
	static const enum GBReg gbMapping[] = {
		['a' - 'a'] = GB_REG_A,
		['b' - 'a'] = GB_REG_B,
		['c' - 'a'] = GB_REG_C,
		['d' - 'a'] = GB_REG_D,
		['e' - 'a'] = GB_REG_E,
		['f' - 'a'] = GB_REG_F,
		['h' - 'a'] = GB_REG_H,
		['l' - 'a'] = GB_REG_L,
	};
#endif

	switch (regStr[0]) {
	case 'r':
	case 'R':
		++regStr;
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		break;
#ifdef M_CORE_GB
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case 'h':
	case 'l':
		if (regStr[1] != '\0') {
			return false;
		}
		*oRegister = gbMapping[regStr[0] - 'a'];
		break;
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
	case 'H':
	case 'L':
		if (regStr[1] != '\0') {
			return false;
		}
		*oRegister = gbMapping[regStr[0] - 'A'];
		return true;
#endif
	}

	char* parseEnd;
	unsigned long regId = strtoul(regStr, &parseEnd, 10);
	if (errno || regId > 15 || *parseEnd) {
		return false;
	}
	*oRegister = regId;
	return true;
}
