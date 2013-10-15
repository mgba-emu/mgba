#include "video-software.h"

#include "gba.h"
#include "gba-io.h"

#include <string.h>

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoSoftwareRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer);

static void GBAVideoSoftwareRendererUpdateDISPCNT(struct GBAVideoSoftwareRenderer* renderer);
static void GBAVideoSoftwareRendererWriteBGCNT(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGPA(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGPB(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGPC(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGPD(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGX_LO(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGX_HI(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGY_LO(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBGY_HI(struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBLDCNT(struct GBAVideoSoftwareRenderer* renderer, uint16_t value);

static void _drawScanline(struct GBAVideoSoftwareRenderer* renderer, int y);
static void _drawBackgroundMode0(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y);
static void _drawBackgroundMode2(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y);
static void _drawBackgroundMode3(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y);
static void _drawBackgroundMode4(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y);
static void _drawBackgroundMode5(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y);
static void _preprocessTransformedSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBATransformedObj* sprite, int y);
static void _preprocessSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int y);
static void _postprocessSprite(struct GBAVideoSoftwareRenderer* renderer, unsigned priority);

static void _updatePalettes(struct GBAVideoSoftwareRenderer* renderer);
static inline color_t _brighten(color_t color, int y);
static inline color_t _darken(color_t color, int y);
static color_t _mix(int weightA, color_t colorA, int weightB, color_t colorB);

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBAVideoSoftwareRendererInit;
	renderer->d.deinit = GBAVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBAVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writeOAM = GBAVideoSoftwareRendererWriteOAM;
	renderer->d.writePalette = GBAVideoSoftwareRendererWritePalette;
	renderer->d.drawScanline = GBAVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = GBAVideoSoftwareRendererFinishFrame;
}

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	int i;

	softwareRenderer->dispcnt.packed = 0x0080;

	softwareRenderer->target1Obj = 0;
	softwareRenderer->target1Bd = 0;
	softwareRenderer->target2Obj = 0;
	softwareRenderer->target2Bd = 0;
	softwareRenderer->blendEffect = BLEND_NONE;
	memset(softwareRenderer->normalPalette, 0, sizeof(softwareRenderer->normalPalette));
	memset(softwareRenderer->variantPalette, 0, sizeof(softwareRenderer->variantPalette));
	memset(softwareRenderer->enabledBitmap, 0, sizeof(softwareRenderer->enabledBitmap));

	softwareRenderer->blda = 0;
	softwareRenderer->bldb = 0;
	softwareRenderer->bldy = 0;

	softwareRenderer->winN[0].control.priority = 0;
	softwareRenderer->winN[1].control.priority = 1;
	softwareRenderer->objwin.priority = 2;
	softwareRenderer->winout.priority = 3;

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
	}
}

static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
}

static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	switch (address) {
	case REG_DISPCNT:
		softwareRenderer->dispcnt.packed = value;
		GBAVideoSoftwareRendererUpdateDISPCNT(softwareRenderer);
		break;
	case REG_BG0CNT:
		value &= 0xFFCF;
		GBAVideoSoftwareRendererWriteBGCNT(softwareRenderer, &softwareRenderer->bg[0], value);
		break;
	case REG_BG1CNT:
		value &= 0xFFCF;
		GBAVideoSoftwareRendererWriteBGCNT(softwareRenderer, &softwareRenderer->bg[1], value);
		break;
	case REG_BG2CNT:
		value &= 0xFFCF;
		GBAVideoSoftwareRendererWriteBGCNT(softwareRenderer, &softwareRenderer->bg[2], value);
		break;
	case REG_BG3CNT:
		value &= 0xFFCF;
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
		GBAVideoSoftwareRendererWriteBGPA(&softwareRenderer->bg[2], value);
		break;
	case REG_BG2PB:
		GBAVideoSoftwareRendererWriteBGPB(&softwareRenderer->bg[2], value);
		break;
	case REG_BG2PC:
		GBAVideoSoftwareRendererWriteBGPC(&softwareRenderer->bg[2], value);
		break;
	case REG_BG2PD:
		GBAVideoSoftwareRendererWriteBGPD(&softwareRenderer->bg[2], value);
		break;
	case REG_BG2X_LO:
		GBAVideoSoftwareRendererWriteBGX_LO(&softwareRenderer->bg[2], value);
		break;
	case REG_BG2X_HI:
		GBAVideoSoftwareRendererWriteBGX_HI(&softwareRenderer->bg[2], value);
		break;
	case REG_BG2Y_LO:
		GBAVideoSoftwareRendererWriteBGY_LO(&softwareRenderer->bg[2], value);
		break;
	case REG_BG2Y_HI:
		GBAVideoSoftwareRendererWriteBGY_HI(&softwareRenderer->bg[2], value);
		break;
	case REG_BG3PA:
		GBAVideoSoftwareRendererWriteBGPA(&softwareRenderer->bg[3], value);
		break;
	case REG_BG3PB:
		GBAVideoSoftwareRendererWriteBGPB(&softwareRenderer->bg[3], value);
		break;
	case REG_BG3PC:
		GBAVideoSoftwareRendererWriteBGPC(&softwareRenderer->bg[3], value);
		break;
	case REG_BG3PD:
		GBAVideoSoftwareRendererWriteBGPD(&softwareRenderer->bg[3], value);
		break;
	case REG_BG3X_LO:
		GBAVideoSoftwareRendererWriteBGX_LO(&softwareRenderer->bg[3], value);
		break;
	case REG_BG3X_HI:
		GBAVideoSoftwareRendererWriteBGX_HI(&softwareRenderer->bg[3], value);
		break;
	case REG_BG3Y_LO:
		GBAVideoSoftwareRendererWriteBGY_LO(&softwareRenderer->bg[3], value);
		break;
	case REG_BG3Y_HI:
		GBAVideoSoftwareRendererWriteBGY_HI(&softwareRenderer->bg[3], value);
		break;
	case REG_BLDCNT:
		GBAVideoSoftwareRendererWriteBLDCNT(softwareRenderer, value);
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
		break;
	case REG_BLDY:
		softwareRenderer->bldy = value & 0x1F;
		if (softwareRenderer->bldy > 0x10) {
			softwareRenderer->bldy = 0x10;
		}
		_updatePalettes(softwareRenderer);
		break;
	case REG_WIN0H:
		softwareRenderer->winN[0].h.packed = value;
		if (softwareRenderer->winN[0].h.start > softwareRenderer->winN[0].h.end || softwareRenderer->winN[0].h.end > VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[0].h.end = VIDEO_HORIZONTAL_PIXELS;
		}
		break;
	case REG_WIN1H:
		softwareRenderer->winN[1].h.packed = value;
		if (softwareRenderer->winN[1].h.start > softwareRenderer->winN[1].h.end || softwareRenderer->winN[1].h.end > VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[1].h.end = VIDEO_HORIZONTAL_PIXELS;
		}
		break;
	case REG_WIN0V:
		softwareRenderer->winN[0].v.packed = value;
		if (softwareRenderer->winN[0].v.start > softwareRenderer->winN[0].v.end || softwareRenderer->winN[0].v.end > VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[0].v.end = VIDEO_VERTICAL_PIXELS;
		}
		break;
	case REG_WIN1V:
		softwareRenderer->winN[1].v.packed = value;
		if (softwareRenderer->winN[1].v.start > softwareRenderer->winN[1].v.end || softwareRenderer->winN[1].v.end > VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[1].v.end = VIDEO_VERTICAL_PIXELS;
		}
		break;
	case REG_WININ:
		softwareRenderer->winN[0].control.packed = value;
		softwareRenderer->winN[1].control.packed = value >> 8;
		break;
	case REG_WINOUT:
		softwareRenderer->winout.packed = value;
		softwareRenderer->objwin.packed = value >> 8;
		break;
	case REG_MOSAIC:
	case REG_GREENSWP:
		GBALog(0, GBA_LOG_STUB, "Stub video register write: 0x%03X", address);
		break;
	default:
		GBALog(0, GBA_LOG_GAME_ERROR, "Invalid video register: 0x%03X", address);
	}
	return value;
}

static void GBAVideoSoftwareRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	if ((oam & 0x3) != 0x3) {
		oam >>= 2;
		struct GBAObj* sprite = &renderer->oam->obj[oam];
		int enabled = sprite->transformed || !sprite->disable;
		enabled <<= (oam & 0x1F);
		softwareRenderer->enabledBitmap[oam >> 5] = (softwareRenderer->enabledBitmap[oam >> 5] & ~(1 << (oam & 0x1F))) | enabled;
	}
}

static void GBAVideoSoftwareRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
#ifdef COLOR_16_BIT
	color_t color = value;
#else
	color_t color = 0;
	color |= (value << 3) & 0xF8;
	color |= (value << 6) & 0xF800;
	color |= (value << 9) & 0xF80000;
#endif
	softwareRenderer->normalPalette[address >> 1] = color;
	if (softwareRenderer->blendEffect == BLEND_BRIGHTEN) {
		softwareRenderer->variantPalette[address >> 1] = _brighten(color, softwareRenderer->bldy);
	} else if (softwareRenderer->blendEffect == BLEND_DARKEN) {
		softwareRenderer->variantPalette[address >> 1] = _darken(color, softwareRenderer->bldy);
	}
}

