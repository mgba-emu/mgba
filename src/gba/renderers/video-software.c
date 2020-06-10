/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/renderers/software-private.h"

#include <mgba/core/cache-set.h>
#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/cache-set.h>

#include <mgba-util/arm-algo.h>
#include <mgba-util/memory.h>

#define DIRTY_SCANLINE(R, Y) R->scanlineDirty[Y >> 5] |= (1 << (Y & 0x1F))
#define CLEAN_SCANLINE(R, Y) R->scanlineDirty[Y >> 5] &= ~(1 << (Y & 0x1F))

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererReset(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address);
static void GBAVideoSoftwareRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoSoftwareRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBAVideoSoftwareRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels);

static void GBAVideoSoftwareRendererUpdateDISPCNT(struct GBAVideoSoftwareRenderer* renderer);
static void GBAVideoSoftwareRendererWriteBGCNT(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGX_LO(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGX_HI(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGY_LO(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGY_HI(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBLDCNT(struct GBAVideoSoftwareRenderer* renderer, uint16_t value);

static void _drawScanline(struct GBAVideoSoftwareRenderer* renderer, int y);

static void _updatePalettes(struct GBAVideoSoftwareRenderer* renderer);

static void _breakWindow(struct GBAVideoSoftwareRenderer* softwareRenderer, struct WindowN* win, int y);
static void _breakWindowInner(struct GBAVideoSoftwareRenderer* softwareRenderer, struct WindowN* win);

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBAVideoSoftwareRendererInit;
	renderer->d.reset = GBAVideoSoftwareRendererReset;
	renderer->d.deinit = GBAVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBAVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writeVRAM = GBAVideoSoftwareRendererWriteVRAM;
	renderer->d.writeOAM = GBAVideoSoftwareRendererWriteOAM;
	renderer->d.writePalette = GBAVideoSoftwareRendererWritePalette;
	renderer->d.drawScanline = GBAVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = GBAVideoSoftwareRendererFinishFrame;
	renderer->d.getPixels = GBAVideoSoftwareRendererGetPixels;
	renderer->d.putPixels = GBAVideoSoftwareRendererPutPixels;

	renderer->d.disableBG[0] = false;
	renderer->d.disableBG[1] = false;
	renderer->d.disableBG[2] = false;
	renderer->d.disableBG[3] = false;
	renderer->d.disableOBJ = false;

	renderer->d.highlightBG[0] = false;
	renderer->d.highlightBG[1] = false;
	renderer->d.highlightBG[2] = false;
	renderer->d.highlightBG[3] = false;
	int i;
	for (i = 0; i < 128; ++i) {
		renderer->d.highlightOBJ[i] = false;
	}
	renderer->d.highlightColor = GBA_COLOR_WHITE;
	renderer->d.highlightAmount = 0;

	renderer->temporaryBuffer = 0;
}

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer) {
	GBAVideoSoftwareRendererReset(renderer);

	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	int y;
	for (y = 0; y < GBA_VIDEO_VERTICAL_PIXELS; ++y) {
		color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
		int x;
		for (x = 0; x < GBA_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GBA_COLOR_WHITE;
		}
	}
}

static void GBAVideoSoftwareRendererReset(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	int i;

	softwareRenderer->dispcnt = 0x0080;

	softwareRenderer->target1Obj = 0;
	softwareRenderer->target1Bd = 0;
	softwareRenderer->target2Obj = 0;
	softwareRenderer->target2Bd = 0;
	softwareRenderer->blendEffect = BLEND_NONE;
	for (i = 0; i < 1024; i += 2) {
		uint16_t entry;
		LOAD_16(entry, i, softwareRenderer->d.palette);
		GBAVideoSoftwareRendererWritePalette(renderer, i, entry);
	}
	softwareRenderer->blendDirty = false;
	_updatePalettes(softwareRenderer);

	softwareRenderer->blda = 0;
	softwareRenderer->bldb = 0;
	softwareRenderer->bldy = 0;

	softwareRenderer->winN[0] = (struct WindowN) { .control = { .priority = 0 } };
	softwareRenderer->winN[1] = (struct WindowN) { .control = { .priority = 1 } };
	softwareRenderer->objwin = (struct WindowControl) { .priority = 2 };
	softwareRenderer->winout = (struct WindowControl) { .priority = 3 };
	softwareRenderer->oamDirty = 1;
	softwareRenderer->oamMax = 0;

	softwareRenderer->mosaic = 0;
	softwareRenderer->nextY = 0;

	softwareRenderer->objOffsetX = 0;
	softwareRenderer->objOffsetY = 0;

	memset(softwareRenderer->scanlineDirty, 0xFFFFFFFF, sizeof(softwareRenderer->scanlineDirty));
	memset(softwareRenderer->cache, 0, sizeof(softwareRenderer->cache));
	memset(softwareRenderer->nextIo, 0, sizeof(softwareRenderer->nextIo));

	softwareRenderer->lastHighlightAmount = 0;

	for (i = 0; i < 4; ++i) {
		struct GBAVideoSoftwareBackground* bg = &softwareRenderer->bg[i];
		bg->index = i;
		bg->enabled = 0;
		bg->priority = 0;
		bg->charBase = 0;
		bg->mosaic = 0;
		bg->multipalette = 0;
		bg->screenBase = 0;
		bg->overflow = 0;
		bg->size = 0;
		bg->target1 = 0;
		bg->target2 = 0;
		bg->x = 0;
		bg->y = 0;
		bg->refx = 0;
		bg->refy = 0;
		bg->dx = 256;
		bg->dmx = 0;
		bg->dy = 0;
		bg->dmy = 256;
		bg->sx = 0;
		bg->sy = 0;
		bg->yCache = -1;
		bg->offsetX = 0;
		bg->offsetY = 0;
	}
}

static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	UNUSED(softwareRenderer);
}

static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	if (renderer->cache) {
		GBAVideoCacheWriteVideoRegister(renderer->cache, address, value);
	}

	switch (address) {
	case REG_DISPCNT:
		value &= 0xFFF7;
		softwareRenderer->dispcnt = value;
		GBAVideoSoftwareRendererUpdateDISPCNT(softwareRenderer);
		break;
	case REG_BG0CNT:
		value &= 0xDFFF;
		GBAVideoSoftwareRendererWriteBGCNT(softwareRenderer, &softwareRenderer->bg[0], value);
		break;
	case REG_BG1CNT:
		value &= 0xDFFF;
		GBAVideoSoftwareRendererWriteBGCNT(softwareRenderer, &softwareRenderer->bg[1], value);
		break;
	case REG_BG2CNT:
		value &= 0xFFFF;
		GBAVideoSoftwareRendererWriteBGCNT(softwareRenderer, &softwareRenderer->bg[2], value);
		break;
	case REG_BG3CNT:
		value &= 0xFFFF;
		GBAVideoSoftwareRendererWriteBGCNT(softwareRenderer, &softwareRenderer->bg[3], value);
		break;
	case REG_BG0HOFS:
		value &= 0x01FF;
		softwareRenderer->bg[0].x = value;
		break;
	case REG_BG0VOFS:
		value &= 0x01FF;
		softwareRenderer->bg[0].y = value;
		break;
	case REG_BG1HOFS:
		value &= 0x01FF;
		softwareRenderer->bg[1].x = value;
		break;
	case REG_BG1VOFS:
		value &= 0x01FF;
		softwareRenderer->bg[1].y = value;
		break;
	case REG_BG2HOFS:
		value &= 0x01FF;
		softwareRenderer->bg[2].x = value;
		break;
	case REG_BG2VOFS:
		value &= 0x01FF;
		softwareRenderer->bg[2].y = value;
		break;
	case REG_BG3HOFS:
		value &= 0x01FF;
		softwareRenderer->bg[3].x = value;
		break;
	case REG_BG3VOFS:
		value &= 0x01FF;
		softwareRenderer->bg[3].y = value;
		break;
	case REG_BG2PA:
		softwareRenderer->bg[2].dx = value;
		break;
	case REG_BG2PB:
		softwareRenderer->bg[2].dmx = value;
		break;
	case REG_BG2PC:
		softwareRenderer->bg[2].dy = value;
		break;
	case REG_BG2PD:
		softwareRenderer->bg[2].dmy = value;
		break;
	case REG_BG2X_LO:
		GBAVideoSoftwareRendererWriteBGX_LO(&softwareRenderer->bg[2], value);
		if (softwareRenderer->bg[2].sx != softwareRenderer->cache[softwareRenderer->nextY].scale[0][0]) {
			DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
		}
		break;
	case REG_BG2X_HI:
		GBAVideoSoftwareRendererWriteBGX_HI(&softwareRenderer->bg[2], value);
		if (softwareRenderer->bg[2].sx != softwareRenderer->cache[softwareRenderer->nextY].scale[0][0]) {
			DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
		}
		break;
	case REG_BG2Y_LO:
		GBAVideoSoftwareRendererWriteBGY_LO(&softwareRenderer->bg[2], value);
		if (softwareRenderer->bg[2].sy != softwareRenderer->cache[softwareRenderer->nextY].scale[0][1]) {
			DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
		}
		break;
	case REG_BG2Y_HI:
		GBAVideoSoftwareRendererWriteBGY_HI(&softwareRenderer->bg[2], value);
		if (softwareRenderer->bg[2].sy != softwareRenderer->cache[softwareRenderer->nextY].scale[0][1]) {
			DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
		}
		break;
	case REG_BG3PA:
		softwareRenderer->bg[3].dx = value;
		break;
	case REG_BG3PB:
		softwareRenderer->bg[3].dmx = value;
		break;
	case REG_BG3PC:
		softwareRenderer->bg[3].dy = value;
		break;
	case REG_BG3PD:
		softwareRenderer->bg[3].dmy = value;
		break;
	case REG_BG3X_LO:
		GBAVideoSoftwareRendererWriteBGX_LO(&softwareRenderer->bg[3], value);
		if (softwareRenderer->bg[3].sx != softwareRenderer->cache[softwareRenderer->nextY].scale[1][0]) {
			DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
		}
		break;
	case REG_BG3X_HI:
		GBAVideoSoftwareRendererWriteBGX_HI(&softwareRenderer->bg[3], value);
		if (softwareRenderer->bg[3].sx != softwareRenderer->cache[softwareRenderer->nextY].scale[1][0]) {
			DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
		}
		break;
	case REG_BG3Y_LO:
		GBAVideoSoftwareRendererWriteBGY_LO(&softwareRenderer->bg[3], value);
		if (softwareRenderer->bg[3].sy != softwareRenderer->cache[softwareRenderer->nextY].scale[1][1]) {
			DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
		}
		break;
	case REG_BG3Y_HI:
		GBAVideoSoftwareRendererWriteBGY_HI(&softwareRenderer->bg[3], value);
		if (softwareRenderer->bg[3].sy != softwareRenderer->cache[softwareRenderer->nextY].scale[1][1]) {
			DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
		}
		break;
	case REG_BLDCNT:
		GBAVideoSoftwareRendererWriteBLDCNT(softwareRenderer, value);
		value &= 0x3FFF;
		break;
	case REG_BLDALPHA:
		softwareRenderer->blda = value & 0x1F;
		if (softwareRenderer->blda > 0x10) {
			softwareRenderer->blda = 0x10;
		}
		softwareRenderer->bldb = (value >> 8) & 0x1F;
		if (softwareRenderer->bldb > 0x10) {
			softwareRenderer->bldb = 0x10;
		}
		value &= 0x1F1F;
		break;
	case REG_BLDY:
		value &= 0x1F;
		if (value > 0x10) {
			value = 0x10;
		}
		if (softwareRenderer->bldy != value) {
			softwareRenderer->bldy = value;
			softwareRenderer->blendDirty = true;
		}
		break;
	case REG_WIN0H:
		softwareRenderer->winN[0].h.end = value;
		softwareRenderer->winN[0].h.start = value >> 8;
		if (softwareRenderer->winN[0].h.start > GBA_VIDEO_HORIZONTAL_PIXELS && softwareRenderer->winN[0].h.start > softwareRenderer->winN[0].h.end) {
			softwareRenderer->winN[0].h.start = 0;
		}
		if (softwareRenderer->winN[0].h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[0].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
			if (softwareRenderer->winN[0].h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
				softwareRenderer->winN[0].h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
			}
		}
		break;
	case REG_WIN1H:
		softwareRenderer->winN[1].h.end = value;
		softwareRenderer->winN[1].h.start = value >> 8;
		if (softwareRenderer->winN[1].h.start > GBA_VIDEO_HORIZONTAL_PIXELS && softwareRenderer->winN[1].h.start > softwareRenderer->winN[1].h.end) {
			softwareRenderer->winN[1].h.start = 0;
		}
		if (softwareRenderer->winN[1].h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[1].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
			if (softwareRenderer->winN[1].h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
				softwareRenderer->winN[1].h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
			}
		}
		break;
	case REG_WIN0V:
		softwareRenderer->winN[0].v.end = value;
		softwareRenderer->winN[0].v.start = value >> 8;
		if (softwareRenderer->winN[0].v.start > GBA_VIDEO_VERTICAL_PIXELS && softwareRenderer->winN[0].v.start > softwareRenderer->winN[0].v.end) {
			softwareRenderer->winN[0].v.start = 0;
		}
		if (softwareRenderer->winN[0].v.end > GBA_VIDEO_VERTICAL_PIXELS) {
			softwareRenderer->winN[0].v.end = GBA_VIDEO_VERTICAL_PIXELS;
			if (softwareRenderer->winN[0].v.start > GBA_VIDEO_VERTICAL_PIXELS) {
				softwareRenderer->winN[0].v.start = GBA_VIDEO_VERTICAL_PIXELS;
			}
		}
		break;
	case REG_WIN1V:
		softwareRenderer->winN[1].v.end = value;
		softwareRenderer->winN[1].v.start = value >> 8;
		if (softwareRenderer->winN[1].v.start > GBA_VIDEO_VERTICAL_PIXELS && softwareRenderer->winN[1].v.start > softwareRenderer->winN[1].v.end) {
			softwareRenderer->winN[1].v.start = 0;
		}
		if (softwareRenderer->winN[1].v.end > GBA_VIDEO_VERTICAL_PIXELS) {
			softwareRenderer->winN[1].v.end = GBA_VIDEO_VERTICAL_PIXELS;
			if (softwareRenderer->winN[1].v.start > GBA_VIDEO_VERTICAL_PIXELS) {
				softwareRenderer->winN[1].v.start = GBA_VIDEO_VERTICAL_PIXELS;
			}
		}
		break;
	case REG_WININ:
		value &= 0x3F3F;
		softwareRenderer->winN[0].control.packed = value;
		softwareRenderer->winN[1].control.packed = value >> 8;
		break;
	case REG_WINOUT:
		value &= 0x3F3F;
		softwareRenderer->winout.packed = value;
		softwareRenderer->objwin.packed = value >> 8;
		break;
	case REG_MOSAIC:
		softwareRenderer->mosaic = value;
		break;
	case REG_GREENSWP:
		mLOG(GBA_VIDEO, STUB, "Stub video register write: 0x%03X", address);
		break;
	default:
		mLOG(GBA_VIDEO, GAME_ERROR, "Invalid video register: 0x%03X", address);
	}
	softwareRenderer->nextIo[address >> 1] = value;
	if (softwareRenderer->cache[softwareRenderer->nextY].io[address >> 1] != value) {
		softwareRenderer->cache[softwareRenderer->nextY].io[address >> 1] = value;
		DIRTY_SCANLINE(softwareRenderer, softwareRenderer->nextY);
	}
	return value;
}

static void GBAVideoSoftwareRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	if (renderer->cache) {
		mCacheSetWriteVRAM(renderer->cache, address);
	}
	memset(softwareRenderer->scanlineDirty, 0xFFFFFFFF, sizeof(softwareRenderer->scanlineDirty));
	softwareRenderer->bg[0].yCache = -1;
	softwareRenderer->bg[1].yCache = -1;
	softwareRenderer->bg[2].yCache = -1;
	softwareRenderer->bg[3].yCache = -1;
}

