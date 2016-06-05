/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core.h"

#include "core/cheats.h"
#include "core/core.h"
#include "core/log.h"
#include "arm/debugger/debugger.h"
#include "ds/ds.h"
#include "ds/extra/cli.h"
#include "util/memory.h"
#include "util/patch.h"
#include "util/vfs.h"

#define SLICE_CYCLES 2048

struct DSCore {
	struct mCore d;
	struct ARMCore* arm7;
	struct ARMCore* arm9;
	int keys;
	struct mCPUComponent* components[CPU_COMPONENT_MAX];
	struct mDebuggerPlatform* debuggerPlatform;
	struct mCheatDevice* cheatDevice;
	int32_t cycleDrift;
};

static bool _DSCoreInit(struct mCore* core) {
	struct DSCore* dscore = (struct DSCore*) core;

	struct ARMCore* arm7 = anonymousMemoryMap(sizeof(struct ARMCore));
	struct ARMCore* arm9 = anonymousMemoryMap(sizeof(struct ARMCore));
	struct DS* ds = anonymousMemoryMap(sizeof(struct DS));
	if (!arm7 || !arm9 || !ds) {
		free(arm7);
		free(arm9);
		free(ds);
		return false;
	}
	core->cpu = arm9;
	core->board = ds;
	core->debugger = NULL;
	dscore->arm7 = arm7;
	dscore->arm9 = arm9;
	dscore->debuggerPlatform = NULL;
	dscore->cheatDevice = NULL;
	dscore->cycleDrift = 0;

	DSCreate(ds);
	memset(dscore->components, 0, sizeof(dscore->components));
	ARMSetComponents(arm7, &ds->d, CPU_COMPONENT_MAX, dscore->components);
	ARMSetComponents(arm9, &ds->d, CPU_COMPONENT_MAX, dscore->components);
	ARMInit(arm7);
	ARMInit(arm9);

	dscore->keys = 0;
	ds->keySource = &dscore->keys;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetInit(&core->dirs);
#endif
	
	return true;
}

static void _DSCoreDeinit(struct mCore* core) {
	struct DSCore* dscore = (struct DSCore*) core;
	ARMDeinit(dscore->arm7);
	ARMDeinit(dscore->arm9);
	DSDestroy(core->board);
	mappedMemoryFree(dscore->arm7, sizeof(struct ARMCore));
	mappedMemoryFree(dscore->arm9, sizeof(struct ARMCore));
	mappedMemoryFree(core->board, sizeof(struct DS));
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetDeinit(&core->dirs);
#endif

	free(dscore->debuggerPlatform);
	if (dscore->cheatDevice) {
		mCheatDeviceDestroy(dscore->cheatDevice);
	}
	free(dscore->cheatDevice);
	free(core);
}

static enum mPlatform _DSCorePlatform(struct mCore* core) {
	UNUSED(core);
	return PLATFORM_DS;
}

static void _DSCoreSetSync(struct mCore* core, struct mCoreSync* sync) {
	struct DS* ds = core->board;
	ds->sync = sync;
}

static void _DSCoreLoadConfig(struct mCore* core, const struct mCoreConfig* config) {
	struct DS* ds = core->board;
	struct VFile* bios = NULL;
	if (core->opts.useBios && core->opts.bios) {
		bios = VFileOpen(core->opts.bios, O_RDONLY);
	}
	if (bios) {
		DSLoadBIOS(ds, bios);
	}
}

static void _DSCoreDesiredVideoDimensions(struct mCore* core, unsigned* width, unsigned* height) {
	UNUSED(core);
	*width = DS_VIDEO_HORIZONTAL_PIXELS;
	*height = DS_VIDEO_VERTICAL_PIXELS;
}

static void _DSCoreSetVideoBuffer(struct mCore* core, color_t* buffer, size_t stride) {
}

static void _DSCoreGetVideoBuffer(struct mCore* core, color_t** buffer, size_t* stride) {
}

static struct blip_t* _DSCoreGetAudioChannel(struct mCore* core, int ch) {
	return NULL;
}

static void _DSCoreSetAudioBufferSize(struct mCore* core, size_t samples) {
}