static void _breakWindow(struct GBAVideoSoftwareRenderer* softwareRenderer, struct WindowN* win) {
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
					for (++activeWindow; win->h.end >= softwareRenderer->windows[activeWindow].endX && softwareRenderer->nWindows > activeWindow; ++activeWindow) {
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
}

static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
	if (softwareRenderer->dispcnt.forcedBlank) {
		int x;
		for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GBA_COLOR_WHITE;
		}
		return;
	}

	memset(softwareRenderer->spriteLayer, 0, sizeof(softwareRenderer->spriteLayer));

	softwareRenderer->windows[0].endX = VIDEO_HORIZONTAL_PIXELS;
	softwareRenderer->nWindows = 1;
	if (softwareRenderer->dispcnt.win0Enable || softwareRenderer->dispcnt.win1Enable || softwareRenderer->dispcnt.objwinEnable) {
		softwareRenderer->windows[0].control = softwareRenderer->winout;
		if (softwareRenderer->dispcnt.win1Enable && y < softwareRenderer->winN[1].v.end && y >= softwareRenderer->winN[1].v.start) {
			_breakWindow(softwareRenderer, &softwareRenderer->winN[1]);
		}
		if (softwareRenderer->dispcnt.win0Enable && y < softwareRenderer->winN[0].v.end && y >= softwareRenderer->winN[0].v.start) {
			_breakWindow(softwareRenderer, &softwareRenderer->winN[0]);
		}
	} else {
		softwareRenderer->windows[0].control.packed = 0xFF;
	}

	int w;
	int x = 0;
	for (w = 0; w < softwareRenderer->nWindows; ++w) {
		// TOOD: handle objwin on backdrop
		uint32_t backdrop = FLAG_UNWRITTEN | FLAG_PRIORITY | FLAG_IS_BACKGROUND;
		if (!softwareRenderer->target1Bd || softwareRenderer->blendEffect == BLEND_NONE || softwareRenderer->blendEffect == BLEND_ALPHA || !softwareRenderer->windows[w].control.blendEnable) {
			backdrop |= softwareRenderer->normalPalette[0];
		} else {
			backdrop |= softwareRenderer->variantPalette[0];
		}
		for (; x < softwareRenderer->windows[w].endX; ++x) {
			softwareRenderer->row[x] = backdrop;
		}
	}

	_drawScanline(softwareRenderer, y);

	if (softwareRenderer->target2Bd) {
		x = 0;
		for (w = 0; w < softwareRenderer->nWindows; ++w) {
		uint32_t backdrop = FLAG_UNWRITTEN | FLAG_PRIORITY | FLAG_IS_BACKGROUND;
			if (!softwareRenderer->target1Bd || softwareRenderer->blendEffect == BLEND_NONE || softwareRenderer->blendEffect == BLEND_ALPHA || !softwareRenderer->windows[w].control.blendEnable) {
				backdrop |= softwareRenderer->normalPalette[0];
			} else {
				backdrop |= softwareRenderer->variantPalette[0];
			}
			for (; x < softwareRenderer->windows[w].endX; ++x) {
				uint32_t color = softwareRenderer->row[x];
				if (color & FLAG_TARGET_1 && !(color & FLAG_FINALIZED)) {
					softwareRenderer->row[x] = _mix(softwareRenderer->bldb, backdrop, softwareRenderer->blda, color);
				}
			}
		}
	}