static void GBAVideoSoftwareRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	UNUSED(oam);
	softwareRenderer->oamDirty = 1;
	memset(softwareRenderer->scanlineDirty, 0xFFFFFFFF, sizeof(softwareRenderer->scanlineDirty));
}

static void GBAVideoSoftwareRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	color_t color = mColorFrom555(value);
	softwareRenderer->normalPalette[address >> 1] = color;
	if (softwareRenderer->blendEffect == BLEND_BRIGHTEN) {
		softwareRenderer->variantPalette[address >> 1] = _brighten(color, softwareRenderer->bldy);
	} else if (softwareRenderer->blendEffect == BLEND_DARKEN) {
		softwareRenderer->variantPalette[address >> 1] = _darken(color, softwareRenderer->bldy);
	}
	if (renderer->cache) {
		mCacheSetWritePalette(renderer->cache, address >> 1, color);
	}
	memset(softwareRenderer->scanlineDirty, 0xFFFFFFFF, sizeof(softwareRenderer->scanlineDirty));
}

static void _breakWindow(struct GBAVideoSoftwareRenderer* softwareRenderer, struct WindowN* win, int y) {
	if (win->v.end >= win->v.start) {
		if (y >= win->v.end) {
			return;
		}
		if (y < win->v.start) {
			return;
		}
	} else if (y >= win->v.end && y < win->v.start) {
		return;
	}
	if (win->h.end > GBA_VIDEO_HORIZONTAL_PIXELS || win->h.end < win->h.start) {
		struct WindowN splits[2] = { *win, *win };
		splits[0].h.start = 0;
		splits[1].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
		_breakWindowInner(softwareRenderer, &splits[0]);
		_breakWindowInner(softwareRenderer, &splits[1]);
	} else {
		_breakWindowInner(softwareRenderer, win);
	}
}

