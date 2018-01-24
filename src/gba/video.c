/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/video.h>

#include <mgba/core/sync.h>
#include <mgba/core/tile-cache.h>
#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/dma.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/serialize.h>

#include <mgba-util/memory.h>

mLOG_DEFINE_CATEGORY(GBA_VIDEO, "GBA Video", "gba.video");

static void GBAVideoDummyRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoDummyRendererReset(struct GBAVideoRenderer* renderer);
static void GBAVideoDummyRendererDeinit(struct GBAVideoRenderer* renderer);
static uint16_t GBAVideoDummyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoDummyRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address);
static void GBAVideoDummyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoDummyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoDummyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoDummyRendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoDummyRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBAVideoDummyRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels);

static void _startHblank(struct mTiming*, void* context, uint32_t cyclesLate);
static void _startHdraw(struct mTiming*, void* context, uint32_t cyclesLate);

const int GBAVideoObjSizes[16][2] = {
	{ 8, 8 },
	{ 16, 16 },
	{ 32, 32 },
	{ 64, 64 },
	{ 16, 8 },
	{ 32, 8 },
	{ 32, 16 },
	{ 64, 32 },
	{ 8, 16 },
	{ 8, 32 },
	{ 16, 32 },
	{ 32, 64 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
};

static struct GBAVideoRenderer dummyRenderer = {
	.init = GBAVideoDummyRendererInit,
	.reset = GBAVideoDummyRendererReset,
	.deinit = GBAVideoDummyRendererDeinit,
	.writeVideoRegister = GBAVideoDummyRendererWriteVideoRegister,
	.writeVRAM = GBAVideoDummyRendererWriteVRAM,
	.writePalette = GBAVideoDummyRendererWritePalette,
	.writeOAM = GBAVideoDummyRendererWriteOAM,
	.drawScanline = GBAVideoDummyRendererDrawScanline,
	.finishFrame = GBAVideoDummyRendererFinishFrame,
	.getPixels = GBAVideoDummyRendererGetPixels,
	.putPixels = GBAVideoDummyRendererPutPixels,
};

void GBAVideoInit(struct GBAVideo* video) {
	video->renderer = &dummyRenderer;
	video->renderer->cache = NULL;
	video->vram = 0;
	video->frameskip = 0;
	video->event.name = "GBA Video";
	video->event.callback = NULL;
	video->event.context = video;
	video->event.priority = 8;
}

void GBAVideoReset(struct GBAVideo* video) {
	if (video->p->memory.fullBios) {
		video->vcount = 0;
	} else {
		// TODO: Verify exact scanline hardware
		video->vcount = 0x7E;
	}
	video->p->memory.io[REG_VCOUNT >> 1] = video->vcount;

	video->event.callback = _startHblank;
	mTimingSchedule(&video->p->timing, &video->event, VIDEO_HDRAW_LENGTH);

	video->frameCounter = 0;
	video->frameskipCounter = 0;

	if (video->vram) {
		mappedMemoryFree(video->vram, SIZE_VRAM);
	}
	video->vram = anonymousMemoryMap(SIZE_VRAM);
	video->renderer->vram = video->vram;

	memset(video->palette, 0, sizeof(video->palette));
	memset(video->oam.raw, 0, sizeof(video->oam.raw));

	video->renderer->deinit(video->renderer);
	video->renderer->init(video->renderer);
}

void GBAVideoDeinit(struct GBAVideo* video) {
	GBAVideoAssociateRenderer(video, &dummyRenderer);
	mappedMemoryFree(video->vram, SIZE_VRAM);
}

void GBAVideoAssociateRenderer(struct GBAVideo* video, struct GBAVideoRenderer* renderer) {
	video->renderer->deinit(video->renderer);
	renderer->cache = video->renderer->cache;
	video->renderer = renderer;
	renderer->palette = video->palette;
	renderer->vram = video->vram;
	renderer->oam = &video->oam;
	video->renderer->init(video->renderer);
}

void _startHdraw(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBAVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->memory.io[REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATClearInHblank(dispstat);
	video->event.callback = _startHblank;
	mTimingSchedule(timing, &video->event, VIDEO_HDRAW_LENGTH - cyclesLate);

	++video->vcount;
	if (video->vcount == VIDEO_VERTICAL_TOTAL_PIXELS) {
		video->vcount = 0;
	}
	video->p->memory.io[REG_VCOUNT >> 1] = video->vcount;

	if (video->vcount == GBARegisterDISPSTATGetVcountSetting(dispstat)) {
		dispstat = GBARegisterDISPSTATFillVcounter(dispstat);
		if (GBARegisterDISPSTATIsVcounterIRQ(dispstat)) {
			GBARaiseIRQ(video->p, IRQ_VCOUNTER);
		}
	} else {
		dispstat = GBARegisterDISPSTATClearVcounter(dispstat);
	}
	video->p->memory.io[REG_DISPSTAT >> 1] = dispstat;

	// Note: state may be recorded during callbacks, so ensure it is consistent!
	switch (video->vcount) {
	case 0:
		GBAFrameStarted(video->p);
		break;
	case VIDEO_VERTICAL_PIXELS:
		video->p->memory.io[REG_DISPSTAT >> 1] = GBARegisterDISPSTATFillInVblank(dispstat);
		if (video->frameskipCounter <= 0) {
			video->renderer->finishFrame(video->renderer);
		}
		GBADMARunVblank(video->p, -cyclesLate);
		if (GBARegisterDISPSTATIsVblankIRQ(dispstat)) {
			GBARaiseIRQ(video->p, IRQ_VBLANK);
		}
		GBAFrameEnded(video->p);
		mCoreSyncPostFrame(video->p->sync);
		--video->frameskipCounter;
		if (video->frameskipCounter < 0) {
			video->frameskipCounter = video->frameskip;
		}
		++video->frameCounter;
		break;
	case VIDEO_VERTICAL_TOTAL_PIXELS - 1:
		video->p->memory.io[REG_DISPSTAT >> 1] = GBARegisterDISPSTATClearInVblank(dispstat);
		break;
	}
}

void _startHblank(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBAVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->memory.io[REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);
	video->event.callback = _startHdraw;
	mTimingSchedule(timing, &video->event, VIDEO_HBLANK_LENGTH - cyclesLate);

	// Begin Hblank
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);
	if (video->vcount < VIDEO_VERTICAL_PIXELS && video->frameskipCounter <= 0) {
		video->renderer->drawScanline(video->renderer, video->vcount);
	}

	if (video->vcount < VIDEO_VERTICAL_PIXELS) {
		GBADMARunHblank(video->p, -cyclesLate);
	}
	if (GBARegisterDISPSTATIsHblankIRQ(dispstat)) {
		GBARaiseIRQ(video->p, IRQ_HBLANK);
	}
	video->p->memory.io[REG_DISPSTAT >> 1] = dispstat;
}

void GBAVideoWriteDISPSTAT(struct GBAVideo* video, uint16_t value) {
	video->p->memory.io[REG_DISPSTAT >> 1] &= 0x7;
	video->p->memory.io[REG_DISPSTAT >> 1] |= value;
	// TODO: Does a VCounter IRQ trigger on write?
}

static void GBAVideoDummyRendererInit(struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBAVideoDummyRendererReset(struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBAVideoDummyRendererDeinit(struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static uint16_t GBAVideoDummyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	UNUSED(renderer);
	switch (address) {
	case REG_DISPCNT:
		value &= 0xFFF7;
		break;
	case REG_BG0CNT:
	case REG_BG1CNT:
		value &= 0xDFFF;
		break;
	case REG_BG2CNT:
	case REG_BG3CNT:
		value &= 0xFFFF;
		break;
	case REG_BG0HOFS:
	case REG_BG0VOFS:
	case REG_BG1HOFS:
	case REG_BG1VOFS:
	case REG_BG2HOFS:
	case REG_BG2VOFS:
	case REG_BG3HOFS:
	case REG_BG3VOFS:
		value &= 0x01FF;
		break;
	case REG_BLDCNT:
		value &= 0x3FFF;
		break;
	case REG_BLDALPHA:
		value &= 0x1F1F;
		break;
	case REG_WININ:
	case REG_WINOUT:
		value &= 0x3F3F;
		break;
	default:
		break;
	}
	return value;
}

static void GBAVideoDummyRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	if (renderer->cache) {
		mTileCacheWriteVRAM(renderer->cache, address);
	}
}

static void GBAVideoDummyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	UNUSED(value);
	if (renderer->cache) {
		mTileCacheWritePalette(renderer->cache, address);
	}
}

static void GBAVideoDummyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	UNUSED(renderer);
	UNUSED(oam);
	// Nothing to do
}

