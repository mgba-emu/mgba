/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/video.h>

#include <mgba/core/sync.h>
#include <mgba/core/thread.h>
#include <mgba/core/tile-cache.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/serialize.h>
#include <mgba/internal/lr35902/lr35902.h>

#include <mgba-util/memory.h>

static void GBVideoDummyRendererInit(struct GBVideoRenderer* renderer, enum GBModel model);
static void GBVideoDummyRendererDeinit(struct GBVideoRenderer* renderer);
static uint8_t GBVideoDummyRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoDummyRendererWritePalette(struct GBVideoRenderer* renderer, int index, uint16_t value);
static void GBVideoDummyRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address);
static void GBVideoDummyRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam);
static void GBVideoDummyRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj* obj, size_t oamMax);
static void GBVideoDummyRendererFinishScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoDummyRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoDummyRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBVideoDummyRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels);

static void _cleanOAM(struct GBVideo* video, int y);

static void _endMode0(struct mTiming* timing, void* context, uint32_t cyclesLate);
static void _endMode1(struct mTiming* timing, void* context, uint32_t cyclesLate);
static void _endMode2(struct mTiming* timing, void* context, uint32_t cyclesLate);
static void _endMode3(struct mTiming* timing, void* context, uint32_t cyclesLate);
static void _updateFrameCount(struct mTiming* timing, void* context, uint32_t cyclesLate);

static struct GBVideoRenderer dummyRenderer = {
	.init = GBVideoDummyRendererInit,
	.deinit = GBVideoDummyRendererDeinit,
	.writeVideoRegister = GBVideoDummyRendererWriteVideoRegister,
	.writeVRAM = GBVideoDummyRendererWriteVRAM,
	.writeOAM = GBVideoDummyRendererWriteOAM,
	.writePalette = GBVideoDummyRendererWritePalette,
	.drawRange = GBVideoDummyRendererDrawRange,
	.finishScanline = GBVideoDummyRendererFinishScanline,
	.finishFrame = GBVideoDummyRendererFinishFrame,
	.getPixels = GBVideoDummyRendererGetPixels,
	.putPixels = GBVideoDummyRendererPutPixels,
};

void GBVideoInit(struct GBVideo* video) {
	video->renderer = &dummyRenderer;
	video->renderer->cache = NULL;
	video->vram = 0;
	video->frameskip = 0;

	video->modeEvent.context = video;
	video->modeEvent.name = "GB Video Mode";
	video->modeEvent.callback = NULL;
	video->modeEvent.priority = 8;
	video->frameEvent.context = video;
	video->frameEvent.name = "GB Video Frame";
	video->frameEvent.callback = _updateFrameCount;
	video->frameEvent.priority = 9;

	video->dmgPalette[0] = 0x7FFF;
	video->dmgPalette[1] = 0x56B5;
	video->dmgPalette[2] = 0x294A;
	video->dmgPalette[3] = 0x0000;
}

void GBVideoReset(struct GBVideo* video) {
	video->ly = 0;
	video->x = 0;
	video->mode = 1;
	video->stat = 1;

	video->frameCounter = 0;
	video->frameskipCounter = 0;

	if (video->vram) {
		mappedMemoryFree(video->vram, GB_SIZE_VRAM);
	}
	video->vram = anonymousMemoryMap(GB_SIZE_VRAM);
	GBVideoSwitchBank(video, 0);
	video->renderer->vram = video->vram;
	memset(&video->oam, 0, sizeof(video->oam));
	video->renderer->oam = &video->oam;
	memset(&video->palette, 0, sizeof(video->palette));

	video->renderer->deinit(video->renderer);
	video->renderer->init(video->renderer, video->p->model);
}

void GBVideoDeinit(struct GBVideo* video) {
	GBVideoAssociateRenderer(video, &dummyRenderer);
	mappedMemoryFree(video->vram, GB_SIZE_VRAM);
}