#ifdef COLOR_16_BIT
	for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
		uint32_t c = softwareRenderer->row[x];
#ifdef COLOR_5_6_5
		c = ((c & 0x001F) << 11) | ((c & 0x03E0) << 1) | ((c & 0x7C00) >> 10);
#endif
		row[x] = c;
	}
#else
	memcpy(row, softwareRenderer->row, VIDEO_HORIZONTAL_PIXELS * sizeof(*row));
#endif
}

static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	softwareRenderer->bg[2].sx = softwareRenderer->bg[2].refx;
	softwareRenderer->bg[2].sy = softwareRenderer->bg[2].refy;
	softwareRenderer->bg[3].sx = softwareRenderer->bg[3].refx;
	softwareRenderer->bg[3].sy = softwareRenderer->bg[3].refy;
}

static void GBAVideoSoftwareRendererUpdateDISPCNT(struct GBAVideoSoftwareRenderer* renderer) {
	renderer->bg[0].enabled = renderer->dispcnt.bg0Enable;
	renderer->bg[1].enabled = renderer->dispcnt.bg1Enable;
	renderer->bg[2].enabled = renderer->dispcnt.bg2Enable;
	renderer->bg[3].enabled = renderer->dispcnt.bg3Enable;
}

static void GBAVideoSoftwareRendererWriteBGCNT(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	(void)(renderer);
	union GBARegisterBGCNT reg = { .packed = value };
	bg->priority = reg.priority;
	bg->charBase = reg.charBase << 14;
	bg->mosaic = reg.mosaic;
	bg->multipalette = reg.multipalette;
	bg->screenBase = reg.screenBase << 11;
	bg->overflow = reg.overflow;
	bg->size = reg.size;
}

static void GBAVideoSoftwareRendererWriteBGPA(struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	bg->dx = value;
}

static void GBAVideoSoftwareRendererWriteBGPB(struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	bg->dmx = value;
}

static void GBAVideoSoftwareRendererWriteBGPC(struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	bg->dy = value;
}

static void GBAVideoSoftwareRendererWriteBGPD(struct GBAVideoSoftwareBackground* bg, uint16_t value) {
	bg->dmy = value;
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
	union {
		struct {
			unsigned target1Bg0 : 1;
			unsigned target1Bg1 : 1;
			unsigned target1Bg2 : 1;
			unsigned target1Bg3 : 1;
			unsigned target1Obj : 1;
			unsigned target1Bd : 1;
			enum BlendEffect effect : 2;
			unsigned target2Bg0 : 1;
			unsigned target2Bg1 : 1;
			unsigned target2Bg2 : 1;
			unsigned target2Bg3 : 1;
			unsigned target2Obj : 1;
			unsigned target2Bd : 1;
		};
		uint16_t packed;
	} bldcnt = { .packed = value };

	enum BlendEffect oldEffect = renderer->blendEffect;

	renderer->bg[0].target1 = bldcnt.target1Bg0;
	renderer->bg[1].target1 = bldcnt.target1Bg1;
	renderer->bg[2].target1 = bldcnt.target1Bg2;
	renderer->bg[3].target1 = bldcnt.target1Bg3;
	renderer->bg[0].target2 = bldcnt.target2Bg0;
	renderer->bg[1].target2 = bldcnt.target2Bg1;
	renderer->bg[2].target2 = bldcnt.target2Bg2;
	renderer->bg[3].target2 = bldcnt.target2Bg3;

	renderer->blendEffect = bldcnt.effect;
	renderer->target1Obj = bldcnt.target1Obj;
	renderer->target1Bd = bldcnt.target1Bd;
	renderer->target2Obj = bldcnt.target2Obj;
	renderer->target2Bd = bldcnt.target2Bd;

	if (oldEffect != renderer->blendEffect) {
		_updatePalettes(renderer);
	}
}