static void _breakWindowInner(struct GBAVideoSoftwareRenderer* softwareRenderer, struct WindowN* win) {
	int activeWindow;
	int startX = 0;
	if (win->h.end > 0) {
		for (activeWindow = 0; activeWindow < softwareRenderer->nWindows; ++activeWindow) {
			if (win->h.start < softwareRenderer->windows[activeWindow].endX) {
				// Insert a window before the end of the active window
				struct Window oldWindow = softwareRenderer->windows[activeWindow];
				if (win->h.start > startX) {
					// And after the start of the active window
					int nextWindow = softwareRenderer->nWindows;
					++softwareRenderer->nWindows;
					for (; nextWindow > activeWindow; --nextWindow) {
						softwareRenderer->windows[nextWindow] = softwareRenderer->windows[nextWindow - 1];
					}
					softwareRenderer->windows[activeWindow].endX = win->h.start;
					++activeWindow;
				}
				softwareRenderer->windows[activeWindow].control = win->control;
				softwareRenderer->windows[activeWindow].endX = win->h.end;
				if (win->h.end >= oldWindow.endX) {
					// Trim off extra windows we've overwritten
					for (++activeWindow; softwareRenderer->nWindows > activeWindow + 1 && win->h.end >= softwareRenderer->windows[activeWindow].endX; ++activeWindow) {
						if (VIDEO_CHECKS && activeWindow >= MAX_WINDOW) {
							mLOG(GBA_VIDEO, FATAL, "Out of bounds window write will occur");
							return;
						}
						softwareRenderer->windows[activeWindow] = softwareRenderer->windows[activeWindow + 1];
						--softwareRenderer->nWindows;
					}
				} else {
					++activeWindow;
					int nextWindow = softwareRenderer->nWindows;
					++softwareRenderer->nWindows;
					for (; nextWindow > activeWindow; --nextWindow) {
						softwareRenderer->windows[nextWindow] = softwareRenderer->windows[nextWindow - 1];
					}
					softwareRenderer->windows[activeWindow] = oldWindow;
				}
				break;
			}
			startX = softwareRenderer->windows[activeWindow].endX;
		}
	}
#ifdef DEBUG
	if (softwareRenderer->nWindows > MAX_WINDOW) {
		mLOG(GBA_VIDEO, FATAL, "Out of bounds window write occurred!");
	}
#endif
}

