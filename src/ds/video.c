/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/video.h>

#include <mgba/core/sync.h>
#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/memory.h>
#include <mgba/internal/gba/video.h>

#include <mgba-util/memory.h>

mLOG_DEFINE_CATEGORY(DS_VIDEO, "DS Video");

static void DSVideoDummyRendererInit(struct DSVideoRenderer* renderer);
static void DSVideoDummyRendererReset(struct DSVideoRenderer* renderer);
static void DSVideoDummyRendererDeinit(struct DSVideoRenderer* renderer);
static uint16_t DSVideoDummyRendererWriteVideoRegister(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
static void DSVideoDummyRendererWritePalette(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
static void DSVideoDummyRendererDrawScanline(struct DSVideoRenderer* renderer, int y);
static void DSVideoDummyRendererFinishFrame(struct DSVideoRenderer* renderer);
static void DSVideoDummyRendererGetPixels(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels);
static void DSVideoDummyRendererPutPixels(struct DSVideoRenderer* renderer, size_t stride, const void* pixels);

static void _startHblank7(struct mTiming*, void* context, uint32_t cyclesLate);
static void _startHdraw7(struct mTiming*, void* context, uint32_t cyclesLate);
static void _startHblank9(struct mTiming*, void* context, uint32_t cyclesLate);
static void _startHdraw9(struct mTiming*, void* context, uint32_t cyclesLate);

static const uint32_t _vramSize[9] = {
	0x20000,
	0x20000,
	0x20000,
	0x20000,
	0x10000,
	0x04000,
	0x04000,
	0x08000,
	0x04000
};

const struct DSVRAMBankInfo {
	int base;
	uint32_t mirrorSize;
	int mode;
	int offset[4];
} _vramInfo[9][8] = {
	{ // A
		{ 0x000, 0x40, 4 }, // LCDC
		{ 0x000, 0x20, 0, { 0x00, 0x08, 0x10, 0x18 } }, // A-BG
		{ 0x000, 0x10, 2, { 0x00, 0x08, 0x80, 0x80 } }, // A-OBJ
	},
	{ // B
		{ 0x008, 0x40, 4 }, // LCDC
		{ 0x000, 0x20, 0, { 0x00, 0x08, 0x10, 0x18 } }, // A-BG
		{ 0x000, 0x10, 2, { 0x00, 0x08, 0x80, 0x80 } }, // A-OBJ
	},
	{ // C
		{ 0x010, 0x40, 4 }, // LCDC
		{ 0x000, 0x20, 0, { 0x00, 0x08, 0x10, 0x18 } }, // A-BG
		{},
		{},
		{ 0x000, 0x08, 1 }, // B-BG
	},
	{ // D
		{ 0x018, 0x40, 4 }, // LCDC
		{ 0x000, 0x20, 0, { 0x00, 0x08, 0x10, 0x18 } }, // A-BG
		{},
		{},
		{ 0x000, 0x08, 3 }, // B-OBJ
	},
	{ // E
		{ 0x020, 0x40, 4 }, // LCDC
		{ 0x000, 0x20, 0 }, // A-BG
		{ 0x000, 0x10, 2 }, // A-OBJ
	},
	{ // F
		{ 0x024, 0x40, 4 }, // LCDC
		{ 0x000, 0x20, 0, { 0x00, 0x01, 0x04, 0x05 } }, // A-BG
		{ 0x000, 0x10, 2, { 0x00, 0x01, 0x04, 0x05 } }, // A-OBJ
	},
	{ // G
		{ 0x025, 0x40, 4 }, // LCDC
		{ 0x000, 0x20, 0 }, // A-BG
		{ 0x000, 0x10, 2 }, // A-OBJ
	},
	{ // H
		{ 0x026, 0x40, 4 }, // LCDC
		{ 0x000, 0x04, 1 }, // B-BG
		{ 0x000, 0x10, 2 }, // A-OBJ
	},
	{ // I
		{ 0x028, 0x40, 4 }, // LCDC
		{ 0x002, 0x04, 1 }, // B-BG
		{ 0x000, 0x01, 3 }, // B-OBJ
	},
};

static struct DSVideoRenderer dummyRenderer = {
	.init = DSVideoDummyRendererInit,
	.reset = DSVideoDummyRendererReset,
	.deinit = DSVideoDummyRendererDeinit,
	.writeVideoRegister = DSVideoDummyRendererWriteVideoRegister,
	.writePalette = DSVideoDummyRendererWritePalette,
	.drawScanline = DSVideoDummyRendererDrawScanline,
	.finishFrame = DSVideoDummyRendererFinishFrame,
	.getPixels = DSVideoDummyRendererGetPixels,
	.putPixels = DSVideoDummyRendererPutPixels,
};

void DSVideoInit(struct DSVideo* video) {
	video->renderer = &dummyRenderer;
	video->vram = NULL;
	video->frameskip = 0;
	video->event7.name = "DS7 Video";
	video->event7.callback = NULL;
	video->event7.context = video;
	video->event7.priority = 8;
	video->event9.name = "DS9 Video";
	video->event9.callback = NULL;
	video->event9.context = video;
	video->event9.priority = 8;
}

void DSVideoReset(struct DSVideo* video) {
	video->vcount = 0;
	video->p->ds7.memory.io[DS_REG_VCOUNT >> 1] = video->vcount;
	video->p->ds9.memory.io[DS_REG_VCOUNT >> 1] = video->vcount;

	video->event7.callback = _startHblank7;
	video->event9.callback = _startHblank9;
	mTimingSchedule(&video->p->ds7.timing, &video->event7, DS_VIDEO_HORIZONTAL_LENGTH - DS7_VIDEO_HBLANK_LENGTH);
	mTimingSchedule(&video->p->ds9.timing, &video->event9, (DS_VIDEO_HORIZONTAL_LENGTH - DS9_VIDEO_HBLANK_LENGTH) * 2);

	video->frameCounter = 0;
	video->frameskipCounter = 0;

	if (video->vram) {
		mappedMemoryFree(video->vram, DS_SIZE_VRAM);
	}
	video->vram = anonymousMemoryMap(DS_SIZE_VRAM);
	video->renderer->vram = video->vram;

	video->p->memory.vramBank[0] = &video->vram[0x00000];
	video->p->memory.vramBank[1] = &video->vram[0x10000];
	video->p->memory.vramBank[2] = &video->vram[0x20000];
	video->p->memory.vramBank[3] = &video->vram[0x30000];
	video->p->memory.vramBank[4] = &video->vram[0x40000];
	video->p->memory.vramBank[5] = &video->vram[0x48000];
	video->p->memory.vramBank[6] = &video->vram[0x4A000];
	video->p->memory.vramBank[7] = &video->vram[0x4C000];
	video->p->memory.vramBank[8] = &video->vram[0x50000];

	video->renderer->deinit(video->renderer);
	video->renderer->init(video->renderer);
}

void DSVideoAssociateRenderer(struct DSVideo* video, struct DSVideoRenderer* renderer) {
	video->renderer->deinit(video->renderer);
	video->renderer = renderer;
	renderer->palette = video->palette;
	renderer->vram = video->vram;
	memcpy(renderer->vramABG, video->vramABG, sizeof(renderer->vramABG));
	memcpy(renderer->vramAOBJ, video->vramAOBJ, sizeof(renderer->vramAOBJ));
	memcpy(renderer->vramBBG, video->vramBBG, sizeof(renderer->vramBBG));
	memcpy(renderer->vramBOBJ, video->vramBOBJ, sizeof(renderer->vramBOBJ));
	renderer->oam = &video->oam;
	video->renderer->init(video->renderer);
}

void DSVideoDeinit(struct DSVideo* video) {
	DSVideoAssociateRenderer(video, &dummyRenderer);
	mappedMemoryFree(video->vram, DS_SIZE_VRAM);
}

void _startHdraw7(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATClearInHblank(dispstat);
	video->event7.callback = _startHblank7;
	mTimingSchedule(timing, &video->event7, DS_VIDEO_HORIZONTAL_LENGTH - DS7_VIDEO_HBLANK_LENGTH - cyclesLate);

	video->p->ds7.memory.io[DS_REG_VCOUNT >> 1] = video->vcount;

	if (video->vcount == GBARegisterDISPSTATGetVcountSetting(dispstat)) {
		dispstat = GBARegisterDISPSTATFillVcounter(dispstat);
		if (GBARegisterDISPSTATIsVcounterIRQ(dispstat)) {
			DSRaiseIRQ(video->p->ds7.cpu, video->p->ds7.memory.io, DS_IRQ_VCOUNTER);
		}
	} else {
		dispstat = GBARegisterDISPSTATClearVcounter(dispstat);
	}
	video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1] = dispstat;

	switch (video->vcount) {
	case DS_VIDEO_VERTICAL_PIXELS:
		video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1] = GBARegisterDISPSTATFillInVblank(dispstat);
		if (GBARegisterDISPSTATIsVblankIRQ(dispstat)) {
			DSRaiseIRQ(video->p->ds7.cpu, video->p->ds7.memory.io, DS_IRQ_VBLANK);
		}
		break;
	case DS_VIDEO_VERTICAL_TOTAL_PIXELS - 1:
		video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1] = GBARegisterDISPSTATClearInVblank(dispstat);
		break;
	}
}

