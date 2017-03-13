/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/renderers/software.h>
#include "gba/renderers/software-private.h"

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/ds/gx.h>
#include <mgba/internal/ds/io.h>

static void DSVideoSoftwareRendererInit(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererDeinit(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererReset(struct DSVideoRenderer* renderer);
static uint16_t DSVideoSoftwareRendererWriteVideoRegister(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
static void DSVideoSoftwareRendererWritePalette(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
static void DSVideoSoftwareRendererWriteOAM(struct DSVideoRenderer* renderer, uint32_t oam);
static void DSVideoSoftwareRendererInvalidateExtPal(struct DSVideoRenderer* renderer, bool obj, bool engB, int slot);
static void DSVideoSoftwareRendererDrawScanline(struct DSVideoRenderer* renderer, int y);
static void DSVideoSoftwareRendererDrawScanlineDirectly(struct DSVideoRenderer* renderer, int y, color_t* scanline);
static void DSVideoSoftwareRendererFinishFrame(struct DSVideoRenderer* renderer);
static void DSVideoSoftwareRendererGetPixels(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels);
static void DSVideoSoftwareRendererPutPixels(struct DSVideoRenderer* renderer, size_t stride, const void* pixels);

static void DSVideoSoftwareRendererDrawBackgroundExt0(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY);
static void DSVideoSoftwareRendererDrawBackgroundExt1(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY);
static void DSVideoSoftwareRendererDrawBackgroundExt2(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY);

static bool _regenerateExtPalette(struct DSVideoSoftwareRenderer* renderer, bool obj, bool engB, int slot) {
	color_t* palette;
	color_t* variantPalette;
	struct GBAVideoSoftwareRenderer* softwareRenderer;
	uint16_t* vram;
	if (!obj) {
		if (!engB) {
			palette = &renderer->extPaletteA[slot * 4096];
			variantPalette = &renderer->variantPaletteA[slot * 4096];
			softwareRenderer = &renderer->engA;
			vram = renderer->d.vramABGExtPal[slot];
		} else {
			palette = &renderer->extPaletteB[slot * 4096];
			variantPalette = &renderer->variantPaletteB[slot * 4096];
			softwareRenderer = &renderer->engB;
			vram = renderer->d.vramBBGExtPal[slot];
		}
	} else {
		if (!engB) {
			palette = renderer->objExtPaletteA;
			variantPalette = renderer->variantPaletteA;
			softwareRenderer = &renderer->engA;
			vram = renderer->d.vramAOBJExtPal;
		} else {
			palette = renderer->objExtPaletteB;
			variantPalette = renderer->variantPaletteB;
			softwareRenderer = &renderer->engB;
			vram = renderer->d.vramBOBJExtPal;
		}
	}
	if (!vram) {
		return false;
	}
	int i;
	for (i = 0; i < 4096; ++i) {
		uint16_t value = vram[i];
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
		unsigned color = 0;
		color |= (value & 0x001F) << 11;
		color |= (value & 0x03E0) << 1;
		color |= (value & 0x7C00) >> 10;
#else
		unsigned color = value;
#endif
#else
		unsigned color = 0;
		color |= (value << 3) & 0xF8;
		color |= (value << 6) & 0xF800;
		color |= (value << 9) & 0xF80000;
		color |= (color >> 5) & 0x070707;
#endif
		palette[i] = color;
		if (softwareRenderer->blendEffect == BLEND_BRIGHTEN) {
			variantPalette[i] = _brighten(color, softwareRenderer->bldy);
		} else if (softwareRenderer->blendEffect == BLEND_DARKEN) {
			variantPalette[i] = _darken(color, softwareRenderer->bldy);
		}
	}
	if (obj) {
		softwareRenderer->objExtPalette = palette;
		softwareRenderer->objExtVariantPalette = variantPalette;
	} else {
		if (slot >= 2) {
			if (GBARegisterBGCNTIsExtPaletteSlot(softwareRenderer->bg[slot - 2].control)) {
				softwareRenderer->bg[slot - 2].extPalette = palette;
				softwareRenderer->bg[slot - 2].variantPalette = variantPalette;
			}
		} else if (slot < 2 && !GBARegisterBGCNTIsExtPaletteSlot(softwareRenderer->bg[slot].control) ) {
			softwareRenderer->bg[slot].extPalette = palette;
			softwareRenderer->bg[slot].variantPalette = variantPalette;
		}
		softwareRenderer->bg[slot].extPalette = palette;
		softwareRenderer->bg[slot].variantPalette = variantPalette;
	}
	return true;
}

static void _updateCharBase(struct DSVideoSoftwareRenderer* softwareRenderer, bool engB) {
	struct GBAVideoSoftwareRenderer* eng;
	if (!engB) {
		eng = &softwareRenderer->engA;
	} else {
		eng = &softwareRenderer->engB;
	}
	int i;
	uint32_t charBase = DSRegisterDISPCNTGetCharBase(softwareRenderer->dispcntA) << 16;
	uint32_t screenBase = DSRegisterDISPCNTGetScreenBase(softwareRenderer->dispcntA) << 16;
	for (i = 0; i < 4; ++i) {
		if (!engB) {
			uint32_t control = eng->bg[i].control;
			eng->d.writeVideoRegister(&eng->d, DS9_REG_A_BG0CNT + i * 2, control);
			eng->bg[i].control = control;
		}

		eng->bg[i].charBase = GBARegisterBGCNTGetCharBase(eng->bg[i].control) << 14;

		if (!engB) {
			softwareRenderer->engA.bg[i].charBase += charBase;
			softwareRenderer->engA.bg[i].screenBase &= ~0x70000;
			softwareRenderer->engA.bg[i].screenBase |= screenBase;
		}
	}
}

void DSVideoSoftwareRendererCreate(struct DSVideoSoftwareRenderer* renderer) {
	renderer->d.init = DSVideoSoftwareRendererInit;
	renderer->d.reset = DSVideoSoftwareRendererReset;
	renderer->d.deinit = DSVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = DSVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writePalette = DSVideoSoftwareRendererWritePalette;
	renderer->d.writeOAM = DSVideoSoftwareRendererWriteOAM;
	renderer->d.invalidateExtPal = DSVideoSoftwareRendererInvalidateExtPal;
	renderer->d.drawScanline = DSVideoSoftwareRendererDrawScanline;
	renderer->d.drawScanlineDirectly = DSVideoSoftwareRendererDrawScanlineDirectly;
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
	softwareRenderer->engA.masterHeight = DS_VIDEO_VERTICAL_PIXELS;
	softwareRenderer->engA.masterScanlines = DS_VIDEO_VERTICAL_TOTAL_PIXELS;
	softwareRenderer->engA.outputBufferStride = softwareRenderer->outputBufferStride;
	softwareRenderer->engB.d.palette = &renderer->palette[512];
	softwareRenderer->engB.d.oam = &renderer->oam->oam[1];
	softwareRenderer->engB.masterEnd = DS_VIDEO_HORIZONTAL_PIXELS;
	softwareRenderer->engB.masterHeight = DS_VIDEO_VERTICAL_PIXELS;
	softwareRenderer->engB.masterScanlines = DS_VIDEO_VERTICAL_TOTAL_PIXELS;
	softwareRenderer->engB.outputBufferStride = softwareRenderer->outputBufferStride;

	DSVideoSoftwareRendererReset(renderer);
}

static void DSVideoSoftwareRendererReset(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.reset(&softwareRenderer->engA.d);
	softwareRenderer->engB.d.reset(&softwareRenderer->engB.d);
	softwareRenderer->powcnt = 0;
	softwareRenderer->dispcntA = 0;
	softwareRenderer->dispcntB = 0;
}

static void DSVideoSoftwareRendererDeinit(struct DSVideoRenderer* renderer) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	softwareRenderer->engA.d.deinit(&softwareRenderer->engA.d);
	softwareRenderer->engB.d.deinit(&softwareRenderer->engB.d);
}

static void DSVideoSoftwareRendererUpdateDISPCNT(struct DSVideoSoftwareRenderer* softwareRenderer, bool engB) {
	uint32_t dispcnt;
	struct GBAVideoSoftwareRenderer* eng;
	if (!engB) {
		dispcnt = softwareRenderer->dispcntA;
		eng = &softwareRenderer->engA;
	} else {
		dispcnt = softwareRenderer->dispcntB;
		eng = &softwareRenderer->engB;
	}
	uint16_t fakeDispcnt = dispcnt & 0xFF87;
	if (!DSRegisterDISPCNTIsTileObjMapping(dispcnt)) {
		eng->tileStride = 0x20;
	} else {
		eng->tileStride = 0x20 << DSRegisterDISPCNTGetTileBoundary(dispcnt);
		fakeDispcnt = GBARegisterDISPCNTFillObjCharacterMapping(fakeDispcnt);
	}
	eng->d.writeVideoRegister(&eng->d, DS9_REG_A_DISPCNT_LO, fakeDispcnt);
	eng->dispcnt |= dispcnt & 0xFFFF0000;
	if (DSRegisterDISPCNTIsBgExtPalette(dispcnt)) {
		color_t* extPalette;
		if (!engB) {
			extPalette = softwareRenderer->extPaletteA;
		} else {
			extPalette = softwareRenderer->extPaletteB;
		}
		int i;
		for (i = 0; i < 4; ++i) {
			int slot = i;
			if (i < 2 && GBARegisterBGCNTIsExtPaletteSlot(eng->bg[i].control)) {
				slot += 2;
			}
			if (eng->bg[i].extPalette != &extPalette[slot * 4096]) {
				_regenerateExtPalette(softwareRenderer, false, engB, slot);
			}
		}
	} else {
		eng->bg[0].extPalette = NULL;
		eng->bg[1].extPalette = NULL;
		eng->bg[2].extPalette = NULL;
		eng->bg[3].extPalette = NULL;
	}
	if (DSRegisterDISPCNTIsObjExtPalette(dispcnt)) {
		if (!engB) {
			if (softwareRenderer->engA.objExtPalette != softwareRenderer->objExtPaletteA) {
				_regenerateExtPalette(softwareRenderer, true, engB, 0);
			}
		} else {
			if (softwareRenderer->engB.objExtPalette != softwareRenderer->objExtPaletteB) {
				_regenerateExtPalette(softwareRenderer, true, engB, 0);
			}
		}
	} else {
		if (!engB) {
			softwareRenderer->engA.objExtPalette = NULL;
		} else {
			softwareRenderer->engB.objExtPalette = NULL;
		}
	}
	if (!engB) {
		eng->dispcnt = DSRegisterDISPCNTClear3D(eng->dispcnt);
		eng->dispcnt |= DSRegisterDISPCNTIs3D(dispcnt);
		_updateCharBase(softwareRenderer, engB);
	}
}

static uint16_t DSVideoSoftwareRendererWriteVideoRegister(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	if (address >= DS9_REG_A_BG0CNT && address <= DS9_REG_A_BLDY) {
		softwareRenderer->engA.d.writeVideoRegister(&softwareRenderer->engA.d, address, value);
	} else if (address >= DS9_REG_B_BG0CNT && address <= DS9_REG_B_BLDY) {
		softwareRenderer->engB.d.writeVideoRegister(&softwareRenderer->engB.d, address & 0xFF, value);
	} else {
		mLOG(DS_VIDEO, STUB, "Stub video register write: %04X:%04X", address, value);
	}
	switch (address) {
	case DS9_REG_A_BG0CNT:
	case DS9_REG_A_BG1CNT:
		softwareRenderer->engA.bg[(address - DS9_REG_A_BG0CNT) >> 1].control = value;
		// Fall through
	case DS9_REG_A_BG2CNT:
	case DS9_REG_A_BG3CNT:
		_updateCharBase(softwareRenderer, false);
		break;
	case DS9_REG_B_BG0CNT:
	case DS9_REG_B_BG1CNT:
		softwareRenderer->engB.bg[(address - DS9_REG_B_BG0CNT) >> 1].control = value;
		// Fall through
	case DS9_REG_B_BG2CNT:
	case DS9_REG_B_BG3CNT:
		_updateCharBase(softwareRenderer, true);
		break;
	case DS9_REG_A_MASTER_BRIGHT:
		softwareRenderer->engA.masterBright = DSRegisterMASTER_BRIGHTGetMode(value);
		softwareRenderer->engA.masterBrightY = DSRegisterMASTER_BRIGHTGetY(value);
		if (softwareRenderer->engA.masterBrightY > 0x10) {
			softwareRenderer->engA.masterBrightY = 0x10;
		}
		break;
	case DS9_REG_B_MASTER_BRIGHT:
		softwareRenderer->engB.masterBright = DSRegisterMASTER_BRIGHTGetMode(value);
		softwareRenderer->engB.masterBrightY = DSRegisterMASTER_BRIGHTGetY(value);
		if (softwareRenderer->engB.masterBrightY > 0x10) {
			softwareRenderer->engB.masterBrightY = 0x10;
		}
		break;
	case DS9_REG_A_BLDCNT:
	case DS9_REG_A_BLDY:
		// TODO: Optimize
		_regenerateExtPalette(softwareRenderer, false, false, 0);
		_regenerateExtPalette(softwareRenderer, false, false, 1);
		_regenerateExtPalette(softwareRenderer, false, false, 2);
		_regenerateExtPalette(softwareRenderer, false, false, 3);
		_regenerateExtPalette(softwareRenderer, true, false, 0);
		break;
	case DS9_REG_B_BLDCNT:
	case DS9_REG_B_BLDY:
		// TODO: Optimize
		_regenerateExtPalette(softwareRenderer, false, true, 0);
		_regenerateExtPalette(softwareRenderer, false, true, 1);
		_regenerateExtPalette(softwareRenderer, false, true, 2);
		_regenerateExtPalette(softwareRenderer, false, true, 3);
		_regenerateExtPalette(softwareRenderer, true, true, 0);
		break;
	case DS9_REG_A_DISPCNT_LO:
		softwareRenderer->dispcntA &= 0xFFFF0000;
		softwareRenderer->dispcntA |= value;
		DSVideoSoftwareRendererUpdateDISPCNT(softwareRenderer, false);
		break;
	case DS9_REG_A_DISPCNT_HI:
		softwareRenderer->dispcntA &= 0x0000FFFF;
		softwareRenderer->dispcntA |= value << 16;
		DSVideoSoftwareRendererUpdateDISPCNT(softwareRenderer, false);
		break;
	case DS9_REG_B_DISPCNT_LO:
		softwareRenderer->dispcntB &= 0xFFFF0000;
		softwareRenderer->dispcntB |= value;
		DSVideoSoftwareRendererUpdateDISPCNT(softwareRenderer, true);
		break;
	case DS9_REG_B_DISPCNT_HI:
		softwareRenderer->dispcntB &= 0x0000FFFF;
		softwareRenderer->dispcntB |= value << 16;
		DSVideoSoftwareRendererUpdateDISPCNT(softwareRenderer, true);
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

static void DSVideoSoftwareRendererInvalidateExtPal(struct DSVideoRenderer* renderer, bool obj, bool engB, int slot) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	_regenerateExtPalette(softwareRenderer, obj, engB, slot);
}

static void DSVideoSoftwareRendererDrawGBAScanline(struct GBAVideoRenderer* renderer, struct DSGX* gx, int y) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	int x;
	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
	if (GBARegisterDISPCNTIsForcedBlank(softwareRenderer->dispcnt)) {
		for (x = 0; x < softwareRenderer->masterEnd; ++x) {
			row[x] = GBA_COLOR_WHITE;
		}
		return;
	}

	GBAVideoSoftwareRendererPreprocessBuffer(softwareRenderer, y);
	int spriteLayers = GBAVideoSoftwareRendererPreprocessSpriteLayer(softwareRenderer, y);
	memset(softwareRenderer->alphaA, softwareRenderer->blda, sizeof(softwareRenderer->alphaA));
	memset(softwareRenderer->alphaB, softwareRenderer->bldb, sizeof(softwareRenderer->alphaB));

	int w;
	unsigned priority;
	for (priority = 0; priority < 4; ++priority) {
		softwareRenderer->end = 0;
		for (w = 0; w < softwareRenderer->nWindows; ++w) {
			softwareRenderer->start = softwareRenderer->end;
			softwareRenderer->end = softwareRenderer->windows[w].endX;
			softwareRenderer->currentWindow = softwareRenderer->windows[w].control;
			if (spriteLayers & (1 << priority)) {
				GBAVideoSoftwareRendererPostprocessSprite(softwareRenderer, priority);
			}
			if (TEST_LAYER_ENABLED(0)) {
				if (DSRegisterDISPCNTIs3D(softwareRenderer->dispcnt) && gx) {
					const color_t* scanline;
					gx->renderer->getScanline(gx->renderer, y, &scanline);
					uint32_t flags = (softwareRenderer->bg[0].priority << OFFSET_PRIORITY) | FLAG_IS_BACKGROUND;
					flags |= FLAG_TARGET_2 * softwareRenderer->bg[0].target2;
					flags |= FLAG_TARGET_1 * (softwareRenderer->bg[0].target1 && softwareRenderer->blendEffect == BLEND_ALPHA && GBAWindowControlIsBlendEnable(softwareRenderer->currentWindow.packed));
					int x;
					for (x = softwareRenderer->start; x < softwareRenderer->end; ++x) {
						if (scanline[x] & 0xF8000000) {
							if ((flags & FLAG_TARGET_1) && (scanline[x] >> 28) != 0xF) {
								// TODO: More precise values
								softwareRenderer->alphaA[x] = (scanline[x] >> 28) + 1;
								softwareRenderer->alphaB[x] = 0x10;
								_compositeBlendNoObjwin(softwareRenderer, x, (scanline[x] & 0x00FFFFFF) | flags, softwareRenderer->row[x]);
							} else {
								_compositeNoBlendNoObjwin(softwareRenderer, x, (scanline[x] & 0x00FFFFFF) | flags, softwareRenderer->row[x]);
								softwareRenderer->alphaA[x] = 0x10;
								softwareRenderer->alphaB[x] = 0;
							}
						}
					}
				} else {
					GBAVideoSoftwareRendererDrawBackgroundMode0(softwareRenderer, &softwareRenderer->bg[0], y);
				}
			}
			if (TEST_LAYER_ENABLED(1)) {
				GBAVideoSoftwareRendererDrawBackgroundMode0(softwareRenderer, &softwareRenderer->bg[1], y);
			}
			if (TEST_LAYER_ENABLED(2)) {
				switch (GBARegisterDISPCNTGetMode(softwareRenderer->dispcnt)) {
				case 0:
				case 1:
				case 3:
					GBAVideoSoftwareRendererDrawBackgroundMode0(softwareRenderer, &softwareRenderer->bg[2], y);
					break;
				case 2:
				case 4:
					GBAVideoSoftwareRendererDrawBackgroundMode2(softwareRenderer, &softwareRenderer->bg[2], y);
					break;
				case 5:
					if (!GBARegisterBGCNTIsExtendedMode1(softwareRenderer->bg[2].control)) {
						DSVideoSoftwareRendererDrawBackgroundExt0(softwareRenderer, &softwareRenderer->bg[2], y);
					} else if (!GBARegisterBGCNTIsExtendedMode0(softwareRenderer->bg[2].control)) {
						DSVideoSoftwareRendererDrawBackgroundExt1(softwareRenderer, &softwareRenderer->bg[2], y);
					} else {
						DSVideoSoftwareRendererDrawBackgroundExt2(softwareRenderer, &softwareRenderer->bg[2], y);
					}
					break;
				}
			}
			if (TEST_LAYER_ENABLED(3)) {
				switch (GBARegisterDISPCNTGetMode(softwareRenderer->dispcnt)) {
				case 0:
					GBAVideoSoftwareRendererDrawBackgroundMode0(softwareRenderer, &softwareRenderer->bg[3], y);
					break;
				case 1:
				case 2:
					GBAVideoSoftwareRendererDrawBackgroundMode2(softwareRenderer, &softwareRenderer->bg[3], y);
					break;
				case 3:
				case 4:
				case 5:
					if (!GBARegisterBGCNTIsExtendedMode1(softwareRenderer->bg[3].control)) {
						DSVideoSoftwareRendererDrawBackgroundExt0(softwareRenderer, &softwareRenderer->bg[3], y);
					} else if (!GBARegisterBGCNTIsExtendedMode0(softwareRenderer->bg[3].control)) {
						DSVideoSoftwareRendererDrawBackgroundExt1(softwareRenderer, &softwareRenderer->bg[3], y);
					} else {
						DSVideoSoftwareRendererDrawBackgroundExt2(softwareRenderer, &softwareRenderer->bg[3], y);
					}
					break;
				}
			}
		}
	}
	softwareRenderer->bg[2].sx += softwareRenderer->bg[2].dmx;
	softwareRenderer->bg[2].sy += softwareRenderer->bg[2].dmy;
	softwareRenderer->bg[3].sx += softwareRenderer->bg[3].dmx;
	softwareRenderer->bg[3].sy += softwareRenderer->bg[3].dmy;

	GBAVideoSoftwareRendererPostprocessBuffer(softwareRenderer);
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
		DSVideoSoftwareRendererDrawGBAScanline(&softwareRenderer->engA.d, softwareRenderer->d.gx, y);
		break;
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
			softwareRenderer->engA.row[x] = color;
		}
		break;
	}
	case 3:
		break;
	}

#ifdef COLOR_16_BIT
#if defined(__ARM_NEON) && !defined(__APPLE__)
	_to16Bit(row, softwareRenderer->engA.row, DS_VIDEO_HORIZONTAL_PIXELS);
#else
	for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
		row[x] = softwareRenderer->engA.row[x];
	}