static void GBAVideoDummyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	UNUSED(renderer);
	UNUSED(y);
	// Nothing to do
}

static void GBAVideoDummyRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void GBAVideoDummyRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
	// Nothing to do
}

static void GBAVideoDummyRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
	// Nothing to do
}

void GBAVideoSerialize(const struct GBAVideo* video, struct GBASerializedState* state) {
	memcpy(state->vram, video->vram, SIZE_VRAM);
	memcpy(state->oam, video->oam.raw, SIZE_OAM);
	memcpy(state->pram, video->palette, SIZE_PALETTE_RAM);
	STORE_32(video->event.when - mTimingCurrentTime(&video->p->timing), 0, &state->video.nextEvent);
	STORE_32(video->frameCounter, 0, &state->video.frameCounter);
}

void GBAVideoDeserialize(struct GBAVideo* video, const struct GBASerializedState* state) {
	memcpy(video->vram, state->vram, SIZE_VRAM);
	uint16_t value;
	int i;
	for (i = 0; i < SIZE_OAM; i += 2) {
		LOAD_16(value, i, state->oam);
		GBAStore16(video->p->cpu, BASE_OAM | i, value, 0);
	}
	for (i = 0; i < SIZE_PALETTE_RAM; i += 2) {
		LOAD_16(value, i, state->pram);
		GBAStore16(video->p->cpu, BASE_PALETTE_RAM | i, value, 0);
	}
	LOAD_32(video->frameCounter, 0, &state->video.frameCounter);

	uint32_t when;
	LOAD_32(when, 0, &state->video.nextEvent);
	GBARegisterDISPSTAT dispstat = video->p->memory.io[REG_DISPSTAT >> 1];
	if (GBARegisterDISPSTATIsInHblank(dispstat)) {
		video->event.callback = _startHdraw;
	} else {
		video->event.callback = _startHblank;
	}
	mTimingSchedule(&video->p->timing, &video->event, when);

	LOAD_16(video->vcount, REG_VCOUNT, state->io);
	video->renderer->reset(video->renderer);
}