static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	if (y == GBA_VIDEO_VERTICAL_PIXELS - 1) {
		softwareRenderer->nextY = 0;
	} else {
		softwareRenderer->nextY = y + 1;
	}

	bool dirty = softwareRenderer->scanlineDirty[y >> 5] & (1 << (y & 0x1F));
	if (memcmp(softwareRenderer->nextIo, softwareRenderer->cache[y].io, sizeof(softwareRenderer->nextIo))) {
		memcpy(softwareRenderer->cache[y].io, softwareRenderer->nextIo, sizeof(softwareRenderer->nextIo));
		dirty = true;
	}

	if (GBARegisterDISPCNTGetMode(softwareRenderer->dispcnt) != 0) {
		if (softwareRenderer->cache[y].scale[0][0] != softwareRenderer->bg[2].sx ||
		    softwareRenderer->cache[y].scale[0][1] != softwareRenderer->bg[2].sy ||
		    softwareRenderer->cache[y].scale[1][0] != softwareRenderer->bg[3].sx ||
		    softwareRenderer->cache[y].scale[1][1] != softwareRenderer->bg[3].sy) {
			dirty = true;
		}
	}
	softwareRenderer->cache[y].scale[0][0] = softwareRenderer->bg[2].sx;
	softwareRenderer->cache[y].scale[0][1] = softwareRenderer->bg[2].sy;
	softwareRenderer->cache[y].scale[1][0] = softwareRenderer->bg[3].sx;
	softwareRenderer->cache[y].scale[1][1] = softwareRenderer->bg[3].sy;

	if (!dirty) {
		if (GBARegisterDISPCNTGetMode(softwareRenderer->dispcnt) != 0) {
			softwareRenderer->bg[2].sx += softwareRenderer->bg[2].dmx;
			softwareRenderer->bg[2].sy += softwareRenderer->bg[2].dmy;
			softwareRenderer->bg[3].sx += softwareRenderer->bg[3].dmx;
			softwareRenderer->bg[3].sy += softwareRenderer->bg[3].dmy;
		}
		return;
	}

	CLEAN_SCANLINE(softwareRenderer, y);

	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
	if (GBARegisterDISPCNTIsForcedBlank(softwareRenderer->dispcnt)) {
		int x;
		for (x = 0; x < GBA_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GBA_COLOR_WHITE;
		}
		return;
	}

	int x;
	for (x = 0; x < GBA_VIDEO_HORIZONTAL_PIXELS; x += 4) {
		softwareRenderer->spriteLayer[x] = FLAG_UNWRITTEN;
		softwareRenderer->spriteLayer[x + 1] = FLAG_UNWRITTEN;
		softwareRenderer->spriteLayer[x + 2] = FLAG_UNWRITTEN;
		softwareRenderer->spriteLayer[x + 3] = FLAG_UNWRITTEN;
	}

	softwareRenderer->windows[0].endX = GBA_VIDEO_HORIZONTAL_PIXELS;
	softwareRenderer->nWindows = 1;
	if (GBARegisterDISPCNTIsWin0Enable(softwareRenderer->dispcnt) || GBARegisterDISPCNTIsWin1Enable(softwareRenderer->dispcnt) || GBARegisterDISPCNTIsObjwinEnable(softwareRenderer->dispcnt)) {
		softwareRenderer->windows[0].control = softwareRenderer->winout;
		if (GBARegisterDISPCNTIsWin1Enable(softwareRenderer->dispcnt)) {
			_breakWindow(softwareRenderer, &softwareRenderer->winN[1], y);
		}
		if (GBARegisterDISPCNTIsWin0Enable(softwareRenderer->dispcnt)) {
			_breakWindow(softwareRenderer, &softwareRenderer->winN[0], y);
		}
	} else {
		softwareRenderer->windows[0].control.packed = 0xFF;
	}

	if (softwareRenderer->lastHighlightAmount != softwareRenderer->d.highlightAmount) {
		softwareRenderer->lastHighlightAmount = softwareRenderer->d.highlightAmount;
		if (softwareRenderer->lastHighlightAmount) {
			softwareRenderer->blendDirty = true;
		}
	}

	if (softwareRenderer->blendDirty) {
		_updatePalettes(softwareRenderer);
		softwareRenderer->blendDirty = false;
	}
	softwareRenderer->forceTarget1 = false;

	int w;
	x = 0;
	for (w = 0; w < softwareRenderer->nWindows; ++w) {
		// TOOD: handle objwin on backdrop
		uint32_t backdrop = FLAG_UNWRITTEN | FLAG_PRIORITY | FLAG_IS_BACKGROUND;
		if (!softwareRenderer->target1Bd || softwareRenderer->blendEffect == BLEND_NONE || softwareRenderer->blendEffect == BLEND_ALPHA || !GBAWindowControlIsBlendEnable(softwareRenderer->windows[w].control.packed)) {
			backdrop |= softwareRenderer->normalPalette[0];
		} else {
			backdrop |= softwareRenderer->variantPalette[0];
		}
		int end = softwareRenderer->windows[w].endX;
		for (; x < end - 3; x += 4) {
			softwareRenderer->row[x] = backdrop;
			softwareRenderer->row[x + 1] = backdrop;
			softwareRenderer->row[x + 2] = backdrop;
			softwareRenderer->row[x + 3] = backdrop;
		}
		for (; x < end; ++x) {
			softwareRenderer->row[x] = backdrop;
		}
	}

	softwareRenderer->bg[0].highlight = softwareRenderer->d.highlightBG[0];
	softwareRenderer->bg[1].highlight = softwareRenderer->d.highlightBG[1];
	softwareRenderer->bg[2].highlight = softwareRenderer->d.highlightBG[2];
	softwareRenderer->bg[3].highlight = softwareRenderer->d.highlightBG[3];

	_drawScanline(softwareRenderer, y);

	if ((softwareRenderer->forceTarget1 || softwareRenderer->bg[0].target1 || softwareRenderer->bg[1].target1 || softwareRenderer->bg[2].target1 || softwareRenderer->bg[3].target1) && softwareRenderer->target2Bd) {
		x = 0;
		for (w = 0; w < softwareRenderer->nWindows; ++w) {
			uint32_t backdrop = 0;
			if (!softwareRenderer->target1Bd || softwareRenderer->blendEffect == BLEND_NONE || softwareRenderer->blendEffect == BLEND_ALPHA || !GBAWindowControlIsBlendEnable(softwareRenderer->windows[w].control.packed)) {
				backdrop |= softwareRenderer->normalPalette[0];
			} else {
				backdrop |= softwareRenderer->variantPalette[0];
			}
			int end = softwareRenderer->windows[w].endX;
			for (; x < end; ++x) {
				uint32_t color = softwareRenderer->row[x];
				if (color & FLAG_TARGET_1) {
					softwareRenderer->row[x] = _mix(softwareRenderer->bldb, backdrop, softwareRenderer->blda, color);
				}
			}
		}
	}
	if (softwareRenderer->forceTarget1 && (softwareRenderer->blendEffect == BLEND_DARKEN || softwareRenderer->blendEffect == BLEND_BRIGHTEN)) {
		x = 0;
		uint32_t mask = FLAG_REBLEND | FLAG_IS_BACKGROUND;
		uint32_t match = FLAG_REBLEND;
		if (GBARegisterDISPCNTIsObjwinEnable(softwareRenderer->dispcnt)) {
			mask |= FLAG_OBJWIN;
			if (GBAWindowControlIsBlendEnable(softwareRenderer->objwin.packed)) {
				match |= FLAG_OBJWIN;
			}
		}
		for (w = 0; w < softwareRenderer->nWindows; ++w) {
			int end = softwareRenderer->windows[w].endX;
			if (!GBAWindowControlIsBlendEnable(softwareRenderer->windows[w].control.packed)) {
				x = end;
				continue;
			}
			if (softwareRenderer->blendEffect == BLEND_DARKEN) {
				for (; x < end; ++x) {
					uint32_t color = softwareRenderer->row[x];
					if ((color & mask) == match) {
						softwareRenderer->row[x] = _darken(color, softwareRenderer->bldy);
					}
				}
			} else if (softwareRenderer->blendEffect == BLEND_BRIGHTEN) {
				for (; x < end; ++x) {
					uint32_t color = softwareRenderer->row[x];
					if ((color & mask) == match) {
						softwareRenderer->row[x] = _brighten(color, softwareRenderer->bldy);
					}
				}
			}
		}
	}