#endif
#else
	switch (softwareRenderer->engA.masterBright) {
	case 0:
	default:
		memcpy(row, softwareRenderer->engA.row, softwareRenderer->engA.masterEnd * sizeof(*row));
		break;
	case 1:
		for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = _brighten(softwareRenderer->engA.row[x], softwareRenderer->engA.masterBrightY);
		}
		break;
	case 2:
		for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = _darken(softwareRenderer->engA.row[x], softwareRenderer->engA.masterBrightY);
		}
		break;
	}
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
		DSVideoSoftwareRendererDrawGBAScanline(&softwareRenderer->engB.d, NULL, y);
		break;
	}

#ifdef COLOR_16_BIT
#if defined(__ARM_NEON) && !defined(__APPLE__)
	_to16Bit(row, softwareRenderer->engB.row, DS_VIDEO_HORIZONTAL_PIXELS);
#else
	for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
		row[x] = softwareRenderer->engB.row[x];
	}
#endif
#else
	switch (softwareRenderer->engB.masterBright) {
	case 0:
	default:
		memcpy(row, softwareRenderer->engB.row, softwareRenderer->engB.masterEnd * sizeof(*row));
		break;
	case 1:
		for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = _brighten(softwareRenderer->engB.row[x], softwareRenderer->engB.masterBrightY);
		}
		break;
	case 2:
		for (x = 0; x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = _darken(softwareRenderer->engB.row[x], softwareRenderer->engB.masterBrightY);
		}
		break;
	}
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