#define TEST_LAYER_ENABLED(X) \
	(renderer->bg[X].enabled && \
	(renderer->currentWindow.bg ## X ## Enable || \
	(renderer->dispcnt.objwinEnable && renderer->objwin.bg ## X ## Enable)) && \
	renderer->bg[X].priority == priority)

static void _drawScanline(struct GBAVideoSoftwareRenderer* renderer, int y) {
	int w;
	renderer->end = 0;
	if (renderer->dispcnt.objEnable) {
		for (w = 0; w < renderer->nWindows; ++w) {
			renderer->start = renderer->end;
			renderer->end = renderer->windows[w].endX;
			renderer->currentWindow = renderer->windows[w].control;
			if (!renderer->currentWindow.objEnable) {
				continue;
			}
			int i, j;
			for (j = 0; j < 4; ++j) {
				uint32_t bitmap = renderer->enabledBitmap[j];
				if (!bitmap) {
					continue;
				}
				for (i = j * 32; i < (j + 1) * 32; ++i) {
					if (bitmap & 1) {
						struct GBAObj* sprite = &renderer->d.oam->obj[i];
						if (sprite->transformed) {
							_preprocessTransformedSprite(renderer, &renderer->d.oam->tobj[i], y);
						} else {
							_preprocessSprite(renderer, sprite, y);
						}
					}
					bitmap >>= 1;
				}
			}
		}
	}

	int priority;
	for (priority = 0; priority < 4; ++priority) {
		_postprocessSprite(renderer, priority);
		renderer->end = 0;
		for (w = 0; w < renderer->nWindows; ++w) {
			renderer->start = renderer->end;
			renderer->end = renderer->windows[w].endX;
			renderer->currentWindow = renderer->windows[w].control;
			if (TEST_LAYER_ENABLED(0) && renderer->dispcnt.mode < 2) {
				_drawBackgroundMode0(renderer, &renderer->bg[0], y);
			}
			if (TEST_LAYER_ENABLED(1) && renderer->dispcnt.mode < 2) {
				_drawBackgroundMode0(renderer, &renderer->bg[1], y);
			}
			if (TEST_LAYER_ENABLED(2)) {
				switch (renderer->dispcnt.mode) {
				case 0:
					_drawBackgroundMode0(renderer, &renderer->bg[2], y);
					break;
				case 1:
				case 2:
					_drawBackgroundMode2(renderer, &renderer->bg[2], y);
					break;
				case 3:
					_drawBackgroundMode3(renderer, &renderer->bg[2], y);
					break;
				case 4:
					_drawBackgroundMode4(renderer, &renderer->bg[2], y);
					break;
				case 5:
					_drawBackgroundMode5(renderer, &renderer->bg[2], y);
					break;
				}
				renderer->bg[2].sx += renderer->bg[2].dmx;
				renderer->bg[2].sy += renderer->bg[2].dmy;
			}
			if (TEST_LAYER_ENABLED(3)) {
				switch (renderer->dispcnt.mode) {
				case 0:
					_drawBackgroundMode0(renderer, &renderer->bg[3], y);
					break;
				case 2:
					_drawBackgroundMode2(renderer, &renderer->bg[3], y);
					break;
				}
				renderer->bg[3].sx += renderer->bg[3].dmx;
				renderer->bg[3].sy += renderer->bg[3].dmy;
			}
		}
	}
}

static void _composite(struct GBAVideoSoftwareRenderer* renderer, int offset, uint32_t color, uint32_t current) {
	// We stash the priority on the top bits so we can do a one-operator comparison
	// The lower the number, the higher the priority, and sprites take precendence over backgrounds
	// We want to do special processing if the color pixel is target 1, however
	if ((color & FLAG_ORDER_MASK) < (current & FLAG_ORDER_MASK)) {
		if (current & FLAG_UNWRITTEN) {
			renderer->row[offset] = color | (current & FLAG_OBJWIN);
		} else if (!(color & FLAG_TARGET_1) || !(current & FLAG_TARGET_2)) {
			renderer->row[offset] = color | FLAG_FINALIZED;
		} else {
			renderer->row[offset] = _mix(renderer->bldb, current, renderer->blda, color) | FLAG_FINALIZED;
		}
	} else {
		if (current & FLAG_TARGET_1 && color & FLAG_TARGET_2) {
			renderer->row[offset] = _mix(renderer->blda, current, renderer->bldb, color) | FLAG_FINALIZED;
		} else {
			renderer->row[offset] = current | FLAG_FINALIZED;
		}
	}
}

#define BACKGROUND_DRAW_PIXEL_16 \
	pixelData = tileData & 0xF; \
	current = renderer->row[outX]; \
	if (pixelData && !(current & FLAG_FINALIZED)) { \
		if (!objwinSlowPath) { \
			_composite(renderer, outX, palette[pixelData | paletteData] | flags, current); \
		} else if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) { \
			color_t* currentPalette = (current & FLAG_OBJWIN) ? objwinPalette : palette; \
			_composite(renderer, outX, currentPalette[pixelData | paletteData] | flags, current); \
		} \
	} \
	tileData >>= 4;

#define BACKGROUND_DRAW_PIXEL_256 \
	pixelData = tileData & 0xFF; \
	current = renderer->row[outX]; \
	if (pixelData && !(current & FLAG_FINALIZED)) { \
		if (!objwinSlowPath) { \
			_composite(renderer, outX, palette[pixelData] | flags, current); \
		} else if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) { \
			color_t* currentPalette = (current & FLAG_OBJWIN) ? objwinPalette : palette; \
			_composite(renderer, outX, currentPalette[pixelData] | flags, current); \
		} \
	} \
	tileData >>= 8;

#define BACKGROUND_TEXT_SELECT_CHARACTER \
	localX = tileX * 8 + inX; \
	xBase = localX & 0xF8; \
	if (background->size & 1) { \
		xBase += (localX & 0x100) << 5; \
	} \
	screenBase = (background->screenBase >> 1) + (xBase >> 3) + (yBase << 2); \
	mapData.packed = renderer->d.vram[screenBase]; \
	if (!mapData.vflip) { \
		localY = inY & 0x7; \
	} else { \
		localY = 7 - (inY & 0x7); \
	}

#define BACKGROUND_MODE_0_TILE_16_LOOP \
	uint32_t tileData; \
	uint32_t current; \
	int paletteData, pixelData; \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		paletteData = mapData.palette << 4; \
		charBase = ((background->charBase + (mapData.tile << 5)) >> 2) + localY; \
		tileData = ((uint32_t*)renderer->d.vram)[charBase]; \
		if (tileData) { \
			if (!mapData.hflip) { \
				BACKGROUND_DRAW_PIXEL_16; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				++outX; \
			} else { \
				outX += 7; \
				BACKGROUND_DRAW_PIXEL_16; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16; \
				outX += 8; \
			} \
		} else { \
			outX += 8; \
		} \
	}

#define BACKGROUND_MODE_0_TILE_256_LOOP \
	uint32_t tileData; \
	uint32_t current; \
	int pixelData; \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = ((background->charBase + (mapData.tile << 6)) >> 2) + (localY << 1); \
		if (!mapData.hflip) { \
			tileData = ((uint32_t*)renderer->d.vram)[charBase]; \
			if (tileData) { \
					BACKGROUND_DRAW_PIXEL_256; \
					++outX; \
					BACKGROUND_DRAW_PIXEL_256; \
					++outX; \
					BACKGROUND_DRAW_PIXEL_256; \
					++outX; \
					BACKGROUND_DRAW_PIXEL_256; \
					++outX; \
			} else { \
				outX += 4; \
			} \
			tileData = ((uint32_t*)renderer->d.vram)[charBase + 1]; \
			if (tileData) { \
					BACKGROUND_DRAW_PIXEL_256; \
					++outX; \
					BACKGROUND_DRAW_PIXEL_256; \
					++outX; \
					BACKGROUND_DRAW_PIXEL_256; \
					++outX; \
					BACKGROUND_DRAW_PIXEL_256; \
					++outX; \
			} else { \
				outX += 4; \
			} \
		} else { \
			uint32_t tileData = ((uint32_t*)renderer->d.vram)[charBase + 1]; \
			if (tileData) { \
				outX += 3; \
				BACKGROUND_DRAW_PIXEL_256; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_256; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_256; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_256; \
				outX += 4; \
			} else { \
				outX += 4; \
			} \
			tileData = ((uint32_t*)renderer->d.vram)[charBase]; \
			if (tileData) { \
				outX += 3; \
				BACKGROUND_DRAW_PIXEL_256; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_256; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_256; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_256; \
				outX += 4; \
			} else { \
				outX += 4; \
			} \
		} \
	}

