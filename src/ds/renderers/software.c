/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/renderers/software.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/ds/io.h>

static void DSVideoSoftwareRendererInit(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererDeinit(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererReset(struct DSVideoRenderer* renderer);
static uint16_t DSVideoSoftwareRendererWriteVideoRegister(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
static void DSVideoSoftwareRendererWritePalette(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
static void DSVideoSoftwareRendererWriteOAM(struct DSVideoRenderer* renderer, uint32_t oam);
static void DSVideoSoftwareRendererDrawScanline(struct DSVideoRenderer* renderer, int y);
static void DSVideoSoftwareRendererFinishFrame(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererGetPixels(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels);
static void DSVideoSoftwareRendererPutPixels(struct DSVideoRenderer* renderer, size_t stride, const void* pixels);

void DSVideoSoftwareRendererCreate(struct DSVideoSoftwareRenderer* renderer) {
	renderer->d.init = DSVideoSoftwareRendererInit;
	renderer->d.reset = DSVideoSoftwareRendererReset;
	renderer->d.deinit = DSVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = DSVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writePalette = DSVideoSoftwareRendererWritePalette;
	renderer->d.writeOAM = DSVideoSoftwareRendererWriteOAM;
	renderer->d.drawScanline = DSVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = DSVideoSoftwareRendererFinishFrame;
	renderer->d.getPixels = DSVideoSoftwareRendererGetPixels;
	renderer->d.putPixels = DSVideoSoftwareRendererPutPixels;

	renderer->engA.d.cache = NULL;
	GBAVideoSoftwareRendererCreate(&renderer->engA);
	renderer->engB.d.cache = NULL;
	GBAVideoSoftwareRendererCreate(&renderer->engB);
}

static void DSVideoSoftwareRendererInit(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.palette = &renderer->palette[0];
	softwareRenderer->engA.d.oam = &renderer->oam->oam[0];
	softwareRenderer->engA.masterEnd = DS_VIDEO_HORIZONTAL_PIXELS;
	softwareRenderer->engA.outputBufferStride = softwareRenderer->outputBufferStride;
	softwareRenderer->engB.d.palette = &renderer->palette[512];
	softwareRenderer->engB.d.oam = &renderer->oam->oam[1];
	softwareRenderer->engB.masterEnd = DS_VIDEO_HORIZONTAL_PIXELS;
	softwareRenderer->engB.outputBufferStride = softwareRenderer->outputBufferStride;

	DSVideoSoftwareRendererReset(renderer);
}

static void DSVideoSoftwareRendererReset(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.reset(&softwareRenderer->engA.d);
	softwareRenderer->engB.d.reset(&softwareRenderer->engB.d);
}

static void DSVideoSoftwareRendererDeinit(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.deinit(&softwareRenderer->engA.d);
	softwareRenderer->engB.d.deinit(&softwareRenderer->engB.d);
}

static void GBAVideoSoftwareRendererUpdateDISPCNTA(struct DSVideoSoftwareRenderer* softwareRenderer) {
	uint16_t fakeDispcnt = softwareRenderer->dispcntA & 0xFF87;
	if (!DSRegisterDISPCNTIsTileObjMapping(softwareRenderer->dispcntA)) {
		softwareRenderer->engA.tileStride = 0x20;
	} else {
		softwareRenderer->engA.tileStride = 0x20 << DSRegisterDISPCNTGetTileBoundary(softwareRenderer->dispcntA);
		fakeDispcnt = GBARegisterDISPCNTFillObjCharacterMapping(fakeDispcnt);
	}
	softwareRenderer->engA.d.writeVideoRegister(&softwareRenderer->engA.d, DS9_REG_A_DISPCNT_LO, fakeDispcnt);
}

static void GBAVideoSoftwareRendererUpdateDISPCNTB(struct DSVideoSoftwareRenderer* softwareRenderer) {
	uint16_t fakeDispcnt = softwareRenderer->dispcntB & 0xFF87;
	if (!DSRegisterDISPCNTIsTileObjMapping(softwareRenderer->dispcntB)) {
		softwareRenderer->engB.tileStride = 0x20;
	} else {
		softwareRenderer->engB.tileStride = 0x20 << DSRegisterDISPCNTGetTileBoundary(softwareRenderer->dispcntB);
		fakeDispcnt = GBARegisterDISPCNTFillObjCharacterMapping(fakeDispcnt);
	}
	softwareRenderer->engB.d.writeVideoRegister(&softwareRenderer->engB.d, DS9_REG_A_DISPCNT_LO, fakeDispcnt);
}

static uint16_t DSVideoSoftwareRendererWriteVideoRegister(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	if (address >= DS9_REG_A_BG0CNT && address <= DS9_REG_A_BLDY) {
		value = softwareRenderer->engA.d.writeVideoRegister(&softwareRenderer->engA.d, address, value);
	} else if (address >= DS9_REG_B_BG0CNT && address <= DS9_REG_B_BLDY) {
		value = softwareRenderer->engB.d.writeVideoRegister(&softwareRenderer->engB.d, address & 0xFF, value);
	} else {
		mLOG(DS_VIDEO, STUB, "Stub video register write: %04X:%04X", address, value);
	}
	switch (address) {
	case DS9_REG_A_DISPCNT_LO:
		softwareRenderer->dispcntA &= 0xFFFF0000;
		softwareRenderer->dispcntA |= value;
		GBAVideoSoftwareRendererUpdateDISPCNTA(softwareRenderer);
		break;
	case DS9_REG_A_DISPCNT_HI:
		softwareRenderer->dispcntA &= 0x0000FFFF;
		softwareRenderer->dispcntA |= value << 16;
		GBAVideoSoftwareRendererUpdateDISPCNTA(softwareRenderer);
		break;
	case DS9_REG_B_DISPCNT_LO:
		softwareRenderer->dispcntB &= 0xFFFF0000;
		softwareRenderer->dispcntB |= value;
		GBAVideoSoftwareRendererUpdateDISPCNTB(softwareRenderer);
		break;
	case DS9_REG_B_DISPCNT_HI:
		softwareRenderer->dispcntB &= 0x0000FFFF;
		softwareRenderer->dispcntB |= value << 16;
		GBAVideoSoftwareRendererUpdateDISPCNTB(softwareRenderer);
		break;
	case DS9_REG_POWCNT1:
		value &= 0x810F;
		softwareRenderer->powcnt = value;
	}
	return value;
}

static void DSVideoSoftwareRendererWritePalette(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	if (address < 0x400) {
		softwareRenderer->engA.d.writePalette(&softwareRenderer->engA.d, address & 0x3FF, value);
	} else {
		softwareRenderer->engB.d.writePalette(&softwareRenderer->engB.d, address & 0x3FF, value);
	}
}

static void DSVideoSoftwareRendererWriteOAM(struct DSVideoRenderer* renderer, uint32_t oam) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	if (oam < 0x200) {
		softwareRenderer->engA.d.writeOAM(&softwareRenderer->engA.d, oam & 0x1FF);
	} else {
		softwareRenderer->engB.d.writeOAM(&softwareRenderer->engB.d, oam & 0x1FF);
	}
}

static void _drawScanlineA(struct DSVideoSoftwareRenderer* softwareRenderer, int y) {
	memcpy(softwareRenderer->engA.d.vramBG, softwareRenderer->d.vramABG, sizeof(softwareRenderer->engA.d.vramBG));
	memcpy(softwareRenderer->engA.d.vramOBJ, softwareRenderer->d.vramAOBJ, sizeof(softwareRenderer->engA.d.vramOBJ));
	color_t* row = &softwareRenderer->engA.outputBuffer[softwareRenderer->outputBufferStride * y];

	int x;
	switch (DSRegisterDISPCNTGetDispMode(softwareRenderer->dispcntA)) {
	case 0:
		for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GBA_COLOR_WHITE;
		}
		return;
	case 1:
		softwareRenderer->engA.d.drawScanline(&softwareRenderer->engA.d, y);
		return;
	case 2: {
		uint16_t* vram = &softwareRenderer->d.vram[0x10000 * DSRegisterDISPCNTGetVRAMBlock(softwareRenderer->dispcntA)];
		for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			color_t color;
			LOAD_16(color, (x + y * DS_VIDEO_HORIZONTAL_PIXELS) * 2, vram);
#ifndef COLOR_16_BIT
			unsigned color32 = 0;
			color32 |= (color << 9) & 0xF80000;
			color32 |= (color << 3) & 0xF8;
			color32 |= (color << 6) & 0xF800;
			color32 |= (color32 >> 5) & 0x070707;
			color = color32;
#elif COLOR_5_6_5
			uint16_t color16 = 0;
			color16 |= (color & 0x001F) << 11;
			color16 |= (color & 0x03E0) << 1;
			color16 |= (color & 0x7C00) >> 10;
			color = color16;
#endif
			softwareRenderer->row[x] = color;
		}
		break;
	}
	case 3:
		break;
	}

#ifdef COLOR_16_BIT
#if defined(__ARM_NEON) && !defined(__APPLE__)
	_to16Bit(row, softwareRenderer->row, DS_VIDEO_HORIZONTAL_PIXELS);
#else
	for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
		row[x] = softwareRenderer->row[x];
	}
#endif
#else
	memcpy(row, softwareRenderer->row, DS_VIDEO_HORIZONTAL_PIXELS * sizeof(*row));
#endif
}

static void _drawScanlineB(struct DSVideoSoftwareRenderer* softwareRenderer, int y) {
	memcpy(softwareRenderer->engB.d.vramBG, softwareRenderer->d.vramBBG, sizeof(softwareRenderer->engB.d.vramBG));
	memcpy(softwareRenderer->engB.d.vramOBJ, softwareRenderer->d.vramBOBJ, sizeof(softwareRenderer->engB.d.vramOBJ));
	color_t* row = &softwareRenderer->engB.outputBuffer[softwareRenderer->outputBufferStride * y];

	int x;
	switch (DSRegisterDISPCNTGetDispMode(softwareRenderer->dispcntB)) {
	case 0:
		for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GBA_COLOR_WHITE;
		}
		return;
	case 1:
		softwareRenderer->engB.d.drawScanline(&softwareRenderer->engB.d, y);
		return;
	}

#ifdef COLOR_16_BIT
#if defined(__ARM_NEON) && !defined(__APPLE__)
	_to16Bit(row, softwareRenderer->row, DS_VIDEO_HORIZONTAL_PIXELS);
#else
	for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
		row[x] = softwareRenderer->row[x];
	}