static void DSVideoSoftwareRendererDrawScanlineDirectly(struct DSVideoRenderer* renderer, int y, color_t* scanline) {
	struct DSVideoSoftwareRenderer* softwareRenderer = (struct DSVideoSoftwareRenderer*) renderer;
	DSVideoSoftwareRendererDrawGBAScanline(&softwareRenderer->engA.d, softwareRenderer->d.gx, y);
	memcpy(scanline, softwareRenderer->engA.row, softwareRenderer->engA.masterEnd * sizeof(*scanline));
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

#define EXT_0_COORD_OVERFLOW \
	localX = x & (sizeAdjusted - 1); \
	localY = y & (sizeAdjusted - 1); \

#define EXT_0_COORD_NO_OVERFLOW \
	if ((x | y) & ~(sizeAdjusted - 1)) { \
		continue; \
	} \
	localX = x; \
	localY = y;

#define EXT_0_NO_MOSAIC(COORD) \
	COORD \
	uint32_t screenBase = background->screenBase + (localX >> 10) + (((localY >> 6) & 0xFE0) << background->size); \
	uint16_t* screenBlock = renderer->d.vramBG[screenBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!screenBlock)) { \
		continue; \
	} \
	LOAD_16(mapData, screenBase & (VRAM_BLOCK_MASK - 1), screenBlock); \
	paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 8; \
	palette = &mainPalette[paletteData]; \
	uint32_t charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + ((localY & 0x700) >> 5) + ((localX & 0x700) >> 8); \
	uint16_t* vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vram)) { \
		continue; \
	} \
	pixelData = ((uint8_t*) vram)[charBase & VRAM_BLOCK_MASK];