static size_t _DSCoreGetAudioBufferSize(struct mCore* core) {
	return 2048;
}

static void _DSCoreSetAVStream(struct mCore* core, struct mAVStream* stream) {
}

static bool _DSCoreLoadROM(struct mCore* core, struct VFile* vf) {
	return DSLoadROM(core->board, vf);
}

static bool _DSCoreLoadBIOS(struct mCore* core, struct VFile* vf, int type) {
	UNUSED(type);
	return DSLoadBIOS(core->board, vf);
}

static bool _DSCoreLoadSave(struct mCore* core, struct VFile* vf) {
	return false;
}

static bool _DSCoreLoadPatch(struct mCore* core, struct VFile* vf) {
	return false;
}

static void _DSCoreUnloadROM(struct mCore* core) {
	return DSUnloadROM(core->board);
}

static void _DSCoreReset(struct mCore* core) {
	struct DSCore* dscore = (struct DSCore*) core;
	struct DS* ds = (struct DS*) core->board;
	ARMReset(ds->arm7);
	ARMReset(ds->arm9);
	dscore->cycleDrift = 0;
}

static void _DSCoreRunFrame(struct mCore* core) {
	struct DSCore* dscore = (struct DSCore*) core;
	struct DS* ds = core->board;
	int32_t frameCounter = ds->video.frameCounter;
	while (ds->video.frameCounter == frameCounter) {
		if (dscore->cycleDrift < SLICE_CYCLES) {
			dscore->cycleDrift += ARMv5RunCycles(dscore->arm9, SLICE_CYCLES);
		}
		if (dscore->cycleDrift >= SLICE_CYCLES) {
			dscore->cycleDrift -= ARMv4RunCycles(dscore->arm7, dscore->cycleDrift >> 1) << 1;
		}
	}
}

static void _DSCoreRunLoop(struct mCore* core) {
	struct DSCore* dscore = (struct DSCore*) core;
	if (dscore->cycleDrift < SLICE_CYCLES) {
		dscore->cycleDrift += ARMv5RunCycles(dscore->arm9, SLICE_CYCLES);
	}
	if (dscore->cycleDrift >= SLICE_CYCLES) {
		dscore->cycleDrift -= ARMv4RunCycles(dscore->arm7, dscore->cycleDrift >> 1) << 1;
	}
}

static void _DSCoreStep(struct mCore* core) {
	struct DSCore* dscore = (struct DSCore*) core;
	if (core->cpu == dscore->arm9) {
		do {
			if (dscore->cycleDrift >= SLICE_CYCLES) {
				dscore->cycleDrift -= ARMv4RunCycles(dscore->arm7, dscore->cycleDrift >> 1) << 1;
			}
			if (dscore->cycleDrift < SLICE_CYCLES) {
				dscore->cycleDrift += ARMv5RunCycles(dscore->arm9, 1);
				break;
			}
		} while (dscore->cycleDrift >= SLICE_CYCLES);
	} else {
		do {
			if (dscore->cycleDrift < SLICE_CYCLES) {
				dscore->cycleDrift += ARMv5RunCycles(dscore->arm9, SLICE_CYCLES - dscore->cycleDrift);
			}
			if (dscore->cycleDrift >= SLICE_CYCLES) {
				dscore->cycleDrift -= ARMv4RunCycles(dscore->arm7, 1) << 1;
				break;
			}
		} while (dscore->cycleDrift < SLICE_CYCLES);
	}
}

static size_t _DSCoreStateSize(struct mCore* core) {
	UNUSED(core);
	return 0;
}

static bool _DSCoreLoadState(struct mCore* core, const void* state) {
	return false;
}

static bool _DSCoreSaveState(struct mCore* core, void* state) {
	return false;
}

static void _DSCoreSetKeys(struct mCore* core, uint32_t keys) {
	struct DSCore* dscore = (struct DSCore*) core;
	dscore->keys = keys;
}

static void _DSCoreAddKeys(struct mCore* core, uint32_t keys) {
	struct DSCore* dscore = (struct DSCore*) core;
	dscore->keys |= keys;
}

static void _DSCoreClearKeys(struct mCore* core, uint32_t keys) {
	struct DSCore* dscore = (struct DSCore*) core;
	dscore->keys &= ~keys;
}