#define PREPARE_OBJWIN \
	int objwinSlowPath = renderer->dispcnt.objwinEnable; \
	int objwinOnly = 0; \
	int objwinForceEnable = 0; \
	color_t* objwinPalette; \
	if (objwinSlowPath) { \
		if (background->target1 && renderer->objwin.blendEnable && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN)) { \
			objwinPalette = renderer->variantPalette; \
		} else { \
			objwinPalette = renderer->normalPalette; \
		} \
		switch (background->index) { \
		case 0: \
			objwinForceEnable = renderer->objwin.bg0Enable && renderer->currentWindow.bg0Enable; \
			objwinOnly = !renderer->objwin.bg0Enable; \
			break; \
		case 1: \
			objwinForceEnable = renderer->objwin.bg1Enable && renderer->currentWindow.bg1Enable; \
			objwinOnly = !renderer->objwin.bg1Enable; \
			break; \
		case 2: \
			objwinForceEnable = renderer->objwin.bg2Enable && renderer->currentWindow.bg2Enable; \
			objwinOnly = !renderer->objwin.bg2Enable; \
			break; \
		case 3: \
			objwinForceEnable = renderer->objwin.bg3Enable && renderer->currentWindow.bg3Enable; \
			objwinOnly = !renderer->objwin.bg3Enable; \
			break; \
		} \
	}

static void _drawBackgroundMode0(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y) {
	int inX = renderer->start + background->x;
	int inY = y + background->y;
	union GBATextMapData mapData;

	unsigned yBase = inY & 0xF8;
	if (background->size == 2) {
		yBase += inY & 0x100;
	} else if (background->size == 3) {
		yBase += (inY & 0x100) << 1;
	}

	int localX;
	int localY;

	unsigned xBase;

	int flags = (background->priority << OFFSET_PRIORITY) | FLAG_IS_BACKGROUND;
	flags |= FLAG_TARGET_1 * (background->target1 && renderer->blendEffect == BLEND_ALPHA);
	flags |= FLAG_TARGET_2 * background->target2;

	uint32_t screenBase;
	uint32_t charBase;
	int variant = background->target1 && renderer->currentWindow.blendEnable && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	color_t* palette = renderer->normalPalette;
	if (variant) {
		palette = renderer->variantPalette;
	}
	PREPARE_OBJWIN;

	int outX = renderer->start;
	int tileX = 0;
	int tileEnd = (renderer->end - renderer->start + (inX & 0x7)) >> 3;
	if (inX & 0x7) {
		uint32_t tileData;
		uint32_t current;
		int pixelData, paletteData;
		int mod8 = inX & 0x7;
		BACKGROUND_TEXT_SELECT_CHARACTER;

		int end = outX + 0x8 - mod8;
		if (!background->multipalette) {
			paletteData = mapData.palette << 4;
			charBase = ((background->charBase + (mapData.tile << 5)) >> 2) + localY;
			tileData = ((uint32_t*)renderer->d.vram)[charBase];
			if (!mapData.hflip) {
				tileData >>= 4 * mod8;
				for (; outX < end; ++outX) {
					BACKGROUND_DRAW_PIXEL_16;
				}
			} else {
				for (outX = end - 1; outX >= renderer->start; --outX) {
					BACKGROUND_DRAW_PIXEL_16;
				}
			}
		} else {
			// TODO: hflip
			charBase = ((background->charBase + (mapData.tile << 6)) >> 2) + (localY << 1);
			int end2 = end - 4;
			int shift = inX & 0x3;
			if (end2 > 0) {
				tileData = ((uint32_t*)renderer->d.vram)[charBase];
				tileData >>= 8 * shift;
				shift = 0;
				for (; outX < end2; ++outX) {
					BACKGROUND_DRAW_PIXEL_256;
				}
			}

			tileData = ((uint32_t*)renderer->d.vram)[charBase + 1];
			tileData >>= 8 * shift;
			for (; outX < end; ++outX) {
				BACKGROUND_DRAW_PIXEL_256;
			}
		}
	}
	if (inX & 0x7 || (renderer->end - renderer->start) & 0x7) {
		tileX = tileEnd;
		uint32_t tileData;
		uint32_t current;
		int pixelData, paletteData;
		int mod8 = (inX + renderer->end - renderer->start) & 0x7;
		BACKGROUND_TEXT_SELECT_CHARACTER;

		int end = 0x8 - mod8;
		if (!background->multipalette) {
			charBase = ((background->charBase + (mapData.tile << 5)) >> 2) + localY;
			tileData = ((uint32_t*)renderer->d.vram)[charBase];
			paletteData = mapData.palette << 4;
			if (!mapData.hflip) {
				outX = renderer->end - mod8;
				if (outX < 0) {
					tileData >>= 4 * -outX;
					outX = 0;
				}
				for (; outX < renderer->end; ++outX) {
					BACKGROUND_DRAW_PIXEL_16;
				}
			} else {
				tileData >>= 4 * (0x8 - mod8);
				int end2 = renderer->end - 8;
				if (end2 < -1) {
					end2 = -1;
				}
				for (outX = renderer->end - 1; outX > end2; --outX) {
					BACKGROUND_DRAW_PIXEL_16;
				}
			}
		} else {
			// TODO: hflip
			charBase = ((background->charBase + (mapData.tile << 6)) >> 2) + (localY << 1);
			outX = renderer->end - 8 + end;
			int end2 = 4 - end;
			if (end2 > 0) {
				tileData = ((uint32_t*)renderer->d.vram)[charBase];
				for (; outX < renderer->end - end2; ++outX) {
					BACKGROUND_DRAW_PIXEL_256;
				}
				++charBase;
			}

			tileData = ((uint32_t*)renderer->d.vram)[charBase];
			for (; outX < renderer->end; ++outX) {
				BACKGROUND_DRAW_PIXEL_256;
			}
		}

		tileX = (inX & 0x7) != 0;
		outX = renderer->start + tileX * 8 - (inX & 0x7);
	}

	if (!background->multipalette) {
		BACKGROUND_MODE_0_TILE_16_LOOP;
	} else {
		BACKGROUND_MODE_0_TILE_256_LOOP;
	}
}