void _startHblank7(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);
	video->event7.callback = _startHdraw7;
	mTimingSchedule(timing, &video->event7, DS7_VIDEO_HBLANK_LENGTH - cyclesLate);

	// Begin Hblank
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);

	if (GBARegisterDISPSTATIsHblankIRQ(dispstat)) {
		DSRaiseIRQ(video->p->ds7.cpu, video->p->ds7.memory.io, DS_IRQ_HBLANK);
	}
	video->p->ds7.memory.io[DS_REG_DISPSTAT >> 1] = dispstat;
}

void _startHdraw9(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATClearInHblank(dispstat);
	video->event9.callback = _startHblank9;
	mTimingSchedule(timing, &video->event9, (DS_VIDEO_HORIZONTAL_LENGTH - DS9_VIDEO_HBLANK_LENGTH) * 2 - cyclesLate);

	++video->vcount;
	if (video->vcount == DS_VIDEO_VERTICAL_TOTAL_PIXELS) {
		video->vcount = 0;
	}
	video->p->ds9.memory.io[DS_REG_VCOUNT >> 1] = video->vcount;

	if (video->vcount == GBARegisterDISPSTATGetVcountSetting(dispstat)) {
		dispstat = GBARegisterDISPSTATFillVcounter(dispstat);
		if (GBARegisterDISPSTATIsVcounterIRQ(dispstat)) {
			DSRaiseIRQ(video->p->ds9.cpu, video->p->ds9.memory.io, DS_IRQ_VCOUNTER);
		}
	} else {
		dispstat = GBARegisterDISPSTATClearVcounter(dispstat);
	}
	video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1] = dispstat;

	// Note: state may be recorded during callbacks, so ensure it is consistent!
	switch (video->vcount) {
	case 0:
		DSFrameStarted(video->p);
		break;
	case DS_VIDEO_VERTICAL_PIXELS:
		video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1] = GBARegisterDISPSTATFillInVblank(dispstat);
		if (video->frameskipCounter <= 0) {
			video->renderer->finishFrame(video->renderer);
		}
		if (GBARegisterDISPSTATIsVblankIRQ(dispstat)) {
			DSRaiseIRQ(video->p->ds9.cpu, video->p->ds9.memory.io, DS_IRQ_VBLANK);
		}
		DSFrameEnded(video->p);
		--video->frameskipCounter;
		if (video->frameskipCounter < 0) {
			mCoreSyncPostFrame(video->p->sync);
			video->frameskipCounter = video->frameskip;
		}
		++video->frameCounter;
		break;
	case DS_VIDEO_VERTICAL_TOTAL_PIXELS - 1:
		video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1] = GBARegisterDISPSTATClearInVblank(dispstat);
		break;
	}
}

