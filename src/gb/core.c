/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core.h"

#include "core/core.h"
#include "gb/cli.h"
#include "gb/gb.h"
#include "gb/renderers/software.h"
#include "lr35902/debugger.h"
#include "util/memory.h"
#include "util/patch.h"

struct GBCore {
	struct mCore d;
	struct GBVideoSoftwareRenderer renderer;
	uint8_t keys;
	struct mCPUComponent* components[CPU_COMPONENT_MAX];
	struct mDebuggerPlatform* debuggerPlatform;
};

static bool _GBCoreInit(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;

	struct LR35902Core* cpu = anonymousMemoryMap(sizeof(struct LR35902Core));
	struct GB* gb = anonymousMemoryMap(sizeof(struct GB));
	if (!cpu || !gb) {
		free(cpu);
		free(gb);
		return false;
	}
	core->cpu = cpu;
	core->board = gb;
	gbcore->debuggerPlatform = NULL;

	GBCreate(gb);
	memset(gbcore->components, 0, sizeof(gbcore->components));
	LR35902SetComponents(cpu, &gb->d, CPU_COMPONENT_MAX, gbcore->components);
	LR35902Init(cpu);

	GBVideoSoftwareRendererCreate(&gbcore->renderer);

	gbcore->keys = 0;
	gb->keySource = &gbcore->keys;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetInit(&core->dirs);
#endif
	
	return true;
}

static void _GBCoreDeinit(struct mCore* core) {
	LR35902Deinit(core->cpu);
	GBDestroy(core->board);
	mappedMemoryFree(core->cpu, sizeof(struct LR35902Core));
	mappedMemoryFree(core->board, sizeof(struct GB));
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetDeinit(&core->dirs);
#endif
	free(core);
}

static enum mPlatform _GBCorePlatform(struct mCore* core) {
	UNUSED(core);
	return PLATFORM_GB;
}

static void _GBCoreSetSync(struct mCore* core, struct mCoreSync* sync) {
	struct GB* gb = core->board;
	gb->sync = sync;
}

static void _GBCoreLoadConfig(struct mCore* core, const struct mCoreConfig* config) {
	UNUSED(config);

	struct GB* gb = core->board;
	if (core->opts.mute) {
		gb->audio.masterVolume = 0;
	} else {
		gb->audio.masterVolume = core->opts.volume;
	}
	gb->video.frameskip = core->opts.frameskip;
}

static void _GBCoreDesiredVideoDimensions(struct mCore* core, unsigned* width, unsigned* height) {
	UNUSED(core);
	*width = GB_VIDEO_HORIZONTAL_PIXELS;
	*height = GB_VIDEO_VERTICAL_PIXELS;
}

static void _GBCoreSetVideoBuffer(struct mCore* core, color_t* buffer, size_t stride) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->renderer.outputBuffer = buffer;
	gbcore->renderer.outputBufferStride = stride;
}

static void _GBCoreGetVideoBuffer(struct mCore* core, color_t** buffer, size_t* stride) {
	struct GBCore* gbcore = (struct GBCore*) core;
	*buffer = gbcore->renderer.outputBuffer;
	*stride = gbcore->renderer.outputBufferStride;
}

static struct blip_t* _GBCoreGetAudioChannel(struct mCore* core, int ch) {
	struct GB* gb = core->board;
	switch (ch) {
	case 0:
		return gb->audio.left;
	case 1:
		return gb->audio.right;
	default:
		return NULL;
	}
}

static void _GBCoreSetAudioBufferSize(struct mCore* core, size_t samples) {
	struct GB* gb = core->board;
	GBAudioResizeBuffer(&gb->audio, samples);
}

static size_t _GBCoreGetAudioBufferSize(struct mCore* core) {
	struct GB* gb = core->board;
	return gb->audio.samples;
}

static void _GBCoreSetAVStream(struct mCore* core, struct mAVStream* stream) {
	struct GB* gb = core->board;
	gb->stream = stream;
	if (stream && stream->videoDimensionsChanged) {
		stream->videoDimensionsChanged(stream, GB_VIDEO_HORIZONTAL_PIXELS, GB_VIDEO_VERTICAL_PIXELS);
	}
}

static bool _GBCoreLoadROM(struct mCore* core, struct VFile* vf) {
	return GBLoadROM(core->board, vf);
}

static bool _GBCoreLoadBIOS(struct mCore* core, struct VFile* vf, int type) {
	UNUSED(core);
	UNUSED(vf);
	UNUSED(type);
	// TODO
	return false;
}

static bool _GBCoreLoadSave(struct mCore* core, struct VFile* vf) {
	return GBLoadSave(core->board, vf);
}

static bool _GBCoreLoadPatch(struct mCore* core, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	struct Patch patch;
	if (!loadPatch(vf, &patch)) {
		return false;
	}
	GBApplyPatch(core->board, &patch);
	return true;
}

static void _GBCoreUnloadROM(struct mCore* core) {
	return GBUnloadROM(core->board);
}