#define EXT_0_MOSAIC(COORD) \
		if (!mosaicWait) { \
			EXT_0_NO_MOSAIC(COORD) \
			mosaicWait = mosaicH; \
		} else { \
			--mosaicWait; \
		}

#define EXT_0_LOOP(MOSAIC, COORD, BLEND, OBJWIN) \
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) { \
		x += background->dx; \
		y += background->dy; \
		\
		uint32_t current = *pixel; \
		MOSAIC(COORD) \
		if (pixelData) { \
			COMPOSITE_256_ ## OBJWIN (BLEND, 0); \
		} \
	}

#define DRAW_BACKGROUND_EXT_0(BLEND, OBJWIN) \
	if (background->overflow) { \
		if (mosaicH > 1) { \
			EXT_0_LOOP(EXT_0_MOSAIC, EXT_0_COORD_OVERFLOW, BLEND, OBJWIN); \
		} else { \
			EXT_0_LOOP(EXT_0_NO_MOSAIC, EXT_0_COORD_OVERFLOW, BLEND, OBJWIN); \
		} \
	} else { \
		if (mosaicH > 1) { \
			EXT_0_LOOP(EXT_0_MOSAIC, EXT_0_COORD_NO_OVERFLOW, BLEND, OBJWIN); \
		} else { \
			EXT_0_LOOP(EXT_0_NO_MOSAIC, EXT_0_COORD_NO_OVERFLOW, BLEND, OBJWIN); \
		} \
	}