void _startHblank9(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSVideo* video = context;
	GBARegisterDISPSTAT dispstat = video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1];
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);
	video->event9.callback = _startHdraw9;
	mTimingSchedule(timing, &video->event9, (DS9_VIDEO_HBLANK_LENGTH * 2) - cyclesLate);

	// Begin Hblank
	dispstat = GBARegisterDISPSTATFillInHblank(dispstat);
	if (video->vcount < DS_VIDEO_VERTICAL_PIXELS && video->frameskipCounter <= 0) {
		video->renderer->drawScanline(video->renderer, video->vcount);
	}

	if (GBARegisterDISPSTATIsHblankIRQ(dispstat)) {
		DSRaiseIRQ(video->p->ds9.cpu, video->p->ds9.memory.io, DS_IRQ_HBLANK);
	}
	video->p->ds9.memory.io[DS_REG_DISPSTAT >> 1] = dispstat;
}

void DSVideoWriteDISPSTAT(struct DSCommon* dscore, uint16_t value) {
	dscore->memory.io[DS_REG_DISPSTAT >> 1] &= 0x7;
	dscore->memory.io[DS_REG_DISPSTAT >> 1] |= value;
	// TODO: Does a VCounter IRQ trigger on write?
}

void DSVideoConfigureVRAM(struct DS* ds, int index, uint8_t value) {
	struct DSMemory* memory = &ds->memory;
	struct DSVRAMBankInfo info = _vramInfo[index][value & 0x7];
	memset(&memory->vramMirror[index], 0, sizeof(memory->vramMirror[index]));
	memset(&memory->vramMode[index], 0, sizeof(memory->vramMode[index]));
	if (!(value & 0x80) || !info.mirrorSize) {
		return;
	}
	uint32_t size = _vramSize[index] >> DS_VRAM_OFFSET;
	memory->vramMode[index][info.mode] = 0xFFFF;
	uint32_t offset = info.base + info.offset[(value >> 3) & 3];
	uint32_t i, j;
	for (j = offset; j < 0x40; j += info.mirrorSize) {
		for (i = 0; i < size; ++i) {
			memory->vramMirror[index][i + j] = 1 << index;
		}
	}
	switch (info.mode) {
	case 0:
		for (i = 0; i < size; ++i) {
			ds->video.vramABG[offset + i] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)];
			ds->video.renderer->vramABG[offset + i] = ds->video.vramABG[offset + i];
		}
		break;
	case 1:
		for (i = 0; i < size; ++i) {
			ds->video.vramBBG[offset + i] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)];
			ds->video.renderer->vramBBG[offset + i] = ds->video.vramBBG[offset + i];
		}
		break;
	case 2:
		for (i = 0; i < size; ++i) {
			ds->video.vramAOBJ[offset + i] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)];
			ds->video.renderer->vramAOBJ[offset + i] = ds->video.vramAOBJ[offset + i];
		}
		break;
	case 3:
		for (i = 0; i < size; ++i) {
			ds->video.vramBOBJ[offset + i] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)];
			ds->video.renderer->vramBOBJ[offset + i] = ds->video.vramBOBJ[offset + i];
		}
		break;
	}
}

static void DSVideoDummyRendererInit(struct DSVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void DSVideoDummyRendererReset(struct DSVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void DSVideoDummyRendererDeinit(struct DSVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static uint16_t DSVideoDummyRendererWriteVideoRegister(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value) {
	UNUSED(renderer);
	return value;
}

static void DSVideoDummyRendererWritePalette(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value) {
	UNUSED(value);
	UNUSED(address);
	UNUSED(value);
}

static void DSVideoDummyRendererDrawScanline(struct DSVideoRenderer* renderer, int y) {
	UNUSED(renderer);
	UNUSED(y);
	// Nothing to do
}

static void DSVideoDummyRendererFinishFrame(struct DSVideoRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void DSVideoDummyRendererGetPixels(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
	// Nothing to do
}

static void DSVideoDummyRendererPutPixels(struct DSVideoRenderer* renderer, size_t stride, const void* pixels) {
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
	// Nothing to do
}