static void _GBCoreReset(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	struct GB* gb = (struct GB*) core->board;
	if (gbcore->renderer.outputBuffer) {
		GBVideoAssociateRenderer(&gb->video, &gbcore->renderer.d);
	}
	LR35902Reset(core->cpu);
}

static void _GBCoreRunFrame(struct mCore* core) {
	struct GB* gb = core->board;
	int32_t frameCounter = gb->video.frameCounter;
	while (gb->video.frameCounter == frameCounter) {
		LR35902Run(core->cpu);
	}
}

static void _GBCoreRunLoop(struct mCore* core) {
	LR35902Run(core->cpu);
}

static void _GBCoreStep(struct mCore* core) {
	LR35902Tick(core->cpu);
}

static bool _GBCoreLoadState(struct mCore* core, struct VFile* vf, int flags) {
	UNUSED(core);
	UNUSED(vf);
	UNUSED(flags);
	// TODO
	return false;
}

static bool _GBCoreSaveState(struct mCore* core, struct VFile* vf, int flags) {
	UNUSED(core);
	UNUSED(vf);
	UNUSED(flags);
	// TODO
	return false;
}

static void _GBCoreSetKeys(struct mCore* core, uint32_t keys) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->keys = keys;
}

static void _GBCoreAddKeys(struct mCore* core, uint32_t keys) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->keys |= keys;
}

static void _GBCoreClearKeys(struct mCore* core, uint32_t keys) {
	struct GBCore* gbcore = (struct GBCore*) core;
	gbcore->keys &= ~keys;
}

static int32_t _GBCoreFrameCounter(struct mCore* core) {
	struct GB* gb = core->board;
	return gb->video.frameCounter;
}

static int32_t _GBCoreFrameCycles(struct mCore* core) {
	UNUSED(core);
	return GB_VIDEO_TOTAL_LENGTH;
}

static int32_t _GBCoreFrequency(struct mCore* core) {
	UNUSED(core);
	// TODO: GB differences
	return DMG_LR35902_FREQUENCY;
}

static void _GBCoreGetGameTitle(struct mCore* core, char* title) {
	GBGetGameTitle(core->board, title);
}

static void _GBCoreGetGameCode(struct mCore* core, char* title) {
	GBGetGameCode(core->board, title);
}

static void _GBCoreSetRTC(struct mCore* core, struct mRTCSource* rtc) {
	struct GB* gb = core->board;
	gb->memory.rtc = rtc;
}

static void _GBCoreSetRotation(struct mCore* core, struct mRotationSource* rotation) {
	struct GB* gb = core->board;
	gb->memory.rotation = rotation;
}

static void _GBCoreSetRumble(struct mCore* core, struct mRumble* rumble) {
	struct GB* gb = core->board;
	gb->memory.rumble = rumble;
}

static uint32_t _GBCoreBusRead8(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return cpu->memory.load8(cpu, address);
}

static uint32_t _GBCoreBusRead16(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return cpu->memory.load8(cpu, address) | (cpu->memory.load8(cpu, address + 1) << 8);
}

static uint32_t _GBCoreBusRead32(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return cpu->memory.load8(cpu, address) | (cpu->memory.load8(cpu, address + 1) << 8) |
	       (cpu->memory.load8(cpu, address + 2) << 16) | (cpu->memory.load8(cpu, address + 3) << 24);
}

static void _GBCoreBusWrite8(struct mCore* core, uint32_t address, uint8_t value) {
	struct LR35902Core* cpu = core->cpu;
	cpu->memory.store8(cpu, address, value);
}

static void _GBCoreBusWrite16(struct mCore* core, uint32_t address, uint16_t value) {
	struct LR35902Core* cpu = core->cpu;
	cpu->memory.store8(cpu, address, value);
	cpu->memory.store8(cpu, address + 1, value >> 8);
}

static void _GBCoreBusWrite32(struct mCore* core, uint32_t address, uint32_t value) {
	struct LR35902Core* cpu = core->cpu;
	cpu->memory.store8(cpu, address, value);
	cpu->memory.store8(cpu, address + 1, value >> 8);
	cpu->memory.store8(cpu, address + 2, value >> 16);
	cpu->memory.store8(cpu, address + 3, value >> 24);
}

static uint32_t _GBCoreRawRead8(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return GBLoad8(cpu, address);
}

static uint32_t _GBCoreRawRead16(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return GBLoad8(cpu, address) | (GBLoad8(cpu, address + 1) << 8);
}

static uint32_t _GBCoreRawRead32(struct mCore* core, uint32_t address) {
	struct LR35902Core* cpu = core->cpu;
	return GBLoad8(cpu, address) | (GBLoad8(cpu, address + 1) << 8) |
	       (GBLoad8(cpu, address + 2) << 16) | (GBLoad8(cpu, address + 3) << 24);
}