void GBVideoAssociateRenderer(struct GBVideo* video, struct GBVideoRenderer* renderer) {
	video->renderer->deinit(video->renderer);
	renderer->cache = video->renderer->cache;
	video->renderer = renderer;
	renderer->vram = video->vram;
	video->renderer->init(video->renderer, video->p->model);
}

static bool _statIRQAsserted(struct GBVideo* video, GBRegisterSTAT stat) {
	// TODO: variable for the IRQ line value?
	if (GBRegisterSTATIsLYCIRQ(stat) && GBRegisterSTATIsLYC(stat)) {
		return true;
	}
	switch (GBRegisterSTATGetMode(stat)) {
	case 0:
		if (GBRegisterSTATIsHblankIRQ(stat)) {
			return true;
		}
		break;
	case 1:
		if (GBRegisterSTATIsVblankIRQ(stat)) {
			return true;
		}
		break;
	case 2:
		if (GBRegisterSTATIsOAMIRQ(stat)) {
			return true;
		}
		break;
	case 3:
		break;
	}
	return false;
}

void _endMode0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBVideo* video = context;
	if (video->frameskipCounter <= 0) {
		video->renderer->finishScanline(video->renderer, video->ly);
	}
	int lyc = video->p->memory.io[REG_LYC];
	int32_t next;
	++video->ly;
	video->p->memory.io[REG_LY] = video->ly;
	GBRegisterSTAT oldStat = video->stat;
	video->stat = GBRegisterSTATSetLYC(video->stat, lyc == video->ly);
	if (video->ly < GB_VIDEO_VERTICAL_PIXELS) {
		// TODO: Cache SCX & 7 in case it changes during mode 2
		next = GB_VIDEO_MODE_2_LENGTH + (video->p->memory.io[REG_SCX] & 7);
		video->mode = 2;
		video->modeEvent.callback = _endMode2;
	} else {
		next = GB_VIDEO_HORIZONTAL_LENGTH;
		video->mode = 1;
		video->modeEvent.callback = _endMode1;

		mTimingDeschedule(&video->p->timing, &video->frameEvent);
		mTimingSchedule(&video->p->timing, &video->frameEvent, -cyclesLate);

		if (!_statIRQAsserted(video, oldStat) && GBRegisterSTATIsOAMIRQ(video->stat)) {
			video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
		}
		video->p->memory.io[REG_IF] |= (1 << GB_IRQ_VBLANK);
	}
	video->stat = GBRegisterSTATSetMode(video->stat, video->mode);
	if (!_statIRQAsserted(video, oldStat) && _statIRQAsserted(video, video->stat)) {
		video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
	}
	GBUpdateIRQs(video->p);
	video->p->memory.io[REG_STAT] = video->stat;
	mTimingSchedule(timing, &video->modeEvent, (next << video->p->doubleSpeed) - cyclesLate);
}

void _endMode1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBVideo* video = context;
	if (!GBRegisterLCDCIsEnable(video->p->memory.io[REG_LCDC])) {
		return;
	}
	int lyc = video->p->memory.io[REG_LYC];
	// TODO: One M-cycle delay
	++video->ly;
	int32_t next;
	if (video->ly == GB_VIDEO_VERTICAL_TOTAL_PIXELS + 1) {
		video->ly = 0;
		video->p->memory.io[REG_LY] = video->ly;
		next = GB_VIDEO_MODE_2_LENGTH + (video->p->memory.io[REG_SCX] & 7);
		video->mode = 2;
		video->modeEvent.callback = _endMode2;
	} else if (video->ly == GB_VIDEO_VERTICAL_TOTAL_PIXELS) {
		video->p->memory.io[REG_LY] = 0;
		next = GB_VIDEO_HORIZONTAL_LENGTH - 8;
	} else if (video->ly == GB_VIDEO_VERTICAL_TOTAL_PIXELS - 1) {
		video->p->memory.io[REG_LY] = video->ly;
		next = 8;
	} else {
		video->p->memory.io[REG_LY] = video->ly;
		next = GB_VIDEO_HORIZONTAL_LENGTH;
	}

	GBRegisterSTAT oldStat = video->stat;
	video->stat = GBRegisterSTATSetMode(video->stat, video->mode);
	video->stat = GBRegisterSTATSetLYC(video->stat, lyc == video->p->memory.io[REG_LY]);
	if (!_statIRQAsserted(video, oldStat) && _statIRQAsserted(video, video->stat)) {
		video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
		GBUpdateIRQs(video->p);
	}
	video->p->memory.io[REG_STAT] = video->stat;
	mTimingSchedule(timing, &video->modeEvent, (next << video->p->doubleSpeed) - cyclesLate);
}