void DSVideoSoftwareRendererDrawBackgroundExt0(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY) {
	int sizeAdjusted = 0x8000 << background->size;

	BACKGROUND_BITMAP_INIT;

	color_t* mainPalette = background->extPalette;
	if (variant) {
		mainPalette = background->variantPalette;
	}
	if (!mainPalette) {
		return;
	}
	int paletteData;

	uint16_t mapData;
	uint8_t pixelData = 0;

	int outX;
	uint32_t* pixel;

	if (!objwinSlowPath) {
		if (!(flags & FLAG_TARGET_2)) {
			DRAW_BACKGROUND_EXT_0(NoBlend, NO_OBJWIN);
		} else {
			DRAW_BACKGROUND_EXT_0(Blend, NO_OBJWIN);
		}
	} else {
		if (!(flags & FLAG_TARGET_2)) {
			DRAW_BACKGROUND_EXT_0(NoBlend, OBJWIN);
		} else {
			DRAW_BACKGROUND_EXT_0(Blend, OBJWIN);
		}
	}
}

void DSVideoSoftwareRendererDrawBackgroundExt1(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY) {
	BACKGROUND_BITMAP_INIT;

	uint8_t color;
	int width, height;
	switch (background->size) {
	case 0:
		width = 128;
		height = 128;
		break;
	case 1:
		width = 256;
		height = 256;
		break;
	case 2:
		width = 512;
		height = 256;
		break;
	case 3:
		width = 512;
		height = 512;
		break;
	}

	int outX;
	for (outX = renderer->start; outX < renderer->end; ++outX) {
		BACKGROUND_BITMAP_ITERATE(width, height);

		if (!mosaicWait) {
			uint32_t address = (localX >> 8) + (localY >> 8) * width;
			color = ((uint8_t*)renderer->d.vramBG[address >> 17])[address];
			mosaicWait = mosaicH;
		} else {
			--mosaicWait;
		}

		uint32_t current = renderer->row[outX];
		if (color && IS_WRITABLE(current)) {
			if (!objwinSlowPath) {
				_compositeBlendNoObjwin(renderer, outX, palette[color] | flags, current);
			} else if (objwinForceEnable || (!(current & FLAG_OBJWIN)) == objwinOnly) {
				color_t* currentPalette = (current & FLAG_OBJWIN) ? objwinPalette : palette;
				unsigned mergedFlags = flags;
				if (current & FLAG_OBJWIN) {
					mergedFlags = objwinFlags;
				}
				_compositeBlendObjwin(renderer, outX, currentPalette[color] | mergedFlags, current);
			}
		}
	}
}