#define BACKGROUND_BITMAP_INIT \
	(void)(unused); \
	int32_t x = background->sx - background->dx; \
	int32_t y = background->sy - background->dy; \
	int32_t localX; \
	int32_t localY; \
	\
	int flags = (background->priority << OFFSET_PRIORITY) | FLAG_IS_BACKGROUND; \
	flags |= FLAG_TARGET_1 * (background->target1 && renderer->blendEffect == BLEND_ALPHA); \
	flags |= FLAG_TARGET_2 * background->target2; \
	int variant = background->target1 && renderer->currentWindow.blendEnable && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN); \
	color_t* palette = renderer->normalPalette; \
	if (variant) { \
		palette = renderer->variantPalette; \
	} \
	PREPARE_OBJWIN;

#define BACKGROUND_BITMAP_ITERATE(W, H) \
	x += background->dx; \
	y += background->dy; \
	\
	if (x < 0 || y < 0 || (x >> 8) >= W || (y >> 8) >= H) { \
		continue; \
	} else { \
		localX = x; \
		localY = y; \
	}

static void _drawBackgroundMode2(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int unused) {
	int sizeAdjusted = 0x8000 << background->size;

	BACKGROUND_BITMAP_INIT;

	uint32_t screenBase = background->screenBase;
	uint32_t charBase = background->charBase;
	uint8_t mapData;
	uint8_t tileData;

	int outX;
	for (outX = renderer->start; outX < VIDEO_HORIZONTAL_PIXELS; ++outX) {
		x += background->dx;
		y += background->dy;

		if (background->overflow) {
			localX = x & (sizeAdjusted - 1);
			localY = y & (sizeAdjusted - 1);
		} else if (x < 0 || y < 0 || x >= sizeAdjusted || y >= sizeAdjusted) {
			continue;
		} else {
			localX = x;
			localY = y;
		}
		mapData = ((uint8_t*)renderer->d.vram)[screenBase + (localX >> 11) + (((localY >> 7) & 0x7F0) << background->size)];
		tileData = ((uint8_t*)renderer->d.vram)[charBase + (mapData << 6) + ((localY & 0x700) >> 5) + ((localX & 0x700) >> 8)];

		uint32_t current = renderer->row[outX];
		if (tileData && !(current & FLAG_FINALIZED)) {
			if (!objwinSlowPath) {
				_composite(renderer, outX, palette[tileData] | flags, current);
			} else if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) {
				color_t* currentPalette = (current & FLAG_OBJWIN) ? objwinPalette : palette;
				_composite(renderer, outX, currentPalette[tileData] | flags, current);
			}
		}
	}
}

static void _drawBackgroundMode3(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int unused) {
	BACKGROUND_BITMAP_INIT;

	uint16_t color;
	uint32_t color32;

	int outX;
	for (outX = 0; outX < VIDEO_HORIZONTAL_PIXELS; ++outX) {
		BACKGROUND_BITMAP_ITERATE(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);

		color = ((uint16_t*)renderer->d.vram)[(localX >> 8) + (localY >> 8) * VIDEO_HORIZONTAL_PIXELS];
		color32 = 0;
		color32 |= (color << 3) & 0xF8;
		color32 |= (color << 6) & 0xF800;
		color32 |= (color << 9) & 0xF80000;

		uint32_t current = renderer->row[outX];
		if (!(current & FLAG_FINALIZED) && (!objwinSlowPath || !(current & FLAG_OBJWIN) != objwinOnly)) {
			if (!variant) {
				_composite(renderer, outX, color32 | flags, current);
			} else if (renderer->blendEffect == BLEND_BRIGHTEN) {
				_composite(renderer, outX, _brighten(color32, renderer->bldy) | flags, current);
			} else if (renderer->blendEffect == BLEND_DARKEN) {
				_composite(renderer, outX, _darken(color32, renderer->bldy) | flags, current);
			}
		}
	}
}

static void _drawBackgroundMode4(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int unused) {
	BACKGROUND_BITMAP_INIT;

	uint16_t color;
	uint32_t offset = 0;
	if (renderer->dispcnt.frameSelect) {
		offset = 0xA000;
	}

	int outX;
	for (outX = 0; outX < VIDEO_HORIZONTAL_PIXELS; ++outX) {
		BACKGROUND_BITMAP_ITERATE(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);

		color = ((uint8_t*)renderer->d.vram)[offset + (localX >> 8) + (localY >> 8) * VIDEO_HORIZONTAL_PIXELS];

		uint32_t current = renderer->row[outX];
		if (color && !(current & FLAG_FINALIZED) && (!objwinSlowPath || !(current & FLAG_OBJWIN) != objwinOnly)) {
			if (!variant) {
				_composite(renderer, outX, renderer->normalPalette[color] | flags, current);
			} else {
				_composite(renderer, outX, renderer->variantPalette[color] | flags, current);
			}
		}
	}
}