void _endMode2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBVideo* video = context;
	_cleanOAM(video, video->ly);
	video->x = 0;
	video->dotClock = timing->masterCycles - cyclesLate;
	int32_t next = GB_VIDEO_MODE_3_LENGTH_BASE + video->objMax * 6 - (video->p->memory.io[REG_SCX] & 7);
	video->mode = 3;
	video->modeEvent.callback = _endMode3;
	GBRegisterSTAT oldStat = video->stat;
	video->stat = GBRegisterSTATSetMode(video->stat, video->mode);
	if (!_statIRQAsserted(video, oldStat) && _statIRQAsserted(video, video->stat)) {
		video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
		GBUpdateIRQs(video->p);
	}
	video->p->memory.io[REG_STAT] = video->stat;
	mTimingSchedule(timing, &video->modeEvent, (next << video->p->doubleSpeed) - cyclesLate);
}

void _endMode3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBVideo* video = context;
	GBVideoProcessDots(video);
	if (video->ly < GB_VIDEO_VERTICAL_PIXELS && video->p->memory.isHdma && video->p->memory.io[REG_HDMA5] != 0xFF) {
		video->p->memory.hdmaRemaining = 0x10;
		video->p->cpuBlocked = true;
		mTimingDeschedule(timing, &video->p->memory.hdmaEvent);
		mTimingSchedule(timing, &video->p->memory.hdmaEvent, 0);
	}
	video->mode = 0;
	video->modeEvent.callback = _endMode0;
	GBRegisterSTAT oldStat = video->stat;
	video->stat = GBRegisterSTATSetMode(video->stat, video->mode);
	if (!_statIRQAsserted(video, oldStat) && _statIRQAsserted(video, video->stat)) {
		video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
		GBUpdateIRQs(video->p);
	}
	video->p->memory.io[REG_STAT] = video->stat;
	int32_t next = GB_VIDEO_MODE_0_LENGTH_BASE - video->objMax * 6;
	mTimingSchedule(timing, &video->modeEvent, (next << video->p->doubleSpeed) - cyclesLate);
}

void _updateFrameCount(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(cyclesLate);
	struct GBVideo* video = context;
	if (video->p->cpu->executionState != LR35902_CORE_FETCH) {
		mTimingSchedule(timing, &video->frameEvent, 4 - ((video->p->cpu->executionState + 1) & 3));
		return;
	}

	size_t c;
	for (c = 0; c < mCoreCallbacksListSize(&video->p->coreCallbacks); ++c) {
		struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&video->p->coreCallbacks, c);
		if (callbacks->videoFrameEnded) {
			callbacks->videoFrameEnded(callbacks->context);
		}
	}

	GBFrameEnded(video->p);
	mCoreSyncPostFrame(video->p->sync);
	--video->frameskipCounter;
	if (video->frameskipCounter < 0) {
		video->renderer->finishFrame(video->renderer);
		video->frameskipCounter = video->frameskip;
	}
	++video->frameCounter;

	// TODO: Move to common code
	if (video->p->stream && video->p->stream->postVideoFrame) {
		const color_t* pixels;
		size_t stride;
		video->renderer->getPixels(video->renderer, &stride, (const void**) &pixels);
		video->p->stream->postVideoFrame(video->p->stream, pixels, stride);
	}

	if (!GBRegisterLCDCIsEnable(video->p->memory.io[REG_LCDC])) {
		mTimingSchedule(timing, &video->frameEvent, GB_VIDEO_TOTAL_LENGTH);
	}

	for (c = 0; c < mCoreCallbacksListSize(&video->p->coreCallbacks); ++c) {
		struct mCoreCallbacks* callbacks = mCoreCallbacksListGetPointer(&video->p->coreCallbacks, c);
		if (callbacks->videoFrameStarted) {
			callbacks->videoFrameStarted(callbacks->context);
		}
	}
}

