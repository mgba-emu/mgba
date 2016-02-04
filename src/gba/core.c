/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "core.h"

#include "core/core.h"
#include "core/log.h"
#include "gba/gba.h"
#include "gba/renderers/video-software.h"
#include "util/memory.h"

struct GBACore {
	struct mCore d;
	struct GBAVideoSoftwareRenderer renderer;
	int keys;
};

static bool _GBACoreInit(struct mCore* core) {
	struct GBACore* gbacore = (struct GBACore*) core;

	struct ARMCore* cpu = anonymousMemoryMap(sizeof(struct ARMCore));
	struct GBA* gba = anonymousMemoryMap(sizeof(struct GBA));
	if (!cpu || !gba) {
		free(cpu);
		free(gba);
		return false;
	}
	core->cpu = cpu;
	core->board = gba;

	GBACreate(gba);
	// TODO: Restore debugger and cheats
	ARMSetComponents(cpu, &gba->d, 0, 0);
	ARMInit(cpu);

	GBAVideoSoftwareRendererCreate(&gbacore->renderer);
	GBAVideoAssociateRenderer(&gba->video, &gbacore->renderer.d);

	gba->keySource = &gbacore->keys;

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetInit(&core->dirs);
#endif
	
	return true;
}

static void _GBACoreDeinit(struct mCore* core) {
	ARMDeinit(core->cpu);
	GBADestroy(core->board);
	mappedMemoryFree(core->cpu, sizeof(struct ARMCore));
	mappedMemoryFree(core->board, sizeof(struct GBA));
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	mDirectorySetDeinit(&core->dirs);
#endif
}

static void _GBACoreSetSync(struct mCore* core, struct mCoreSync* sync) {
	struct GBA* gba = core->board;
	gba->sync = sync;
}

static void _GBACoreDesiredVideoDimensions(struct mCore* core, unsigned* width, unsigned* height) {
	UNUSED(core);
	*width = VIDEO_HORIZONTAL_PIXELS;
	*height = VIDEO_VERTICAL_PIXELS;
}

static void _GBACoreSetVideoBuffer(struct mCore* core, color_t* buffer, size_t stride) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->renderer.outputBuffer = buffer;
	gbacore->renderer.outputBufferStride = stride;
}

static struct blip_t* _GBACoreGetAudioChannel(struct mCore* core, int ch) {
	struct GBA* gba = core->board;
	switch (ch) {
	case 0:
		return gba->audio.psg.left;
	case 1:
		return gba->audio.psg.right;
	default:
		return NULL;
	}
}

static bool _GBACoreLoadROM(struct mCore* core, struct VFile* vf) {
	return GBALoadROM2(core->board, vf);
}

static bool _GBACoreLoadSave(struct mCore* core, struct VFile* vf) {
	return GBALoadSave(core->board, vf);
}

static bool _GBACoreLoadPatch(struct mCore* core, struct VFile* vf) {
	// TODO
	UNUSED(core);
	UNUSED(vf);
	mLOG(GBA, STUB, "Patches are not yet supported");
	return false;
}

static void _GBACoreUnloadROM(struct mCore* core) {
	return GBAUnloadROM(core->board);
}

static void _GBACoreReset(struct mCore* core) {
	ARMReset(core->cpu);
}

static void _GBACoreRunFrame(struct mCore* core) {
	struct GBA* gba = core->board;
	int32_t frameCounter = gba->video.frameCounter;
	while (gba->video.frameCounter == frameCounter) {
		ARMRunLoop(core->cpu);
	}
}

static void _GBACoreRunLoop(struct mCore* core) {
	ARMRunLoop(core->cpu);
}

static void _GBACoreStep(struct mCore* core) {
	ARMRun(core->cpu);
}

static void _GBACoreSetKeys(struct mCore* core, uint32_t keys) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->keys = keys;
}

static void _GBACoreAddKeys(struct mCore* core, uint32_t keys) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->keys |= keys;
}

static void _GBACoreClearKeys(struct mCore* core, uint32_t keys) {
	struct GBACore* gbacore = (struct GBACore*) core;
	gbacore->keys &= ~keys;
}

static int32_t _GBACoreFrameCounter(struct mCore* core) {
	struct GBA* gba = core->board;
	return gba->video.frameCounter;
}

static int32_t _GBACoreFrameCycles(struct mCore* core) {
	UNUSED(core);
	return VIDEO_TOTAL_LENGTH;
}

static int32_t _GBACoreFrequency(struct mCore* core) {
	UNUSED(core);
	return GBA_ARM7TDMI_FREQUENCY;
}

static void _GBACoreSetRTC(struct mCore* core, struct mRTCSource* rtc) {
	struct GBA* gba = core->board;
	gba->rtcSource = rtc;
}

struct mCore* GBACoreCreate(void) {
	struct GBACore* gbacore = malloc(sizeof(*gbacore));
	struct mCore* core = &gbacore->d;
	core->cpu = 0;
	core->board = 0;
	core->init = _GBACoreInit;
	core->deinit = _GBACoreDeinit;
	core->setSync = _GBACoreSetSync;
	core->desiredVideoDimensions = _GBACoreDesiredVideoDimensions;
	core->setVideoBuffer = _GBACoreSetVideoBuffer;
	core->getAudioChannel = _GBACoreGetAudioChannel;
	core->isROM = GBAIsROM;
	core->loadROM = _GBACoreLoadROM;
	core->loadSave = _GBACoreLoadSave;
	core->loadPatch = _GBACoreLoadPatch;
	core->unloadROM = _GBACoreUnloadROM;
	core->reset = _GBACoreReset;
	core->runFrame = _GBACoreRunFrame;
	core->runLoop = _GBACoreRunLoop;
	core->step = _GBACoreStep;
	core->setKeys = _GBACoreSetKeys;
	core->addKeys = _GBACoreAddKeys;
	core->clearKeys = _GBACoreClearKeys;
	core->frameCounter = _GBACoreFrameCounter;
	core->frameCycles = _GBACoreFrameCycles;
	core->frequency = _GBACoreFrequency;
	core->setRTC = _GBACoreSetRTC;
	return core;
}