static void _drawBackgroundMode5(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int unused) {
	BACKGROUND_BITMAP_INIT;

	uint32_t color;
	uint32_t offset = 0;
	if (renderer->dispcnt.frameSelect) {
		offset = 0xA000;
	}

	int outX;
	for (outX = 0; outX < VIDEO_HORIZONTAL_PIXELS; ++outX) {
		BACKGROUND_BITMAP_ITERATE(160, 128);

		color = ((uint16_t*)renderer->d.vram)[offset + (localX >> 8) + (localY >> 8) * 160];
#ifndef COLOR_16_BIT
		color_t color32 = 0;
		color32 |= (color << 9) & 0xF80000;
		color32 |= (color << 3) & 0xF8;
		color32 |= (color << 6) & 0xF800;
		color = color32;
#endif

		uint32_t current = renderer->row[outX];
		if (!(current & FLAG_FINALIZED) && (!objwinSlowPath || !(current & FLAG_OBJWIN) != objwinOnly)) {
			if (!variant) {
				_composite(renderer, outX, color | flags, current);
			} else if (renderer->blendEffect == BLEND_BRIGHTEN) {
				_composite(renderer, outX, _brighten(color, renderer->bldy) | flags, current);
			} else if (renderer->blendEffect == BLEND_DARKEN) {
				_composite(renderer, outX, _darken(color, renderer->bldy) | flags, current);
			}
		}
	}
}

static const int _objSizes[32] = {
	8, 8,
	16, 16,
	32, 32,
	64, 64,
	16, 8,
	32, 8,
	32, 16,
	64, 32,
	8, 16,
	8, 32,
	16, 32,
	32, 64,
	0, 0,
	0, 0,
	0, 0,
	0, 0
};

#define SPRITE_NORMAL_LOOP(DEPTH, TYPE) \
		SPRITE_YBASE_ ## DEPTH(inY); \
		int outX = x >= start ? x : start; \
		int condition = x + width; \
		if (end < condition) { \
			condition = end; \
		} \
		for (; outX < condition; ++outX) { \
			int inX = outX - x; \
			if (sprite->hflip) { \
				inX = width - inX - 1; \
			} \
			if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
				continue; \
			} \
			SPRITE_XBASE_ ## DEPTH(inX); \
			SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(inX); \
		}

#define SPRITE_TRANSFORMED_LOOP(DEPTH, TYPE) \
	int outX; \
	for (outX = x >= start ? x : start; outX < x + totalWidth && outX < end; ++outX) { \
		if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
			continue; \
		} \
		int inX = outX - x; \
		int localX = ((mat->a * (inX - (totalWidth >> 1)) + mat->b * (inY - (totalHeight >> 1))) >> 8) + (width >> 1); \
		int localY = ((mat->c * (inX - (totalWidth >> 1)) + mat->d * (inY - (totalHeight >> 1))) >> 8) + (height >> 1); \
		\
		if (localX < 0 || localX >= width || localY < 0 || localY >= height) { \
			continue; \
		} \
		\
		SPRITE_YBASE_ ## DEPTH(localY); \
		SPRITE_XBASE_ ## DEPTH(localX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(localX); \
	}

#define SPRITE_XBASE_16(localX) unsigned xBase = (localX & ~0x7) * 4 + ((localX >> 1) & 2);
#define SPRITE_YBASE_16(localY) unsigned yBase = (localY & ~0x7) * (renderer->dispcnt.objCharacterMapping ? width >> 1 : 0x80) + (localY & 0x7) * 4;

#define SPRITE_DRAW_PIXEL_16_NORMAL(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData && (!(renderer->spriteLayer[outX]) || ((renderer->spriteLayer[outX] & FLAG_ORDER_MASK) > flags))) { \
		renderer->spriteLayer[outX] = renderer->normalPalette[0x100 | tileData | (sprite->palette << 4)] | flags; \
	}

#define SPRITE_DRAW_PIXEL_16_VARIANT(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData && (!(renderer->spriteLayer[outX]) || ((renderer->spriteLayer[outX] & FLAG_ORDER_MASK) > flags))) { \
		renderer->spriteLayer[outX] = renderer->variantPalette[0x100 | tileData | (sprite->palette << 4)] | flags; \
	}

#define SPRITE_DRAW_PIXEL_16_OBJWIN(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

#define SPRITE_XBASE_256(localX) unsigned xBase = (localX & ~0x7) * 8 + (localX & 6);
#define SPRITE_YBASE_256(localY) unsigned yBase = (localY & ~0x7) * (renderer->dispcnt.objCharacterMapping ? width : 0x80) + (localY & 0x7) * 8;

#define SPRITE_DRAW_PIXEL_256_NORMAL(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData && (!(renderer->spriteLayer[outX]) || ((renderer->spriteLayer[outX] & FLAG_ORDER_MASK) > flags))) { \
		renderer->spriteLayer[outX] = renderer->normalPalette[0x100 | tileData] | flags; \
	}

#define SPRITE_DRAW_PIXEL_256_VARIANT(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData && (!(renderer->spriteLayer[outX]) || ((renderer->spriteLayer[outX] & FLAG_ORDER_MASK) > flags))) { \
		renderer->spriteLayer[outX] = renderer->variantPalette[0x100 | tileData] | flags; \
	}

#define SPRITE_DRAW_PIXEL_256_OBJWIN(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

