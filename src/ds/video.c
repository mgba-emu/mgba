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
static void DSVideoDummyRendererWriteOAM(struct DSVideoRenderer* renderer, uint32_t oam);
static void DSVideoDummyRendererInvalidateExtPal(struct DSVideoRenderer* renderer, bool obj, bool engB, int slot);
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

enum DSVRAMBankMode {
	MODE_A_BG = 0,
	MODE_B_BG = 1,
	MODE_A_OBJ = 2,
	MODE_B_OBJ = 3,
	MODE_LCDC,
	MODE_7_VRAM,
	MODE_A_BG_EXT_PAL,
	MODE_B_BG_EXT_PAL,
	MODE_A_OBJ_EXT_PAL,
	MODE_B_OBJ_EXT_PAL,
	MODE_3D_TEX,
	MODE_3D_TEX_PAL,
};

const struct DSVRAMBankInfo {
	int base;
	uint32_t mirrorSize;
	enum DSVRAMBankMode mode;
	int offset[4];
} _vramInfo[9][8] = {
	{ // A
		{ 0x000, 0x40, MODE_LCDC },
		{ 0x000, 0x20, MODE_A_BG, { 0x00, 0x08, 0x10, 0x18 } },
		{ 0x000, 0x10, MODE_A_OBJ, { 0x00, 0x08, 0x80, 0x80 } },
		{ 0x000, 0x01, MODE_3D_TEX, { 0x00, 0x01, 0x02, 0x03 } },
	},
	{ // B
		{ 0x008, 0x40, MODE_LCDC },
		{ 0x000, 0x20, MODE_A_BG, { 0x00, 0x08, 0x10, 0x18 } },
		{ 0x000, 0x10, MODE_A_OBJ, { 0x00, 0x08, 0x80, 0x80 } },
		{ 0x000, 0x01, MODE_3D_TEX, { 0x00, 0x01, 0x02, 0x03 } },
	},
	{ // C
		{ 0x010, 0x40, MODE_LCDC },
		{ 0x000, 0x20, MODE_A_BG, { 0x00, 0x08, 0x10, 0x18 } },
		{ 0x000, 0x40, MODE_7_VRAM, { 0x00, 0x08, 0x80, 0x80 } },
		{ 0x000, 0x01, MODE_3D_TEX, { 0x00, 0x01, 0x02, 0x03 } },
		{ 0x000, 0x08, MODE_B_BG },
	},
	{ // D
		{ 0x018, 0x40, MODE_LCDC },
		{ 0x000, 0x20, MODE_A_BG, { 0x00, 0x08, 0x10, 0x18 } },
		{ 0x000, 0x40, MODE_7_VRAM, { 0x00, 0x08, 0x80, 0x80 } },
		{ 0x000, 0x01, MODE_3D_TEX, { 0x00, 0x01, 0x02, 0x03 } },
		{ 0x000, 0x08, MODE_B_OBJ },
	},
	{ // E
		{ 0x020, 0x40, MODE_LCDC },
		{ 0x000, 0x20, MODE_A_BG },
		{ 0x000, 0x10, MODE_A_OBJ },
		{ 0x000, 0x04, MODE_3D_TEX_PAL },
		{ 0x000, 0x04, MODE_A_BG_EXT_PAL },
	},
	{ // F
		{ 0x024, 0x40, MODE_LCDC },
		{ 0x000, 0x20, MODE_A_BG, { 0x00, 0x01, 0x04, 0x05 } },
		{ 0x000, 0x10, MODE_A_OBJ, { 0x00, 0x01, 0x04, 0x05 } },
		{ 0x000, 0x01, MODE_3D_TEX_PAL, { 0x00, 0x01, 0x04, 0x05 } },
		{ 0x000, 0x02, MODE_A_BG_EXT_PAL, { 0x00, 0x02, 0x00, 0x02 } },
		{ 0x000, 0x01, MODE_A_OBJ_EXT_PAL},
	},
	{ // G
		{ 0x025, 0x40, MODE_LCDC },
		{ 0x000, 0x20, MODE_A_BG, { 0x00, 0x01, 0x04, 0x05 } },
		{ 0x000, 0x10, MODE_A_OBJ, { 0x00, 0x01, 0x04, 0x05 } },
		{ 0x000, 0x01, MODE_3D_TEX_PAL, { 0x00, 0x01, 0x04, 0x05 } },
		{ 0x000, 0x02, MODE_A_BG_EXT_PAL, { 0x00, 0x02, 0x00, 0x02 } },
		{ 0x000, 0x01, MODE_A_OBJ_EXT_PAL},
	},
	{ // H
		{ 0x026, 0x40, MODE_LCDC },
		{ 0x000, 0x04, MODE_B_BG },
		{ 0x000, 0x04, MODE_B_BG_EXT_PAL },
	},
	{ // I
		{ 0x028, 0x40, MODE_LCDC },
		{ 0x002, 0x04, MODE_B_BG },
		{ 0x000, 0x01, MODE_B_OBJ },
		{ 0x000, 0x01, MODE_B_OBJ_EXT_PAL },
	},
};