static int32_t _DSCoreFrameCounter(struct mCore* core) {
	struct DS* ds = core->board;
	return ds->video.frameCounter;
}

static int32_t _DSCoreFrameCycles(struct mCore* core) {
	UNUSED(core);
	return DS_VIDEO_TOTAL_LENGTH;
}

static int32_t _DSCoreFrequency(struct mCore* core) {
	UNUSED(core);
	return DS_ARM946ES_FREQUENCY;
}

static void _DSCoreGetGameTitle(struct mCore* core, char* title) {
	DSGetGameTitle(core->board, title);
}

static void _DSCoreGetGameCode(struct mCore* core, char* title) {
	DSGetGameCode(core->board, title);
}

static void _DSCoreSetRTC(struct mCore* core, struct mRTCSource* rtc) {
	struct DS* ds = core->board;
	ds->rtcSource = rtc;
}

static void _DSCoreSetRotation(struct mCore* core, struct mRotationSource* rotation) {
}

static void _DSCoreSetRumble(struct mCore* core, struct mRumble* rumble) {
	struct DS* ds = core->board;
	ds->rumble = rumble;
}

static uint32_t _DSCoreBusRead8(struct mCore* core, uint32_t address) {
	struct ARMCore* cpu = core->cpu;
	return cpu->memory.load8(cpu, address, 0);
}

static uint32_t _DSCoreBusRead16(struct mCore* core, uint32_t address) {
	struct ARMCore* cpu = core->cpu;
	return cpu->memory.load16(cpu, address, 0);

}

static uint32_t _DSCoreBusRead32(struct mCore* core, uint32_t address) {
	struct ARMCore* cpu = core->cpu;
	return cpu->memory.load32(cpu, address, 0);
}

static void _DSCoreBusWrite8(struct mCore* core, uint32_t address, uint8_t value) {
	struct ARMCore* cpu = core->cpu;
	cpu->memory.store8(cpu, address, value, 0);
}

static void _DSCoreBusWrite16(struct mCore* core, uint32_t address, uint16_t value) {
	struct ARMCore* cpu = core->cpu;
	cpu->memory.store16(cpu, address, value, 0);
}

static void _DSCoreBusWrite32(struct mCore* core, uint32_t address, uint32_t value) {
	struct ARMCore* cpu = core->cpu;
	cpu->memory.store32(cpu, address, value, 0);
}

static uint32_t _DSCoreRawRead8(struct mCore* core, uint32_t address) {
	return 0;
}

static uint32_t _DSCoreRawRead16(struct mCore* core, uint32_t address) {
	return 0;
}

static uint32_t _DSCoreRawRead32(struct mCore* core, uint32_t address) {
	return 0;
}

static void _DSCoreRawWrite8(struct mCore* core, uint32_t address, uint8_t value) {
}

static void _DSCoreRawWrite16(struct mCore* core, uint32_t address, uint16_t value) {
}

static void _DSCoreRawWrite32(struct mCore* core, uint32_t address, uint32_t value) {
}

static bool _DSCoreSupportsDebuggerType(struct mCore* core, enum mDebuggerType type) {
	UNUSED(core);
	switch (type) {
#ifdef USE_CLI_DEBUGGER
	case DEBUGGER_CLI:
		return true;
#endif
#ifdef USE_GDB_STUB
	case DEBUGGER_GDB:
		return true;
#endif
	default:
		return false;
	}
}

static struct mDebuggerPlatform* _DSCoreDebuggerPlatform(struct mCore* core) {
	struct DSCore* dscore = (struct DSCore*) core;
	if (!dscore->debuggerPlatform) {
		dscore->debuggerPlatform = ARMDebuggerPlatformCreate();
	}
	return dscore->debuggerPlatform;
}

static struct CLIDebuggerSystem* _DSCoreCliDebuggerSystem(struct mCore* core) {
#ifdef USE_CLI_DEBUGGER
	return &DSCLIDebuggerCreate(core)->d;
#else
	UNUSED(core);
	return NULL;
#endif
}