static void _preprocessSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int y) {
	int width = _objSizes[sprite->shape * 8 + sprite->size * 2];
	int height = _objSizes[sprite->shape * 8 + sprite->size * 2 + 1];
	int start = renderer->start;
	int end = renderer->end;
	if ((y < sprite->y && (sprite->y + height - 256 < 0 || y >= sprite->y + height - 256)) || y >= sprite->y + height) {
		return;
	}
	uint32_t flags = (sprite->priority << OFFSET_PRIORITY) | FLAG_FINALIZED;
	flags |= FLAG_TARGET_1 * ((renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || sprite->mode == OBJ_MODE_SEMITRANSPARENT);
	flags |= FLAG_TARGET_2 *renderer->target2Obj;
	flags |= FLAG_OBJWIN * (sprite->mode == OBJ_MODE_OBJWIN);
	int x = sprite->x;
	int inY = y - sprite->y;
	if (sprite->y + height - 256 >= 0) {
		inY += 256;
	}
	if (sprite->vflip) {
		inY = height - inY - 1;
	}
	unsigned charBase = BASE_TILE + sprite->tile * 0x20;
	int variant = renderer->target1Obj && renderer->currentWindow.blendEnable && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	if (!sprite->multipalette) {
		if (flags & FLAG_OBJWIN) {
			SPRITE_NORMAL_LOOP(16, OBJWIN);
		} else if (!variant) {
			SPRITE_NORMAL_LOOP(16, NORMAL);
		} else {
			SPRITE_NORMAL_LOOP(16, VARIANT);
		}
	} else {
		if (flags & FLAG_OBJWIN) {
			SPRITE_NORMAL_LOOP(256, OBJWIN);
		} else if (!variant) {
			SPRITE_NORMAL_LOOP(256, NORMAL);
		} else {
			SPRITE_NORMAL_LOOP(256, VARIANT);
		}
	}
}

static void _preprocessTransformedSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBATransformedObj* sprite, int y) {
	int width = _objSizes[sprite->shape * 8 + sprite->size * 2];
	int totalWidth = width << sprite->doublesize;
	int height = _objSizes[sprite->shape * 8 + sprite->size * 2 + 1];
	int totalHeight = height << sprite->doublesize;
	int start = renderer->start;
	int end = renderer->end;
	if ((y < sprite->y && (sprite->y + totalHeight - 256 < 0 || y >= sprite->y + totalHeight - 256)) || y >= sprite->y + totalHeight) {
		return;
	}
	uint32_t flags = (sprite->priority << OFFSET_PRIORITY) | FLAG_FINALIZED;
	flags |= FLAG_TARGET_1 * ((renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || sprite->mode == OBJ_MODE_SEMITRANSPARENT);
	flags |= FLAG_TARGET_2 * renderer->target2Obj;
	flags |= FLAG_OBJWIN * (sprite->mode == OBJ_MODE_OBJWIN);
	int x = sprite->x;
	unsigned charBase = BASE_TILE + sprite->tile * 0x20;
	struct GBAOAMMatrix* mat = &renderer->d.oam->mat[sprite->matIndex];
	int variant = renderer->target1Obj && renderer->currentWindow.blendEnable && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	int inY = y - sprite->y;
	if (inY < 0) {
		inY += 256;
	}
	if (!sprite->multipalette) {
		if (flags & FLAG_OBJWIN) {
			SPRITE_TRANSFORMED_LOOP(16, OBJWIN);
		} else if (!variant) {
			SPRITE_TRANSFORMED_LOOP(16, NORMAL);
		} else {
			SPRITE_TRANSFORMED_LOOP(16, VARIANT);
		}
	} else {
		if (flags & FLAG_OBJWIN) {
			SPRITE_TRANSFORMED_LOOP(256, OBJWIN);
		} else if (!variant) {
			SPRITE_TRANSFORMED_LOOP(256, NORMAL);
		} else {
			SPRITE_TRANSFORMED_LOOP(256, VARIANT);
		}
	}
}

static void _postprocessSprite(struct GBAVideoSoftwareRenderer* renderer, unsigned priority) {
	int x;
	for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
		uint32_t color = renderer->spriteLayer[x];
		uint32_t current = renderer->row[x];
		if ((color & FLAG_FINALIZED) && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority && !(current & FLAG_FINALIZED)) {
			_composite(renderer, x, color & ~FLAG_FINALIZED, current);
		}
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
}

static inline color_t _brighten(color_t color, int y) {
	color_t c = 0;
	color_t a;
#ifdef COLOR_16_BIT
	a = color & 0x1F;
	c |= (a + ((0x1F - a) * y) / 16) & 0x1F;

	a = color & 0x3E0;
	c |= (a + ((0x3E0 - a) * y) / 16) & 0x3E0;

	a = color & 0x7C00;
	c |= (a + ((0x7C00 - a) * y) / 16) & 0x7C00;
#else
	a = color & 0xF8;
	c |= (a + ((0xF8 - a) * y) / 16) & 0xF8;

	a = color & 0xF800;
	c |= (a + ((0xF800 - a) * y) / 16) & 0xF800;

	a = color & 0xF80000;
	c |= (a + ((0xF80000 - a) * y) / 16) & 0xF80000;
#endif
	return c;
}

static inline color_t _darken(color_t color, int y) {
	color_t c = 0;
	color_t a;
#ifdef COLOR_16_BIT
	a = color & 0x1F;
	c |= (a - (a * y) / 16) & 0x1F;

	a = color & 0x3E0;
	c |= (a - (a * y) / 16) & 0x3E0;

	a = color & 0x7C00;
	c |= (a - (a * y) / 16) & 0x7C00;
#else
	a = color & 0xF8;
	c |= (a - (a * y) / 16) & 0xF8;

	a = color & 0xF800;
	c |= (a - (a * y) / 16) & 0xF800;

	a = color & 0xF80000;
	c |= (a - (a * y) / 16) & 0xF80000;
#endif
	return c;
}

static color_t _mix(int weightA, color_t colorA, int weightB, color_t colorB) {
	color_t c = 0;
	color_t a, b;
#ifdef COLOR_16_BIT
	a = colorA & 0x1F;
	b = colorB & 0x1F;
	c |= ((a * weightA + b * weightB) / 16) & 0x3F;
	if (c & 0x0020) {
		c = 0x001F;
	}

	a = colorA & 0x3E0;
	b = colorB & 0x3E0;
	c |= ((a * weightA + b * weightB) / 16) & 0x7E0;
	if (c & 0x0400) {
		c |= 0x03E0;
	}

	a = colorA & 0x7C00;
	b = colorB & 0x7C00;
	c |= ((a * weightA + b * weightB) / 16) & 0xFC00;
	if (c & 0x8000) {
		c |= 0x7C00;
	}
#else
	a = colorA & 0xF8;
	b = colorB & 0xF8;
	c |= ((a * weightA + b * weightB) / 16) & 0x1F8;
	if (c & 0x00000100) {
		c = 0x000000F8;
	}

	a = colorA & 0xF800;
	b = colorB & 0xF800;
	c |= ((a * weightA + b * weightB) / 16) & 0x1F800;
	if (c & 0x00010000) {
		c = (c & 0x000000F8) | 0x0000F800;
	}

	a = colorA & 0xF80000;
	b = colorB & 0xF80000;
	c |= ((a * weightA + b * weightB) / 16) & 0x1F80000;
	if (c & 0x01000000) {
		c = (c & 0x0000F8F8) | 0x00F80000;
	}
#endif
	return c;
}