static void _cleanOAM(struct GBVideo* video, int y) {
	// TODO: GBC differences
	// TODO: Optimize
	video->objMax = 0;
	int spriteHeight = 8;
	if (GBRegisterLCDCIsObjSize(video->p->memory.io[REG_LCDC])) {
		spriteHeight = 16;
	}
	int o = 0;
	int i;
	for (i = 0; i < 40; ++i) {
		uint8_t oy = video->oam.obj[i].y;
		if (y < oy - 16 || y >= oy - 16 + spriteHeight) {
			continue;
		}
		// TODO: Sort
		video->objThisLine[o] = video->oam.obj[i];
		++o;
		if (o == 10) {
			break;
		}
	}
	video->objMax = o;
}

void GBVideoProcessDots(struct GBVideo* video) {
	if (video->mode != 3) {
		return;
	}
	int oldX = video->x;
	video->x = (video->p->timing.masterCycles - video->dotClock + video->p->cpu->cycles) >> video->p->doubleSpeed;
	if (video->x > GB_VIDEO_HORIZONTAL_PIXELS) {
		video->x = GB_VIDEO_HORIZONTAL_PIXELS;
	} else if (video->x < 0) {
		mLOG(GB, FATAL, "Video dot clock went negative!");
		video->x = oldX;
	}
	if (video->frameskipCounter <= 0) {
		video->renderer->drawRange(video->renderer, oldX, video->x, video->ly, video->objThisLine, video->objMax);
	}
}

void GBVideoWriteLCDC(struct GBVideo* video, GBRegisterLCDC value) {
	if (!GBRegisterLCDCIsEnable(video->p->memory.io[REG_LCDC]) && GBRegisterLCDCIsEnable(value)) {
		video->mode = 2;
		video->modeEvent.callback = _endMode2;
		int32_t next = GB_VIDEO_MODE_2_LENGTH - 5; // TODO: Why is this fudge factor needed? Might be related to T-cycles for load/store differing
		mTimingSchedule(&video->p->timing, &video->modeEvent, next << video->p->doubleSpeed);

		video->ly = 0;
		video->p->memory.io[REG_LY] = 0;
		GBRegisterSTAT oldStat = video->stat;
		video->stat = GBRegisterSTATSetMode(video->stat, 0);
		video->stat = GBRegisterSTATSetLYC(video->stat, video->ly == video->p->memory.io[REG_LYC]);
		if (!_statIRQAsserted(video, oldStat) && _statIRQAsserted(video, video->stat)) {
			video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
			GBUpdateIRQs(video->p);
		}
		video->p->memory.io[REG_STAT] = video->stat;
		video->renderer->writePalette(video->renderer, 0, video->palette[0]);

		mTimingDeschedule(&video->p->timing, &video->frameEvent);
	}
	if (GBRegisterLCDCIsEnable(video->p->memory.io[REG_LCDC]) && !GBRegisterLCDCIsEnable(value)) {
		// TODO: Fix serialization; this gets internal and visible modes out of sync
		video->stat = GBRegisterSTATSetMode(video->stat, 0);
		video->p->memory.io[REG_STAT] = video->stat;
		video->ly = 0;
		video->p->memory.io[REG_LY] = 0;
		video->renderer->writePalette(video->renderer, 0, video->dmgPalette[0]);
	
		mTimingDeschedule(&video->p->timing, &video->modeEvent);
		mTimingSchedule(&video->p->timing, &video->frameEvent, GB_VIDEO_TOTAL_LENGTH);
	}
	video->p->memory.io[REG_STAT] = video->stat;
}

