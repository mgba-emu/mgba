/* Copyright (c) 2013-2022 Jeffrey Pfau
*  Copyright (c) 2022 Felix Jones
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <mgba/core/cheats.h>
#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
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
	char* returnCodeRegister;
};

static void _romTestShutdown(int signal);
static bool _parseRomTestOpts(struct mSubParser* parser, int option, const char* arg);
static bool _parseSwi(const char* regStr, int* oSwi);

static bool _romTestCheckResiger(void);

static struct mCore* core;

static bool _dispatchExiting = false;
static int _exitCode = 0;
static struct mStandardLogger _logger;

static void _romTestCallback(void* context);
#ifdef M_CORE_GBA
static void _romTestSwi16(struct ARMCore* cpu, int immediate);
static void _romTestSwi32(struct ARMCore* cpu, int immediate);

static int _exitSwiImmediate;
static char* _returnCodeRegister;

void (*_armSwi16)(struct ARMCore* cpu, int immediate);
void (*_armSwi32)(struct ARMCore* cpu, int immediate);
#endif

int main(int argc, char * argv[]) {
	signal(SIGINT, _romTestShutdown);

	struct RomTestOpts romTestOpts = { 3, NULL };
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
	core = mCoreFind(args.fname);
	if (!core) {
		return 1;
	}
	core->init(core);
	mCoreInitConfig(core, "romTest");
	mArgumentsApply(&args, NULL, 0, &core->config);

	mCoreConfigSetDefaultValue(&core->config, "idleOptimization", "remove");
	mCoreConfigSetDefaultIntValue(&core->config, "logToStdout", true);

	mStandardLoggerInit(&_logger);
	mStandardLoggerConfig(&_logger, &core->config);
	mLogSetDefaultLogger(&_logger.d);

	bool cleanExit = false;
	struct mCoreCallbacks callbacks = {0};

	_returnCodeRegister = romTestOpts.returnCodeRegister;
	if (!_romTestCheckResiger()) {
		goto loadError;
	}

	switch (core->platform(core)) {
#ifdef M_CORE_GBA
	case mPLATFORM_GBA:
		((struct GBA*) core->board)->hardCrash = false;
		_exitSwiImmediate = romTestOpts.exitSwiImmediate;

		if (_exitSwiImmediate == 3) {
			// Hook into SWI 3 (shutdown)
			callbacks.shutdown = _romTestCallback;
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
		callbacks.shutdown = _romTestCallback;
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
	mStandardLoggerDeinit(&_logger);
	mCoreConfigDeinit(&core->config);
	core->deinit(core);
	if (_returnCodeRegister) {
		free(_returnCodeRegister);
	}

	return cleanExit ? _exitCode : 1;
}

static void _romTestShutdown(int signal) {
	UNUSED(signal);
	_dispatchExiting = true;
}

static bool _romTestCheckResiger(void) {
	if (!_returnCodeRegister) {
		return true;
	}

	const struct mCoreRegisterInfo* registers;
	const struct mCoreRegisterInfo* reg = NULL;
	size_t regCount = core->listRegisters(core, &registers);
	size_t i;
	for (i = 0; i < regCount; ++i) {
		if (strcasecmp(_returnCodeRegister, registers[i].name) == 0) {
			reg = &registers[i];
			break;
		}
		if (registers[i].aliases) {
			size_t j;
			for (j = 0; registers[i].aliases[j]; ++j) {
				if (strcasecmp(_returnCodeRegister, registers[i].aliases[j]) == 0) {
					reg = &registers[i];
					break;
				}
			}
			if (reg) {
				break;
			}
		}
	}
	if (!reg) {
		return false;
	}

	if (reg->width > 4) {
		return false;
	}

	if (reg->type != mCORE_REGISTER_GPR) {
		return false;
	}

	if (reg->mask != 0xFFFFFFFFU >> (4 - reg->width) * 8) {
		return false;
	}

	return true;
}

static void _romTestCallback(void* context) {
	UNUSED(context);
	if (_returnCodeRegister) {
		core->readRegister(core, _returnCodeRegister, &_exitCode);
	}
	_dispatchExiting = true;
}

#ifdef M_CORE_GBA
static void _romTestSwi16(struct ARMCore* cpu, int immediate) {
	if (immediate == _exitSwiImmediate) {
		if (_returnCodeRegister) {
			core->readRegister(core, _returnCodeRegister, &_exitCode);
		}
		_dispatchExiting = true;
		return;
	}
	_armSwi16(cpu, immediate);
}

static void _romTestSwi32(struct ARMCore* cpu, int immediate) {
	if (immediate == _exitSwiImmediate) {
		if (_returnCodeRegister) {
			core->readRegister(core, _returnCodeRegister, &_exitCode);
		}
		_dispatchExiting = true;
		return;
	}
	_armSwi32(cpu, immediate);
}
#endif

static bool _parseRomTestOpts(struct mSubParser* parser, int option, const char* arg) {
	struct RomTestOpts* opts = parser->opts;
	errno = 0;
	switch (option) {
	case 'S':
		return _parseSwi(arg, &opts->exitSwiImmediate);
	case 'R':
		opts->returnCodeRegister = strdup(arg);
		return true;
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