#ifdef COLOR_16_BIT
	for (x = 0; x < GBA_VIDEO_HORIZONTAL_PIXELS; x += 4) {
		row[x] = softwareRenderer->row[x];
		row[x + 1] = softwareRenderer->row[x + 1];
		row[x + 2] = softwareRenderer->row[x + 2];
		row[x + 3] = softwareRenderer->row[x + 3];
	}
#else
	memcpy(row, softwareRenderer->row, GBA_VIDEO_HORIZONTAL_PIXELS * sizeof(*row));
#endif
}

static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	softwareRenderer->nextY = 0;
	if (softwareRenderer->temporaryBuffer) {
		mappedMemoryFree(softwareRenderer->temporaryBuffer, GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * 4);
		softwareRenderer->temporaryBuffer = 0;
	}
	softwareRenderer->bg[2].sx = softwareRenderer->bg[2].refx;
	softwareRenderer->bg[2].sy = softwareRenderer->bg[2].refy;
	softwareRenderer->bg[3].sx = softwareRenderer->bg[3].refx;
	softwareRenderer->bg[3].sy = softwareRenderer->bg[3].refy;

	if (softwareRenderer->bg[0].enabled > 0) {
		softwareRenderer->bg[0].enabled = 4;
	}
	if (softwareRenderer->bg[1].enabled > 0) {
		softwareRenderer->bg[1].enabled = 4;
	}
	if (softwareRenderer->bg[2].enabled > 0) {
		softwareRenderer->bg[2].enabled = 4;
	}
	if (softwareRenderer->bg[3].enabled > 0) {
		softwareRenderer->bg[3].enabled = 4;
	}
}