void GBVideoWriteSTAT(struct GBVideo* video, GBRegisterSTAT value) {
	GBRegisterSTAT oldStat = video->stat;
	video->stat = (video->stat & 0x7) | (value & 0x78);
	if (!GBRegisterLCDCIsEnable(video->p->memory.io[REG_LCDC]) || video->p->model >= GB_MODEL_CGB) {
		return;
	}
	if (!_statIRQAsserted(video, oldStat) && video->mode < 3) {
		// TODO: variable for the IRQ line value?
		video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
		GBUpdateIRQs(video->p);
	}
}

void GBVideoWriteLYC(struct GBVideo* video, uint8_t value) {
	GBRegisterSTAT oldStat = video->stat;
	video->stat = GBRegisterSTATSetLYC(video->stat, value == video->ly);
	if (!_statIRQAsserted(video, oldStat) && _statIRQAsserted(video, video->stat)) {
		video->p->memory.io[REG_IF] |= (1 << GB_IRQ_LCDSTAT);
		GBUpdateIRQs(video->p);
	}
	video->p->memory.io[REG_STAT] = video->stat;
}

void GBVideoWritePalette(struct GBVideo* video, uint16_t address, uint8_t value) {
	if (video->p->model < GB_MODEL_CGB) {
		switch (address) {
		case REG_BGP:
			video->palette[0] = video->dmgPalette[value & 3];
			video->palette[1] = video->dmgPalette[(value >> 2) & 3];
			video->palette[2] = video->dmgPalette[(value >> 4) & 3];
			video->palette[3] = video->dmgPalette[(value >> 6) & 3];
			video->renderer->writePalette(video->renderer, 0, video->palette[0]);
			video->renderer->writePalette(video->renderer, 1, video->palette[1]);
			video->renderer->writePalette(video->renderer, 2, video->palette[2]);
			video->renderer->writePalette(video->renderer, 3, video->palette[3]);
			break;
		case REG_OBP0:
			video->palette[8 * 4 + 0] = video->dmgPalette[value & 3];
			video->palette[8 * 4 + 1] = video->dmgPalette[(value >> 2) & 3];
			video->palette[8 * 4 + 2] = video->dmgPalette[(value >> 4) & 3];
			video->palette[8 * 4 + 3] = video->dmgPalette[(value >> 6) & 3];
			video->renderer->writePalette(video->renderer, 8 * 4 + 0, video->palette[8 * 4 + 0]);
			video->renderer->writePalette(video->renderer, 8 * 4 + 1, video->palette[8 * 4 + 1]);
			video->renderer->writePalette(video->renderer, 8 * 4 + 2, video->palette[8 * 4 + 2]);
			video->renderer->writePalette(video->renderer, 8 * 4 + 3, video->palette[8 * 4 + 3]);
			break;
		case REG_OBP1:
			video->palette[9 * 4 + 0] = video->dmgPalette[value & 3];
			video->palette[9 * 4 + 1] = video->dmgPalette[(value >> 2) & 3];
			video->palette[9 * 4 + 2] = video->dmgPalette[(value >> 4) & 3];
			video->palette[9 * 4 + 3] = video->dmgPalette[(value >> 6) & 3];
			video->renderer->writePalette(video->renderer, 9 * 4 + 0, video->palette[9 * 4 + 0]);
			video->renderer->writePalette(video->renderer, 9 * 4 + 1, video->palette[9 * 4 + 1]);
			video->renderer->writePalette(video->renderer, 9 * 4 + 2, video->palette[9 * 4 + 2]);
			video->renderer->writePalette(video->renderer, 9 * 4 + 3, video->palette[9 * 4 + 3]);
			break;
		}
	} else {
		switch (address) {
		case REG_BCPD:
			if (video->bcpIndex & 1) {
				video->palette[video->bcpIndex >> 1] &= 0x00FF;
				video->palette[video->bcpIndex >> 1] |= value << 8;
			} else {
				video->palette[video->bcpIndex >> 1] &= 0xFF00;
				video->palette[video->bcpIndex >> 1] |= value;
			}
			video->renderer->writePalette(video->renderer, video->bcpIndex >> 1, video->palette[video->bcpIndex >> 1]);
			if (video->bcpIncrement) {
				++video->bcpIndex;
				video->bcpIndex &= 0x3F;
				video->p->memory.io[REG_BCPS] &= 0x80;
				video->p->memory.io[REG_BCPS] |= video->bcpIndex;
			}
			video->p->memory.io[REG_BCPD] = video->palette[video->bcpIndex >> 1] >> (8 * (video->bcpIndex & 1));
			break;
		case REG_OCPD:
			if (video->ocpIndex & 1) {
				video->palette[8 * 4 + (video->ocpIndex >> 1)] &= 0x00FF;
				video->palette[8 * 4 + (video->ocpIndex >> 1)] |= value << 8;
			} else {
				video->palette[8 * 4 + (video->ocpIndex >> 1)] &= 0xFF00;
				video->palette[8 * 4 + (video->ocpIndex >> 1)] |= value;
			}
			video->renderer->writePalette(video->renderer, 8 * 4 + (video->ocpIndex >> 1), video->palette[8 * 4 + (video->ocpIndex >> 1)]);
			if (video->ocpIncrement) {
				++video->ocpIndex;
				video->ocpIndex &= 0x3F;
				video->p->memory.io[REG_OCPS] &= 0x80;
				video->p->memory.io[REG_OCPS] |= video->ocpIndex;
			}
			video->p->memory.io[REG_OCPD] = video->palette[8 * 4 + (video->ocpIndex >> 1)] >> (8 * (video->ocpIndex & 1));
			break;
		}
	}
}