void DSVideoSoftwareRendererDrawBackgroundExt2(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY) {
	BACKGROUND_BITMAP_INIT;

	uint32_t color;
	int width, height;
	switch (background->size) {
	case 0:
		width = 128;
		height = 128;
		break;
	case 1:
		width = 256;
		height = 256;
		break;
	case 2:
		width = 512;
		height = 256;
		break;
	case 3:
		width = 512;
		height = 512;
		break;
	}

	int outX;
	for (outX = renderer->start; outX < renderer->end; ++outX) {
		BACKGROUND_BITMAP_ITERATE(width, height);

		if (!mosaicWait) {
			uint32_t address = ((localX >> 8) + (localY >> 8) * width) << 1;
			LOAD_16(color, address & 0x1FFFE, renderer->d.vramBG[address >> 17]);
#ifndef COLOR_16_BIT
			unsigned color32;
			color32 = 0;
			color32 |= (color << 3) & 0xF8;
			color32 |= (color << 6) & 0xF800;
			color32 |= (color << 9) & 0xF80000;
			color32 |= (color32 >> 5) & 0x070707;
			color = color32;
#elif COLOR_5_6_5
			uint16_t color16 = 0;
			color16 |= (color & 0x001F) << 11;
			color16 |= (color & 0x03E0) << 1;
			color16 |= (color & 0x7C00) >> 10;
			color = color16;
#endif
			mosaicWait = mosaicH;
		} else {
			--mosaicWait;
		}

		uint32_t current = renderer->row[outX];
		if (!objwinSlowPath || (!(current & FLAG_OBJWIN)) != objwinOnly) {
			unsigned mergedFlags = flags;
			if (current & FLAG_OBJWIN) {
				mergedFlags = objwinFlags;
			}
			if (!variant) {
				_compositeBlendObjwin(renderer, outX, color | mergedFlags, current);
			} else if (renderer->blendEffect == BLEND_BRIGHTEN) {
				_compositeBlendObjwin(renderer, outX, _brighten(color, renderer->bldy) | mergedFlags, current);
			} else if (renderer->blendEffect == BLEND_DARKEN) {
				_compositeBlendObjwin(renderer, outX, _darken(color, renderer->bldy) | mergedFlags, current);
			}
		}
	}
}