static void GBAVideoSoftwareRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	*stride = softwareRenderer->outputBufferStride;
	*pixels = softwareRenderer->outputBuffer;
}

static void GBAVideoSoftwareRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	const color_t* colorPixels = pixels;
	unsigned i;
	for (i = 0; i < GBA_VIDEO_VERTICAL_PIXELS; ++i) {
		memmove(&softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * i], &colorPixels[stride * i], GBA_VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL);
	}
}

static void _enableBg(struct GBAVideoSoftwareRenderer* renderer, int bg, bool active) {
	int wasActive = renderer->bg[bg].enabled;
	if (!active) {
		renderer->bg[bg].enabled = 0;
	} else if (!wasActive && active) {
		if (renderer->nextY == 0 || GBARegisterDISPCNTGetMode(renderer->dispcnt) > 2) {
			// TODO: Investigate in more depth how switching background works in different modes
			renderer->bg[bg].enabled = 4;
		} else {
			renderer->bg[bg].enabled = 1;
		}
	}
}

static void GBAVideoSoftwareRendererUpdateDISPCNT(struct GBAVideoSoftwareRenderer* renderer) {
	_enableBg(renderer, 0, GBARegisterDISPCNTGetBg0Enable(renderer->dispcnt));
	_enableBg(renderer, 1, GBARegisterDISPCNTGetBg1Enable(renderer->dispcnt));
	_enableBg(renderer, 2, GBARegisterDISPCNTGetBg2Enable(renderer->dispcnt));
	_enableBg(renderer, 3, GBARegisterDISPCNTGetBg3Enable(renderer->dispcnt));
}

