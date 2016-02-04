/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core.h"

#include "core/core.h"
#include "gb/gb.h"
#include "gb/renderers/software.h"
#include "util/memory.h"

struct GBCore {
	struct mCore d;
	struct GBVideoSoftwareRenderer renderer;
	uint8_t keys;
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

	GBCreate(gb);
	LR35902SetComponents(cpu, &gb->d, 0, 0);
	LR35902Init(cpu);

	GBVideoSoftwareRendererCreate(&gbcore->renderer);
	GBVideoAssociateRenderer(&gb->video, &gbcore->renderer.d);

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
}

static void _GBCoreSetSync(struct mCore* core, struct mCoreSync* sync) {
	struct GB* gb = core->board;
	gb->sync = sync;
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

static bool _GBCoreLoadROM(struct mCore* core, struct VFile* vf) {
	return GBLoadROM(core->board, vf);
}

static bool _GBCoreLoadSave(struct mCore* core, struct VFile* vf) {
	return GBLoadSave(core->board, vf);
}

static void _GBCoreUnloadROM(struct mCore* core) {
	return GBUnloadROM(core->board);
}

static void _GBCoreReset(struct mCore* core) {
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

static void _GBCoreSetRTC(struct mCore* core, struct mRTCSource* rtc) {
	struct GB* gb = core->board;
	gb->memory.rtc = rtc;
}

struct mCore* GBCoreCreate(void) {
	struct GBCore* gbcore = malloc(sizeof(*gbcore));
	struct mCore* core = &gbcore->d;
	core->cpu = 0;
	core->board = 0;
	core->init = _GBCoreInit;
	core->deinit = _GBCoreDeinit;
	core->setSync = _GBCoreSetSync;
	core->desiredVideoDimensions = _GBCoreDesiredVideoDimensions;
	core->setVideoBuffer = _GBCoreSetVideoBuffer;
	core->isROM = GBIsROM;
	core->loadROM = _GBCoreLoadROM;
	core->loadSave = _GBCoreLoadSave;
	core->unloadROM = _GBCoreUnloadROM;
	core->reset = _GBCoreReset;
	core->runFrame = _GBCoreRunFrame;
	core->runLoop = _GBCoreRunLoop;
	core->step = _GBCoreStep;
	core->setKeys = _GBCoreSetKeys;
	core->addKeys = _GBCoreAddKeys;
	core->clearKeys = _GBCoreClearKeys;
	core->frameCounter = _GBCoreFrameCounter;
	core->frameCycles = _GBCoreFrameCycles;
	core->frequency = _GBCoreFrequency;
	core->setRTC = _GBCoreSetRTC;
	return core;
}