static struct DSVideoRenderer dummyRenderer = {
	.init = DSVideoDummyRendererInit,
	.reset = DSVideoDummyRendererReset,
	.deinit = DSVideoDummyRendererDeinit,
	.writeVideoRegister = DSVideoDummyRendererWriteVideoRegister,
	.writePalette = DSVideoDummyRendererWritePalette,
	.writeOAM = DSVideoDummyRendererWriteOAM,
	.invalidateExtPal = DSVideoDummyRendererInvalidateExtPal,
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
	memcpy(renderer->vramABGExtPal, video->vramABGExtPal, sizeof(renderer->vramABGExtPal));
	renderer->vramAOBJExtPal = video->vramAOBJExtPal;
	memcpy(renderer->vramBBG, video->vramBBG, sizeof(renderer->vramBBG));
	memcpy(renderer->vramBOBJ, video->vramBOBJ, sizeof(renderer->vramBOBJ));
	memcpy(renderer->vramBBGExtPal, video->vramBBGExtPal, sizeof(renderer->vramBBGExtPal));
	renderer->vramBOBJExtPal = video->vramBOBJExtPal;
	renderer->oam = &video->oam;
	renderer->gx = &video->p->gx;
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
			DSGXFlush(&video->p->gx);
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
	if (video->frameskipCounter <= 0) {
		if (video->vcount < DS_VIDEO_VERTICAL_PIXELS) {
			video->renderer->drawScanline(video->renderer, video->vcount);
		}
		if (video->vcount < DS_VIDEO_VERTICAL_PIXELS - 48) {
			video->p->gx.renderer->drawScanline(video->p->gx.renderer, video->vcount + 48);
		}
		if (video->vcount >= DS_VIDEO_VERTICAL_TOTAL_PIXELS - 48) {
			video->p->gx.renderer->drawScanline(video->p->gx.renderer, video->vcount + 48 - DS_VIDEO_VERTICAL_TOTAL_PIXELS);
		}
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

void DSVideoConfigureVRAM(struct DS* ds, int index, uint8_t value, uint8_t oldValue) {
	struct DSMemory* memory = &ds->memory;
	if (value == oldValue) {
		return;
	}
	uint32_t i, j;
	uint32_t size = _vramSize[index] >> DS_VRAM_OFFSET;
	struct DSVRAMBankInfo oldInfo = _vramInfo[index][oldValue & 0x7];
	uint32_t offset = oldInfo.base + oldInfo.offset[(oldValue >> 3) & 3];
	switch (oldInfo.mode) {
	case MODE_A_BG:
		for (i = 0; i < size; ++i) {
			if (ds->video.vramABG[offset + i] == &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)]) {
				ds->video.vramABG[offset + i] = NULL;
				ds->video.renderer->vramABG[offset + i] = NULL;
			}
		}
		break;
	case MODE_B_BG:
		for (i = 0; i < size; ++i) {
			if (ds->video.vramBBG[offset + i] == &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)]) {
				ds->video.vramBBG[offset + i] = NULL;
				ds->video.renderer->vramBBG[offset + i] = NULL;
			}
		}
		break;
	case MODE_A_OBJ:
		for (i = 0; i < size; ++i) {
			if (ds->video.vramAOBJ[offset + i] == &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)]) {
				ds->video.vramAOBJ[offset + i] = NULL;
				ds->video.renderer->vramAOBJ[offset + i] = NULL;
			}
		}
		break;
	case MODE_B_OBJ:
		for (i = 0; i < size; ++i) {
			if (ds->video.vramBOBJ[offset + i] == &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)]) {
				ds->video.vramBOBJ[offset + i] = NULL;
				ds->video.renderer->vramBOBJ[offset + i] = NULL;
			}
		}
		break;
	case MODE_A_BG_EXT_PAL:
		for (i = 0; i < oldInfo.mirrorSize; ++i) {
			if (ds->video.vramABGExtPal[offset + i] == &memory->vramBank[index][i << 12]) {
				ds->video.vramABGExtPal[offset + i] = NULL;
				ds->video.renderer->vramABGExtPal[offset + i] = NULL;
				ds->video.renderer->invalidateExtPal(ds->video.renderer, false, false, offset + i);
			}
		}
		break;
	case MODE_B_BG_EXT_PAL:
		for (i = 0; i < oldInfo.mirrorSize; ++i) {
			if (ds->video.vramBBGExtPal[offset + i] == &memory->vramBank[index][i << 12]) {
				ds->video.vramBBGExtPal[offset + i] = NULL;
				ds->video.renderer->vramBBGExtPal[offset + i] = NULL;
				ds->video.renderer->invalidateExtPal(ds->video.renderer, false, true, offset + i);
			}
		}
		break;
	case MODE_A_OBJ_EXT_PAL:
		if (ds->video.vramAOBJExtPal == memory->vramBank[index]) {
			ds->video.vramAOBJExtPal = NULL;
			ds->video.renderer->vramAOBJExtPal = NULL;
			ds->video.renderer->invalidateExtPal(ds->video.renderer, true, false, 0);
		}
		break;
	case MODE_B_OBJ_EXT_PAL:
		if (ds->video.vramBOBJExtPal == memory->vramBank[index]) {
			ds->video.vramBOBJExtPal = NULL;
			ds->video.renderer->vramBOBJExtPal = NULL;
			ds->video.renderer->invalidateExtPal(ds->video.renderer, true, true, 0);
		}
		break;
	case MODE_3D_TEX:
		if (ds->gx.tex[offset] == memory->vramBank[index]) {
			ds->gx.tex[offset] = NULL;
			ds->gx.renderer->tex[offset] = NULL;
			ds->gx.renderer->invalidateTex(ds->gx.renderer, offset);
		}
		break;
	case MODE_3D_TEX_PAL:
		for (i = 0; i < oldInfo.mirrorSize; ++i) {
			if (ds->gx.texPal[offset + i] == &memory->vramBank[index][i << 13]) {
				ds->gx.texPal[offset + i] = NULL;
				ds->gx.renderer->texPal[offset + i] = NULL;
			}
		}
		break;
	case MODE_7_VRAM:
		for (i = 0; i < size; i += 16) {
			ds->memory.vram7[(offset + i) >> 4] = NULL;
		}
		break;
	case MODE_LCDC:
		break;
	}

	struct DSVRAMBankInfo info = _vramInfo[index][value & 0x7];
	memset(&memory->vramMirror[index], 0, sizeof(memory->vramMirror[index]));
	memset(&memory->vramMode[index], 0, sizeof(memory->vramMode[index]));
	if (!(value & 0x80) || !info.mirrorSize) {
		return;
	}
	offset = info.base + info.offset[(value >> 3) & 3];
	if (info.mode <= MODE_LCDC) {
		memory->vramMode[index][info.mode] = 0xFFFF;
		for (j = offset; j < 0x40; j += info.mirrorSize) {
			for (i = 0; i < size; ++i) {
				memory->vramMirror[index][i + j] = 1 << index;
			}
		}
	}
	switch (info.mode) {
	case MODE_A_BG:
		for (i = 0; i < size; ++i) {
			ds->video.vramABG[offset + i] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)];
			ds->video.renderer->vramABG[offset + i] = ds->video.vramABG[offset + i];
		}
		break;
	case MODE_B_BG:
		for (i = 0; i < size; ++i) {
			ds->video.vramBBG[offset + i] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)];
			ds->video.renderer->vramBBG[offset + i] = ds->video.vramBBG[offset + i];
		}
		break;
	case MODE_A_OBJ:
		for (i = 0; i < size; ++i) {
			ds->video.vramAOBJ[offset + i] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)];
			ds->video.renderer->vramAOBJ[offset + i] = ds->video.vramAOBJ[offset + i];
		}
		break;
	case MODE_B_OBJ:
		for (i = 0; i < size; ++i) {
			ds->video.vramBOBJ[offset + i] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 1)];
			ds->video.renderer->vramBOBJ[offset + i] = ds->video.vramBOBJ[offset + i];
		}
		break;
	case MODE_A_BG_EXT_PAL:
		for (i = 0; i < info.mirrorSize; ++i) {
			ds->video.vramABGExtPal[offset + i] = &memory->vramBank[index][i << 12];
			ds->video.renderer->vramABGExtPal[offset + i] = ds->video.vramABGExtPal[offset + i];
			ds->video.renderer->invalidateExtPal(ds->video.renderer, false, false, offset + i);
		}
		break;
	case MODE_B_BG_EXT_PAL:
		for (i = 0; i < info.mirrorSize; ++i) {
			ds->video.vramBBGExtPal[offset + i] = &memory->vramBank[index][i << 12];
			ds->video.renderer->vramBBGExtPal[offset + i] = ds->video.vramBBGExtPal[offset + i];
			ds->video.renderer->invalidateExtPal(ds->video.renderer, false, true, offset + i);
		}
		break;
	case MODE_A_OBJ_EXT_PAL:
		ds->video.vramAOBJExtPal = memory->vramBank[index];
		ds->video.renderer->vramAOBJExtPal = ds->video.vramAOBJExtPal;
		ds->video.renderer->invalidateExtPal(ds->video.renderer, true, false, 0);
		break;
	case MODE_B_OBJ_EXT_PAL:
		ds->video.vramBOBJExtPal = memory->vramBank[index];
		ds->video.renderer->vramBOBJExtPal = ds->video.vramBOBJExtPal;
		ds->video.renderer->invalidateExtPal(ds->video.renderer, true, true, 0);
		break;
	case MODE_3D_TEX:
		ds->gx.tex[offset] = memory->vramBank[index];
		ds->gx.renderer->tex[offset] = ds->gx.tex[offset];
		ds->gx.renderer->invalidateTex(ds->gx.renderer, offset);
		break;
	case MODE_3D_TEX_PAL:
		for (i = 0; i < info.mirrorSize; ++i) {
			ds->gx.texPal[offset + i] = &memory->vramBank[index][i << 13];
			ds->gx.renderer->texPal[offset + i] = ds->gx.texPal[offset + i];
		}
		break;
	case MODE_7_VRAM:
		for (i = 0; i < size; i += 16) {
			ds->memory.vram7[(offset + i) >> 4] = &memory->vramBank[index][i << (DS_VRAM_OFFSET - 5)];
		}
		break;
	case MODE_LCDC:
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
	UNUSED(renderer);
	UNUSED(address);
	UNUSED(value);
	// Nothing to do
}

static void DSVideoDummyRendererWriteOAM(struct DSVideoRenderer* renderer, uint32_t oam) {
	UNUSED(renderer);
	UNUSED(oam);
	// Nothing to do
}

static void DSVideoDummyRendererInvalidateExtPal(struct DSVideoRenderer* renderer, bool obj, bool engB, int slot) {
	UNUSED(renderer);
	UNUSED(obj);
	UNUSED(engB);
	// Nothing to do
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