static void GBAVideoSoftwareRendererWriteBGCNT(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	UNUSED(renderer);
	bg->priority = GBARegisterBGCNTGetPriority(value);
	bg->charBase = GBARegisterBGCNTGetCharBase(value) << 14;
	bg->mosaic = GBARegisterBGCNTGetMosaic(value);
	bg->multipalette = GBARegisterBGCNTGet256Color(value);
	bg->screenBase = GBARegisterBGCNTGetScreenBase(value) << 11;
	bg->overflow = GBARegisterBGCNTGetOverflow(value);
	bg->size = GBARegisterBGCNTGetSize(value);
}

static void GBAVideoSoftwareRendererWriteBGX_LO(struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	bg->refx = (bg->refx & 0xFFFF0000) | value;
	bg->sx = bg->refx;
}

static void GBAVideoSoftwareRendererWriteBGX_HI(struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	bg->refx = (bg->refx & 0x0000FFFF) | (value << 16);
	bg->refx <<= 4;
	bg->refx >>= 4;
	bg->sx = bg->refx;
}

static void GBAVideoSoftwareRendererWriteBGY_LO(struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	bg->refy = (bg->refy & 0xFFFF0000) | value;
	bg->sy = bg->refy;
}

static void GBAVideoSoftwareRendererWriteBGY_HI(struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	bg->refy = (bg->refy & 0x0000FFFF) | (value << 16);
	bg->refy <<= 4;
	bg->refy >>= 4;
	bg->sy = bg->refy;
}

static void GBAVideoSoftwareRendererWriteBLDCNT(struct GBAVideoSoftwareRenderer* renderer, uint16_t value) {
	enum GBAVideoBlendEffect oldEffect = renderer->blendEffect;

	renderer->bg[0].target1 = GBARegisterBLDCNTGetTarget1Bg0(value);
	renderer->bg[1].target1 = GBARegisterBLDCNTGetTarget1Bg1(value);
	renderer->bg[2].target1 = GBARegisterBLDCNTGetTarget1Bg2(value);
	renderer->bg[3].target1 = GBARegisterBLDCNTGetTarget1Bg3(value);
	renderer->bg[0].target2 = GBARegisterBLDCNTGetTarget2Bg0(value);
	renderer->bg[1].target2 = GBARegisterBLDCNTGetTarget2Bg1(value);
	renderer->bg[2].target2 = GBARegisterBLDCNTGetTarget2Bg2(value);
	renderer->bg[3].target2 = GBARegisterBLDCNTGetTarget2Bg3(value);

	renderer->blendEffect = GBARegisterBLDCNTGetEffect(value);
	renderer->target1Obj = GBARegisterBLDCNTGetTarget1Obj(value);
	renderer->target1Bd = GBARegisterBLDCNTGetTarget1Bd(value);
	renderer->target2Obj = GBARegisterBLDCNTGetTarget2Obj(value);
	renderer->target2Bd = GBARegisterBLDCNTGetTarget2Bd(value);

	if (oldEffect != renderer->blendEffect) {
		renderer->blendDirty = true;
	}
}