static bool _GBCoreSupportsDebuggerType(struct mCore* core, enum mDebuggerType type) {
	UNUSED(core);
	switch (type) {
#ifdef USE_CLI_DEBUGGER
	case DEBUGGER_CLI:
		return true;
#endif
	default:
		return false;
	}
}

static struct mDebuggerPlatform* _GBCoreDebuggerPlatform(struct mCore* core) {
	struct GBCore* gbcore = (struct GBCore*) core;
	if (!gbcore->debuggerPlatform) {
		gbcore->debuggerPlatform = LR35902DebuggerPlatformCreate();
	}
	return gbcore->debuggerPlatform;
}

static struct CLIDebuggerSystem* _GBCoreCliDebuggerSystem(struct mCore* core) {
#ifdef USE_CLI_DEBUGGER
	return GBCLIDebuggerCreate(core);
#else
	UNUSED(core);
	return NULL;
#endif
}

static void _GBCoreAttachDebugger(struct mCore* core, struct mDebugger* debugger) {
	struct LR35902Core* cpu = core->cpu;
	if (core->debugger) {
		LR35902HotplugDetach(cpu, CPU_COMPONENT_DEBUGGER);
	}
	cpu->components[CPU_COMPONENT_DEBUGGER] = &debugger->d;
	LR35902HotplugAttach(cpu, CPU_COMPONENT_DEBUGGER);
	core->debugger = debugger;
}

static void _GBCoreDetachDebugger(struct mCore* core) {
	struct LR35902Core* cpu = core->cpu;
	LR35902HotplugDetach(cpu, CPU_COMPONENT_DEBUGGER);
	cpu->components[CPU_COMPONENT_DEBUGGER] = NULL;
	core->debugger = NULL;
}

struct mCore* GBCoreCreate(void) {
	struct GBCore* gbcore = malloc(sizeof(*gbcore));
	struct mCore* core = &gbcore->d;
	memset(&core->opts, 0, sizeof(core->opts));
	core->cpu = NULL;
	core->board = NULL;
	core->debugger = NULL;
	core->init = _GBCoreInit;
	core->deinit = _GBCoreDeinit;
	core->platform = _GBCorePlatform;
	core->setSync = _GBCoreSetSync;
	core->loadConfig = _GBCoreLoadConfig;
	core->desiredVideoDimensions = _GBCoreDesiredVideoDimensions;
	core->setVideoBuffer = _GBCoreSetVideoBuffer;
	core->getVideoBuffer = _GBCoreGetVideoBuffer;
	core->getAudioChannel = _GBCoreGetAudioChannel;
	core->setAudioBufferSize = _GBCoreSetAudioBufferSize;
	core->getAudioBufferSize = _GBCoreGetAudioBufferSize;
	core->setAVStream = _GBCoreSetAVStream;
	core->isROM = GBIsROM;
	core->loadROM = _GBCoreLoadROM;
	core->loadBIOS = _GBCoreLoadBIOS;
	core->loadSave = _GBCoreLoadSave;
	core->loadPatch = _GBCoreLoadPatch;
	core->unloadROM = _GBCoreUnloadROM;
	core->reset = _GBCoreReset;
	core->runFrame = _GBCoreRunFrame;
	core->runLoop = _GBCoreRunLoop;
	core->step = _GBCoreStep;
	core->loadState = _GBCoreLoadState;
	core->saveState = _GBCoreSaveState;
	core->setKeys = _GBCoreSetKeys;
	core->addKeys = _GBCoreAddKeys;
	core->clearKeys = _GBCoreClearKeys;
	core->frameCounter = _GBCoreFrameCounter;
	core->frameCycles = _GBCoreFrameCycles;
	core->frequency = _GBCoreFrequency;
	core->getGameTitle = _GBCoreGetGameTitle;
	core->getGameCode = _GBCoreGetGameCode;
	core->setRTC = _GBCoreSetRTC;
	core->setRotation = _GBCoreSetRotation;
	core->setRumble = _GBCoreSetRumble;
	core->busRead8 = _GBCoreBusRead8;
	core->busRead16 = _GBCoreBusRead16;
	core->busRead32 = _GBCoreBusRead32;
	core->busWrite8 = _GBCoreBusWrite8;
	core->busWrite16 = _GBCoreBusWrite16;
	core->busWrite32 = _GBCoreBusWrite32;
	core->rawRead8 = _GBCoreRawRead8;
	core->rawRead16 = _GBCoreRawRead16;
	core->rawRead32 = _GBCoreRawRead32;
	core->rawWrite8 = NULL;
	core->rawWrite16 = NULL;
	core->rawWrite32 = NULL;
	core->supportsDebuggerType = _GBCoreSupportsDebuggerType;
	core->debuggerPlatform = _GBCoreDebuggerPlatform;
	core->cliDebuggerSystem = _GBCoreCliDebuggerSystem;
	core->attachDebugger = _GBCoreAttachDebugger;
	core->detachDebugger = _GBCoreDetachDebugger;
	return core;
}