void GBVideoSwitchBank(struct GBVideo* video, uint8_t value) {
	value &= 1;
	video->vramBank = &video->vram[value * GB_SIZE_VRAM_BANK0];
	video->vramCurrentBank = value;
}

void GBVideoSetPalette(struct GBVideo* video, unsigned index, uint32_t color) {
	if (index >= 4) {
		return;
	}
	video->dmgPalette[index] = M_RGB8_TO_RGB5(color);
}

static void GBVideoDummyRendererInit(struct GBVideoRenderer* renderer, enum GBModel model) {
	UNUSED(renderer);
	UNUSED(model);
	// Nothing to do
}

static void GBVideoDummyRendererDeinit(struct GBVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static uint8_t GBVideoDummyRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value) {
	UNUSED(renderer);
	UNUSED(address);
	return value;
}

static void GBVideoDummyRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address) {
	if (renderer->cache) {
		mTileCacheWriteVRAM(renderer->cache, address);
	}
}

static void GBVideoDummyRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam) {
	UNUSED(renderer);
	UNUSED(oam);
	// Nothing to do
}

static void GBVideoDummyRendererWritePalette(struct GBVideoRenderer* renderer, int index, uint16_t value) {
	UNUSED(value);
	if (renderer->cache) {
		mTileCacheWritePalette(renderer->cache, index << 1);
	}
}

static void GBVideoDummyRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj* obj, size_t oamMax) {
	UNUSED(renderer);
	UNUSED(endX);
	UNUSED(startX);
	UNUSED(y);
	UNUSED(obj);
	UNUSED(oamMax);
	// Nothing to do
}

static void GBVideoDummyRendererFinishScanline(struct GBVideoRenderer* renderer, int y) {
	UNUSED(renderer);
	UNUSED(y);
	// Nothing to do
}

static void GBVideoDummyRendererFinishFrame(struct GBVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBVideoDummyRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
	// Nothing to do
}

static void GBVideoDummyRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
	// Nothing to do
}

