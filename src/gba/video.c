/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/video.h>

#include <mgba/core/sync.h>
#include <mgba/core/cache-set.h>
#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/dma.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/cache-set.h>
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

MGBA_EXPORT const int GBAVideoObjSizes[16][2] = {
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

void GBAVideoInit(struct GBAVideo* video) {
	video->renderer = NULL;
	video->vram = anonymousMemoryMap(GBA_SIZE_VRAM);
	video->frameskip = 0;
	video->event.name = "GBA Video";
	video->event.callback = NULL;
	video->event.context = video;
	video->event.priority = 8;
}

void GBAVideoReset(struct GBAVideo* video) {
	int32_t nextEvent = VIDEO_HDRAW_LENGTH;
	if (video->p->memory.fullBios) {
		video->vcount = 0;
	} else {
		// TODO: Verify exact scanline on hardware
		video->vcount = 0x7E;
		nextEvent = 117;
	}
	video->p->memory.io[REG_VCOUNT >> 1] = video->vcount;

	video->event.callback = _startHblank;
	mTimingSchedule(&video->p->timing, &video->event, nextEvent);

	video->frameCounter = 0;
	video->frameskipCounter = 0;
	video->shouldStall = 0;

	memset(video->palette, 0, sizeof(video->palette));
	memset(video->oam.raw, 0, sizeof(video->oam.raw));

	if (!video->renderer) {
		mLOG(GBA_VIDEO, FATAL, "No renderer associated");
		return;
	}
	video->renderer->vram = video->vram;
	video->renderer->reset(video->renderer);
}

void GBAVideoDeinit(struct GBAVideo* video) {
	video->renderer->deinit(video->renderer);
	mappedMemoryFree(video->vram, GBA_SIZE_VRAM);
}

void GBAVideoDummyRendererCreate(struct GBAVideoRenderer* renderer) {
	static const struct GBAVideoRenderer dummyRenderer = {
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
	memcpy(renderer, &dummyRenderer, sizeof(*renderer));
}

void GBAVideoAssociateRenderer(struct GBAVideo* video, struct GBAVideoRenderer* renderer) {
	if (video->renderer) {
		video->renderer->deinit(video->renderer);
		renderer->cache = video->renderer->cache;
	} else {
		renderer->cache = NULL;
	}
	video->renderer = renderer;
	renderer->palette = video->palette;
	renderer->vram = video->vram;
	renderer->oam = &video->oam;
	video->renderer->init(video->renderer);
	video->renderer->reset(video->renderer);
	renderer->writeVideoRegister(renderer, REG_DISPCNT, video->p->memory.io[REG_DISPCNT >> 1]);
	renderer->writeVideoRegister(renderer, REG_GREENSWP, video->p->memory.io[REG_GREENSWP >> 1]);
	int address;
	for (address = REG_BG0CNT; address < 0x56; address += 2) {
		if (address == 0x4E) {
			continue;
		}
		renderer->writeVideoRegister(renderer, address, video->p->memory.io[address >> 1]);
	}
}

void _startHdraw(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBAVideo* video = context;
	video->event.callback = _startHblank;
	mTimingSchedule(timing, &video->event, VIDEO_HDRAW_LENGTH - cyclesLate);

	++video->vcount;
	if (video->vcount == VIDEO_VERTICAL_TOTAL_PIXELS) {
		video->vcount = 0;
	}
	video->p->memory.io[REG_VCOUNT >> 1] = video->vcount;

	if (video->vcount < GBA_VIDEO_VERTICAL_PIXELS) {
		video->shouldStall = 1;
	}

	GBARegisterDISPSTAT dispstat = video->p->memory.io[REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATClearInHblank(dispstat);
	if (video->vcount == GBARegisterDISPSTATGetVcountSetting(dispstat)) {
		dispstat = GBARegisterDISPSTATFillVcounter(dispstat);
		if (GBARegisterDISPSTATIsVcounterIRQ(dispstat)) {
			GBARaiseIRQ(video->p, GBA_IRQ_VCOUNTER, cyclesLate);
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
	case GBA_VIDEO_VERTICAL_PIXELS:
		video->p->memory.io[REG_DISPSTAT >> 1] = GBARegisterDISPSTATFillInVblank(dispstat);
		if (video->frameskipCounter <= 0) {
			video->renderer->finishFrame(video->renderer);
		}
		GBADMARunVblank(video->p, -cyclesLate);
		if (GBARegisterDISPSTATIsVblankIRQ(dispstat)) {
			GBARaiseIRQ(video->p, GBA_IRQ_VBLANK, cyclesLate);
		}
		GBAFrameEnded(video->p);
		mCoreSyncPostFrame(video->p->sync);
		--video->frameskipCounter;
		if (video->frameskipCounter < 0) {
			video->frameskipCounter = video->frameskip;
		}
		++video->frameCounter;
		video->p->earlyExit = true;
		break;
	case VIDEO_VERTICAL_TOTAL_PIXELS - 1:
		video->p->memory.io[REG_DISPSTAT >> 1] = GBARegisterDISPSTATClearInVblank(dispstat);
		break;
	}
}

void _startHblank(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBAVideo* video = context;
	video->event.callback = _startHdraw;
	mTimingSchedule(timing, &video->event, VIDEO_HBLANK_LENGTH - cyclesLate);

	// Begin Hblank
	GBARegisterDISPSTAT dispstat = video->p->memory.io[REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);
	if (video->vcount < GBA_VIDEO_VERTICAL_PIXELS && video->frameskipCounter <= 0) {
		video->renderer->drawScanline(video->renderer, video->vcount);
	}

	if (video->vcount < GBA_VIDEO_VERTICAL_PIXELS) {
		GBADMARunHblank(video->p, -cyclesLate);
	}
	if (video->vcount >= 2 && video->vcount < GBA_VIDEO_VERTICAL_PIXELS + 2) {
		GBADMARunDisplayStart(video->p, -cyclesLate);
	}
	if (GBARegisterDISPSTATIsHblankIRQ(dispstat)) {
		GBARaiseIRQ(video->p, GBA_IRQ_HBLANK, cyclesLate - 6); // TODO: Where does this fudge factor come from?
	}
	video->shouldStall = 0;
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
	if (renderer->cache) {
		GBAVideoCacheWriteVideoRegister(renderer->cache, address, value);
	}
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
		mCacheSetWriteVRAM(renderer->cache, address);
	}
}

static void GBAVideoDummyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	if (renderer->cache) {
		mCacheSetWritePalette(renderer->cache, address >> 1, mColorFrom555(value));
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
	memcpy(state->vram, video->vram, GBA_SIZE_VRAM);
	memcpy(state->oam, video->oam.raw, GBA_SIZE_OAM);
	memcpy(state->pram, video->palette, GBA_SIZE_PALETTE_RAM);
	STORE_32(video->event.when - mTimingCurrentTime(&video->p->timing), 0, &state->video.nextEvent);
	int32_t flags = 0;
	if (video->event.callback == _startHdraw) {
		flags = GBASerializedVideoFlagsSetMode(flags, 1);
	} else if (video->event.callback == _startHblank) {
		flags = GBASerializedVideoFlagsSetMode(flags, 2);
	}
	STORE_32(flags, 0, &state->video.flags);
	STORE_32(video->frameCounter, 0, &state->video.frameCounter);
}

void GBAVideoDeserialize(struct GBAVideo* video, const struct GBASerializedState* state) {
	memcpy(video->vram, state->vram, GBA_SIZE_VRAM);
	uint16_t value;
	int i;
	for (i = 0; i < GBA_SIZE_OAM; i += 2) {
		LOAD_16(value, i, state->oam);
		GBAStore16(video->p->cpu, GBA_BASE_OAM | i, value, 0);
	}
	for (i = 0; i < GBA_SIZE_PALETTE_RAM; i += 2) {
		LOAD_16(value, i, state->pram);
		GBAStore16(video->p->cpu, GBA_BASE_PALETTE_RAM | i, value, 0);
	}
	LOAD_32(video->frameCounter, 0, &state->video.frameCounter);

	video->shouldStall = 0;
	int32_t flags;
	LOAD_32(flags, 0, &state->video.flags);
	GBARegisterDISPSTAT dispstat = state->io[REG_DISPSTAT >> 1];
	switch (GBASerializedVideoFlagsGetMode(flags)) {
	case 0:
		if (GBARegisterDISPSTATIsInHblank(dispstat)) {
			video->event.callback = _startHdraw;
		} else {
			video->event.callback = _startHblank;
		}
		break;
	case 1:
		video->event.callback = _startHdraw;
		break;
	case 2:
		video->event.callback = _startHblank;
		video->shouldStall = 1;
		break;
	case 3:
		video->event.callback = _startHdraw;
		break;
	}
	uint32_t when;
	if (state->versionMagic < 0x01000007) {
		// This field was moved in v7
		LOAD_32(when, 0, &state->audio.lastSample);
	} else {
		LOAD_32(when, 0, &state->video.nextEvent);
	}
	mTimingSchedule(&video->p->timing, &video->event, when);

	LOAD_16(video->vcount, REG_VCOUNT, state->io);
	video->renderer->reset(video->renderer);
}