#endif
#else
	memcpy(row, softwareRenderer->row, DS_VIDEO_HORIZONTAL_PIXELS * sizeof(*row));
#endif
}

static void DSVideoSoftwareRendererDrawScanline(struct DSVideoRenderer* renderer, int y) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	if (!DSRegisterPOWCNT1IsSwap(softwareRenderer->powcnt)) {
		softwareRenderer->engA.outputBuffer = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * DS_VIDEO_VERTICAL_PIXELS];
		softwareRenderer->engB.outputBuffer = softwareRenderer->outputBuffer;
	} else {
		softwareRenderer->engA.outputBuffer = softwareRenderer->outputBuffer;
		softwareRenderer->engB.outputBuffer = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * DS_VIDEO_VERTICAL_PIXELS];
	}

	_drawScanlineA(softwareRenderer, y);
	_drawScanlineB(softwareRenderer, y);
}

static void DSVideoSoftwareRendererFinishFrame(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.finishFrame(&softwareRenderer->engA.d);
	softwareRenderer->engB.d.finishFrame(&softwareRenderer->engB.d);
}

static void DSVideoSoftwareRendererGetPixels(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
#ifdef COLOR_16_BIT
#error Not yet supported
#else
	*stride = softwareRenderer->outputBufferStride;
	*pixels = softwareRenderer->outputBuffer;
#endif
}

static void DSVideoSoftwareRendererPutPixels(struct DSVideoRenderer* renderer, size_t stride, const void* pixels) {
}