void GBVideoSerialize(const struct GBVideo* video, struct GBSerializedState* state) {
	STORE_16LE(video->x, 0, &state->video.x);
	STORE_16LE(video->ly, 0, &state->video.ly);
	STORE_32LE(video->frameCounter, 0, &state->video.frameCounter);
	state->video.vramCurrentBank = video->vramCurrentBank;

	GBSerializedVideoFlags flags = 0;
	flags = GBSerializedVideoFlagsSetBcpIncrement(flags, video->bcpIncrement);
	flags = GBSerializedVideoFlagsSetOcpIncrement(flags, video->ocpIncrement);
	flags = GBSerializedVideoFlagsSetMode(flags, video->mode);
	flags = GBSerializedVideoFlagsSetNotModeEventScheduled(flags, !mTimingIsScheduled(&video->p->timing, &video->modeEvent));
	flags = GBSerializedVideoFlagsSetNotFrameEventScheduled(flags, !mTimingIsScheduled(&video->p->timing, &video->frameEvent));
	state->video.flags = flags;
	STORE_16LE(video->bcpIndex, 0, &state->video.bcpIndex);
	STORE_16LE(video->ocpIndex, 0, &state->video.ocpIndex);

	size_t i;
	for (i = 0; i < 64; ++i) {
		STORE_16LE(video->palette[i], i * 2, state->video.palette);
	}

	STORE_32LE(video->modeEvent.when - mTimingCurrentTime(&video->p->timing), 0, &state->video.nextMode);
	STORE_32LE(video->frameEvent.when - mTimingCurrentTime(&video->p->timing), 0, &state->video.nextFrame);

	memcpy(state->vram, video->vram, GB_SIZE_VRAM);
	memcpy(state->oam, &video->oam.raw, GB_SIZE_OAM);
}

void GBVideoDeserialize(struct GBVideo* video, const struct GBSerializedState* state) {
	LOAD_16LE(video->x, 0, &state->video.x);
	LOAD_16LE(video->ly, 0, &state->video.ly);
	LOAD_32LE(video->frameCounter, 0, &state->video.frameCounter);
	video->vramCurrentBank = state->video.vramCurrentBank;

	GBSerializedVideoFlags flags = state->video.flags;
	video->bcpIncrement = GBSerializedVideoFlagsGetBcpIncrement(flags);
	video->ocpIncrement = GBSerializedVideoFlagsGetOcpIncrement(flags);
	video->mode = GBSerializedVideoFlagsGetMode(flags);
	LOAD_16LE(video->bcpIndex, 0, &state->video.bcpIndex);
	video->bcpIndex &= 0x3F;
	LOAD_16LE(video->ocpIndex, 0, &state->video.ocpIndex);
	video->ocpIndex &= 0x3F;

	switch (video->mode) {
	case 0:
		video->modeEvent.callback = _endMode0;
		break;
	case 1:
		video->modeEvent.callback = _endMode1;
		break;
	case 2:
		video->modeEvent.callback = _endMode2;
		break;
	case 3:
		video->modeEvent.callback = _endMode3;
		break;
	}

	uint32_t when;
	if (!GBSerializedVideoFlagsIsNotModeEventScheduled(flags)) {
		LOAD_32LE(when, 0, &state->video.nextMode);
		mTimingSchedule(&video->p->timing, &video->modeEvent, when);
	}
	if (!GBSerializedVideoFlagsIsNotFrameEventScheduled(flags)) {
		LOAD_32LE(when, 0, &state->video.nextFrame);
		mTimingSchedule(&video->p->timing, &video->frameEvent, when);
	}

	size_t i;
	for (i = 0; i < 64; ++i) {
		LOAD_16LE(video->palette[i], i * 2, state->video.palette);
		video->renderer->writePalette(video->renderer, i, video->palette[i]);
	}

	memcpy(video->vram, state->vram, GB_SIZE_VRAM);
	memcpy(&video->oam.raw, state->oam, GB_SIZE_OAM);

	_cleanOAM(video, video->ly);
	GBVideoSwitchBank(video, video->vramCurrentBank);

	video->renderer->deinit(video->renderer);
	video->renderer->init(video->renderer, video->p->model);
}