#define TEST_LAYER_ENABLED(X) \
	!renderer->d.disableBG[X] && \
	(renderer->bg[X].enabled == 4 && \
	(GBAWindowControlIsBg ## X ## Enable(renderer->currentWindow.packed) || \
	(GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt) && GBAWindowControlIsBg ## X ## Enable (renderer->objwin.packed))) && \
	renderer->bg[X].priority == priority)

static void _drawScanline(struct GBAVideoSoftwareRenderer* renderer, int y) {
	int w;
	int spriteLayers = 0;
	if (GBARegisterDISPCNTIsObjEnable(renderer->dispcnt) && !renderer->d.disableOBJ) {
		if (renderer->oamDirty) {
			renderer->oamMax = GBAVideoRendererCleanOAM(renderer->d.oam->obj, renderer->sprites, renderer->objOffsetY);
			renderer->oamDirty = false;
		}
		renderer->spriteCyclesRemaining = GBARegisterDISPCNTIsHblankIntervalFree(renderer->dispcnt) ? OBJ_HBLANK_FREE_LENGTH : OBJ_LENGTH;
		int mosaicV = GBAMosaicControlGetObjV(renderer->mosaic) + 1;
		int mosaicY = y - (y % mosaicV);
		int i;
		for (i = 0; i < renderer->oamMax; ++i) {
			struct GBAVideoRendererSprite* sprite = &renderer->sprites[i];
			int localY = y;
			renderer->end = 0;
			if ((y < sprite->y && (sprite->endY - 256 < 0 || y >= sprite->endY - 256)) || y >= sprite->endY) {
				continue;
			}
			if (GBAObjAttributesAIsMosaic(sprite->obj.a) && mosaicV > 1) {
				localY = mosaicY;
				if (localY < sprite->y && sprite->y < GBA_VIDEO_VERTICAL_PIXELS) {
					localY = sprite->y;
				}
				if (localY >= (sprite->endY & 0xFF)) {
					localY = sprite->endY - 1;
				}
			}
			for (w = 0; w < renderer->nWindows; ++w) {
				renderer->currentWindow = renderer->windows[w].control;
				renderer->start = renderer->end;
				renderer->end = renderer->windows[w].endX;
				// TODO: partial sprite drawing
				if (!GBAWindowControlIsObjEnable(renderer->currentWindow.packed) && !GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt)) {
					continue;
				}

				int drawn = GBAVideoSoftwareRendererPreprocessSprite(renderer, &sprite->obj, sprite->index, localY);
				spriteLayers |= drawn << GBAObjAttributesCGetPriority(sprite->obj.c);
			}
			int cycles = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->obj.a) * 4 + GBAObjAttributesBGetSize(sprite->obj.b)][0];
			if (GBAObjAttributesAIsTransformed(sprite->obj.a)) {
				cycles <<= GBAObjAttributesAGetDoubleSize(sprite->obj.a) + 1;
				cycles += 10;
			}
			renderer->spriteCyclesRemaining -= cycles;
			if (renderer->spriteCyclesRemaining <= 0) {
				break;
			}
		}
	}

	unsigned priority;
	for (priority = 0; priority < 4; ++priority) {
		renderer->end = 0;
		for (w = 0; w < renderer->nWindows; ++w) {
			renderer->start = renderer->end;
			renderer->end = renderer->windows[w].endX;
			renderer->currentWindow = renderer->windows[w].control;
			if (spriteLayers & (1 << priority)) {
				GBAVideoSoftwareRendererPostprocessSprite(renderer, priority);
			}
			if (TEST_LAYER_ENABLED(0) && GBARegisterDISPCNTGetMode(renderer->dispcnt) < 2) {
				GBAVideoSoftwareRendererDrawBackgroundMode0(renderer, &renderer->bg[0], y);
			}
			if (TEST_LAYER_ENABLED(1) && GBARegisterDISPCNTGetMode(renderer->dispcnt) < 2) {
				GBAVideoSoftwareRendererDrawBackgroundMode0(renderer, &renderer->bg[1], y);
			}
			if (TEST_LAYER_ENABLED(2)) {
				switch (GBARegisterDISPCNTGetMode(renderer->dispcnt)) {
				case 0:
					GBAVideoSoftwareRendererDrawBackgroundMode0(renderer, &renderer->bg[2], y);
					break;
				case 1:
				case 2:
					GBAVideoSoftwareRendererDrawBackgroundMode2(renderer, &renderer->bg[2], y);
					break;
				case 3:
					GBAVideoSoftwareRendererDrawBackgroundMode3(renderer, &renderer->bg[2], y);
					break;
				case 4:
					GBAVideoSoftwareRendererDrawBackgroundMode4(renderer, &renderer->bg[2], y);
					break;
				case 5:
					GBAVideoSoftwareRendererDrawBackgroundMode5(renderer, &renderer->bg[2], y);
					break;
				}
			}
			if (TEST_LAYER_ENABLED(3)) {
				switch (GBARegisterDISPCNTGetMode(renderer->dispcnt)) {
				case 0:
					GBAVideoSoftwareRendererDrawBackgroundMode0(renderer, &renderer->bg[3], y);
					break;
				case 2:
					GBAVideoSoftwareRendererDrawBackgroundMode2(renderer, &renderer->bg[3], y);
					break;
				}
			}
		}
	}
	if (GBARegisterDISPCNTGetMode(renderer->dispcnt) != 0) {
		renderer->bg[2].sx += renderer->bg[2].dmx;
		renderer->bg[2].sy += renderer->bg[2].dmy;
		renderer->bg[3].sx += renderer->bg[3].dmx;
		renderer->bg[3].sy += renderer->bg[3].dmy;
	}

	if (renderer->bg[0].enabled > 0 && renderer->bg[0].enabled < 4) {
		++renderer->bg[0].enabled;
		DIRTY_SCANLINE(renderer, y);
	}
	if (renderer->bg[1].enabled > 0 && renderer->bg[1].enabled < 4) {
		++renderer->bg[1].enabled;
		DIRTY_SCANLINE(renderer, y);
	}
	if (renderer->bg[2].enabled > 0 && renderer->bg[2].enabled < 4) {
		++renderer->bg[2].enabled;
		DIRTY_SCANLINE(renderer, y);
	}
	if (renderer->bg[3].enabled > 0 && renderer->bg[3].enabled < 4) {
		++renderer->bg[3].enabled;
		DIRTY_SCANLINE(renderer, y);
	}
}

static void _updatePalettes(struct GBAVideoSoftwareRenderer* renderer) {
	int i;
	if (renderer->blendEffect == BLEND_BRIGHTEN) {
		for (i = 0; i < 512; ++i) {
			renderer->variantPalette[i] = _brighten(renderer->normalPalette[i], renderer->bldy);
		}
	} else if (renderer->blendEffect == BLEND_DARKEN) {
		for (i = 0; i < 512; ++i) {
			renderer->variantPalette[i] = _darken(renderer->normalPalette[i], renderer->bldy);
		}
	} else {
		for (i = 0; i < 512; ++i) {
			renderer->variantPalette[i] = renderer->normalPalette[i];
		}
	}
	unsigned highlightAmount = renderer->d.highlightAmount >> 4;

	if (highlightAmount) {
		for (i = 0; i < 512; ++i) {
			renderer->highlightPalette[i] = _mix(0x10 - highlightAmount, renderer->normalPalette[i], highlightAmount, renderer->d.highlightColor);
			renderer->highlightVariantPalette[i] = _mix(0x10 - highlightAmount, renderer->variantPalette[i], highlightAmount, renderer->d.highlightColor);
		}
	}
}