static void _DSCoreAttachDebugger(struct mCore* core, struct mDebugger* debugger) {
	if (core->debugger) {
		DSDetachDebugger(core->board);
	}
	DSAttachDebugger(core->board, debugger);
	core->debugger = debugger;
}

static void _DSCoreDetachDebugger(struct mCore* core) {
	DSDetachDebugger(core->board);
	core->debugger = NULL;
}

static struct mCheatDevice* _DSCoreCheatDevice(struct mCore* core) {
	return NULL;
}

static size_t _DSCoreSavedataClone(struct mCore* core, void** sram) {
	return 0;
}

static bool _DSCoreSavedataLoad(struct mCore* core, const void* sram, size_t size) {
	return false;
}

struct mCore* DSCoreCreate(void) {
	struct DSCore* dscore = malloc(sizeof(*dscore));
	struct mCore* core = &dscore->d;
	memset(&core->opts, 0, sizeof(core->opts));
	core->cpu = NULL;
	core->board = NULL;
	core->debugger = NULL;
	core->init = _DSCoreInit;
	core->deinit = _DSCoreDeinit;
	core->platform = _DSCorePlatform;
	core->setSync = _DSCoreSetSync;
	core->loadConfig = _DSCoreLoadConfig;
	core->desiredVideoDimensions = _DSCoreDesiredVideoDimensions;
	core->setVideoBuffer = _DSCoreSetVideoBuffer;
	core->getVideoBuffer = _DSCoreGetVideoBuffer;
	core->getAudioChannel = _DSCoreGetAudioChannel;
	core->setAudioBufferSize = _DSCoreSetAudioBufferSize;
	core->getAudioBufferSize = _DSCoreGetAudioBufferSize;
	core->setAVStream = _DSCoreSetAVStream;
	core->isROM = DSIsROM;
	core->loadROM = _DSCoreLoadROM;
	core->loadBIOS = _DSCoreLoadBIOS;
	core->loadSave = _DSCoreLoadSave;
	core->loadPatch = _DSCoreLoadPatch;
	core->unloadROM = _DSCoreUnloadROM;
	core->reset = _DSCoreReset;
	core->runFrame = _DSCoreRunFrame;
	core->runLoop = _DSCoreRunLoop;
	core->step = _DSCoreStep;
	core->stateSize = _DSCoreStateSize;
	core->loadState = _DSCoreLoadState;
	core->saveState = _DSCoreSaveState;
	core->setKeys = _DSCoreSetKeys;
	core->addKeys = _DSCoreAddKeys;
	core->clearKeys = _DSCoreClearKeys;
	core->frameCounter = _DSCoreFrameCounter;
	core->frameCycles = _DSCoreFrameCycles;
	core->frequency = _DSCoreFrequency;
	core->getGameTitle = _DSCoreGetGameTitle;
	core->getGameCode = _DSCoreGetGameCode;
	core->setRTC = _DSCoreSetRTC;
	core->setRotation = _DSCoreSetRotation;
	core->setRumble = _DSCoreSetRumble;
	core->busRead8 = _DSCoreBusRead8;
	core->busRead16 = _DSCoreBusRead16;
	core->busRead32 = _DSCoreBusRead32;
	core->busWrite8 = _DSCoreBusWrite8;
	core->busWrite16 = _DSCoreBusWrite16;
	core->busWrite32 = _DSCoreBusWrite32;
	core->rawRead8 = _DSCoreRawRead8;
	core->rawRead16 = _DSCoreRawRead16;
	core->rawRead32 = _DSCoreRawRead32;
	core->rawWrite8 = _DSCoreRawWrite8;
	core->rawWrite16 = _DSCoreRawWrite16;
	core->rawWrite32 = _DSCoreRawWrite32;
	core->supportsDebuggerType = _DSCoreSupportsDebuggerType;
	core->debuggerPlatform = _DSCoreDebuggerPlatform;
	core->cliDebuggerSystem = _DSCoreCliDebuggerSystem;
	core->attachDebugger = _DSCoreAttachDebugger;
	core->detachDebugger = _DSCoreDetachDebugger;
	core->cheatDevice = _DSCoreCheatDevice;
	core->savedataClone = _DSCoreSavedataClone;
	core->savedataLoad = _DSCoreSavedataLoad;
	return core;
}
