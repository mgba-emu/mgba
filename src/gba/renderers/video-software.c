#include "video-software.h"

#include "gba.h"
#include "gba-io.h"

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

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoSoftwareRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererGetPixels(struct GBAVideoRenderer* renderer, unsigned* stride, void** pixels);
static void GBAVideoSoftwareRendererPutPixels(struct GBAVideoRenderer* renderer, unsigned stride, void* pixels);

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
static void _cleanOAM(struct GBAVideoSoftwareRenderer* renderer);
static int _preprocessTransformedSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBATransformedObj* sprite, int y);
static int _preprocessSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int y);
static void _postprocessSprite(struct GBAVideoSoftwareRenderer* renderer, unsigned priority);

static void _updatePalettes(struct GBAVideoSoftwareRenderer* renderer);
static inline unsigned _brighten(unsigned color, int y);
static inline unsigned _darken(unsigned color, int y);
static unsigned _mix(int weightA, unsigned colorA, int weightB, unsigned colorB);

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBAVideoSoftwareRendererInit;
	renderer->d.reset = GBAVideoSoftwareRendererInit;
	renderer->d.deinit = GBAVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBAVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writeOAM = GBAVideoSoftwareRendererWriteOAM;
	renderer->d.writePalette = GBAVideoSoftwareRendererWritePalette;
	renderer->d.drawScanline = GBAVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = GBAVideoSoftwareRendererFinishFrame;
	renderer->d.getPixels = GBAVideoSoftwareRendererGetPixels;
	renderer->d.putPixels = GBAVideoSoftwareRendererPutPixels;
}

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	int i;

	softwareRenderer->dispcnt = 0x0080;

	softwareRenderer->target1Obj = 0;
	softwareRenderer->target1Bd = 0;
	softwareRenderer->target2Obj = 0;
	softwareRenderer->target2Bd = 0;
	softwareRenderer->blendEffect = BLEND_NONE;
	memset(softwareRenderer->normalPalette, 0, sizeof(softwareRenderer->normalPalette));
	memset(softwareRenderer->variantPalette, 0, sizeof(softwareRenderer->variantPalette));

	softwareRenderer->blda = 0;
	softwareRenderer->bldb = 0;
	softwareRenderer->bldy = 0;

	softwareRenderer->winN[0].h.packed = 0;
	softwareRenderer->winN[0].v.packed = 0;
	softwareRenderer->winN[0].control.packed = 0;
	softwareRenderer->winN[0].control.priority = 0;
	softwareRenderer->winN[1].h.packed = 0;
	softwareRenderer->winN[1].v.packed = 0;
	softwareRenderer->winN[1].control.packed = 0;
	softwareRenderer->winN[1].control.priority = 1;
	softwareRenderer->objwin.packed = 0;
	softwareRenderer->objwin.priority = 2;
	softwareRenderer->winout.packed = 0;
	softwareRenderer->winout.priority = 3;
	softwareRenderer->oamMax = 0;

	softwareRenderer->mosaic.packed = 0;

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
	UNUSED(softwareRenderer);
}

static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	switch (address) {
	case REG_DISPCNT:
		softwareRenderer->dispcnt = value;
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
		if (softwareRenderer->winN[0].h.start > VIDEO_HORIZONTAL_PIXELS && softwareRenderer->winN[0].h.start > softwareRenderer->winN[0].h.end) {
			softwareRenderer->winN[0].h.start = 0;
		}
		if (softwareRenderer->winN[0].h.start > softwareRenderer->winN[0].h.end || softwareRenderer->winN[0].h.end > VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[0].h.end = VIDEO_HORIZONTAL_PIXELS;
		}
		break;
	case REG_WIN1H:
		softwareRenderer->winN[1].h.packed = value;
		if (softwareRenderer->winN[1].h.start > VIDEO_HORIZONTAL_PIXELS && softwareRenderer->winN[1].h.start > softwareRenderer->winN[1].h.end) {
			softwareRenderer->winN[1].h.start = 0;
		}
		if (softwareRenderer->winN[1].h.start > softwareRenderer->winN[1].h.end || softwareRenderer->winN[1].h.end > VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[1].h.end = VIDEO_HORIZONTAL_PIXELS;
		}
		break;
	case REG_WIN0V:
		softwareRenderer->winN[0].v.packed = value;
		if (softwareRenderer->winN[0].v.start > VIDEO_VERTICAL_PIXELS && softwareRenderer->winN[0].v.start > softwareRenderer->winN[0].v.end) {
			softwareRenderer->winN[0].v.start = 0;
		}
		if (softwareRenderer->winN[0].v.start > softwareRenderer->winN[0].v.end || softwareRenderer->winN[0].v.end > VIDEO_HORIZONTAL_PIXELS) {
			softwareRenderer->winN[0].v.end = VIDEO_VERTICAL_PIXELS;
		}
		break;
	case REG_WIN1V:
		softwareRenderer->winN[1].v.packed = value;
		if (softwareRenderer->winN[1].v.start > VIDEO_VERTICAL_PIXELS && softwareRenderer->winN[1].v.start > softwareRenderer->winN[1].v.end) {
			softwareRenderer->winN[1].v.start = 0;
		}
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
		softwareRenderer->mosaic.packed = value;
		break;
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
	softwareRenderer->oamDirty = 1;
	UNUSED(oam);
}

static void GBAVideoSoftwareRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
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

static void _cleanOAM(struct GBAVideoSoftwareRenderer* renderer) {
	int i;
	int oamMax = 0;
	for (i = 0; i < 128; ++i) {
		struct GBAObj* obj = &renderer->d.oam->obj[i];
		if (obj->transformed || !obj->disable) {
			int height = _objSizes[obj->shape * 8 + obj->size * 2 + 1];
			if (obj->transformed) {
				height <<= ((struct GBATransformedObj*) obj)->doublesize;
			}
			if (obj->y < VIDEO_VERTICAL_PIXELS || obj->y + height >= VIDEO_VERTICAL_TOTAL_PIXELS) {
				renderer->sprites[oamMax].y = obj->y;
				renderer->sprites[oamMax].endY = obj->y + height;
				renderer->sprites[oamMax].obj = *obj;
				++oamMax;
			}
		}
	}
	renderer->oamMax = oamMax;
	renderer->oamDirty = 0;
}


static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
	if (GBARegisterDISPCNTIsForcedBlank(softwareRenderer->dispcnt)) {
		int x;
		for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GBA_COLOR_WHITE;
		}
		return;
	}

	int x;
	for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; x += 4) {
		softwareRenderer->spriteLayer[x] = FLAG_UNWRITTEN;
		softwareRenderer->spriteLayer[x + 1] = FLAG_UNWRITTEN;
		softwareRenderer->spriteLayer[x + 2] = FLAG_UNWRITTEN;
		softwareRenderer->spriteLayer[x + 3] = FLAG_UNWRITTEN;
	}

	softwareRenderer->windows[0].endX = VIDEO_HORIZONTAL_PIXELS;
	softwareRenderer->nWindows = 1;
	if (GBARegisterDISPCNTIsWin0Enable(softwareRenderer->dispcnt) || GBARegisterDISPCNTIsWin1Enable(softwareRenderer->dispcnt) || GBARegisterDISPCNTIsObjwinEnable(softwareRenderer->dispcnt)) {
		softwareRenderer->windows[0].control = softwareRenderer->winout;
		if (GBARegisterDISPCNTIsWin1Enable(softwareRenderer->dispcnt) && y < softwareRenderer->winN[1].v.end && y >= softwareRenderer->winN[1].v.start) {
			_breakWindow(softwareRenderer, &softwareRenderer->winN[1]);
		}
		if (GBARegisterDISPCNTIsWin0Enable(softwareRenderer->dispcnt) && y < softwareRenderer->winN[0].v.end && y >= softwareRenderer->winN[0].v.start) {
			_breakWindow(softwareRenderer, &softwareRenderer->winN[0]);
		}
	} else {
		softwareRenderer->windows[0].control.packed = 0xFF;
	}

	int w;
	x = 0;
	for (w = 0; w < softwareRenderer->nWindows; ++w) {
		// TOOD: handle objwin on backdrop
		uint32_t backdrop = FLAG_UNWRITTEN | FLAG_PRIORITY | FLAG_IS_BACKGROUND;
		if (!softwareRenderer->target1Bd || softwareRenderer->blendEffect == BLEND_NONE || softwareRenderer->blendEffect == BLEND_ALPHA || !softwareRenderer->windows[w].control.blendEnable) {
			backdrop |= softwareRenderer->normalPalette[0];
		} else {
			backdrop |= softwareRenderer->variantPalette[0];
		}
		int end = softwareRenderer->windows[w].endX;
		for (; x < end; ++x) {
			softwareRenderer->row[x] = backdrop;
		}
	}

	_drawScanline(softwareRenderer, y);

	if (softwareRenderer->target2Bd) {
		x = 0;
		for (w = 0; w < softwareRenderer->nWindows; ++w) {
		uint32_t backdrop = FLAG_UNWRITTEN;
			if (!softwareRenderer->target1Bd || softwareRenderer->blendEffect == BLEND_NONE || softwareRenderer->blendEffect == BLEND_ALPHA || !softwareRenderer->windows[w].control.blendEnable) {
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

#ifdef COLOR_16_BIT
#ifdef __arm__
	_to16Bit(row, softwareRenderer->row, VIDEO_HORIZONTAL_PIXELS);
#else
	for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
		row[x] = softwareRenderer->row[x];
	}
#endif
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

static void GBAVideoSoftwareRendererGetPixels(struct GBAVideoRenderer* renderer, unsigned* stride, void** pixels) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	*stride = softwareRenderer->outputBufferStride;
	*pixels = softwareRenderer->outputBuffer;
}

static void GBAVideoSoftwareRendererPutPixels(struct GBAVideoRenderer* renderer, unsigned stride, void* pixels) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	uint32_t* colorPixels = pixels;
	unsigned i;
	for (i = 0; i < VIDEO_VERTICAL_PIXELS; ++i) {
		memmove(&softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * i], &colorPixels[stride * i], VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL);
	}
}

static void GBAVideoSoftwareRendererUpdateDISPCNT(struct GBAVideoSoftwareRenderer* renderer) {
	renderer->bg[0].enabled = GBARegisterDISPCNTGetBg0Enable(renderer->dispcnt);
	renderer->bg[1].enabled = GBARegisterDISPCNTGetBg1Enable(renderer->dispcnt);
	renderer->bg[2].enabled = GBARegisterDISPCNTGetBg2Enable(renderer->dispcnt);
	renderer->bg[3].enabled = GBARegisterDISPCNTGetBg3Enable(renderer->dispcnt);
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

	renderer->anyTarget2 = bldcnt.packed & 0x3F00;

	if (oldEffect != renderer->blendEffect) {
		_updatePalettes(renderer);
	}
}

#define TEST_LAYER_ENABLED(X) \
	(renderer->bg[X].enabled && \
	(renderer->currentWindow.bg ## X ## Enable || \
	(GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt) && renderer->objwin.bg ## X ## Enable)) && \
	renderer->bg[X].priority == priority)

static void _drawScanline(struct GBAVideoSoftwareRenderer* renderer, int y) {
	int w;
	renderer->end = 0;
	int spriteLayers = 0;
	if (GBARegisterDISPCNTIsObjEnable(renderer->dispcnt)) {
		if (renderer->oamDirty) {
			_cleanOAM(renderer);
		}
		int mosaicV = renderer->mosaic.objV + 1;
		int mosaicY = y - (y % mosaicV);
		for (w = 0; w < renderer->nWindows; ++w) {
			renderer->start = renderer->end;
			renderer->end = renderer->windows[w].endX;
			renderer->currentWindow = renderer->windows[w].control;
			if (!renderer->currentWindow.objEnable) {
				continue;
			}
			int i;
			int drawn;
			for (i = 0; i < renderer->oamMax; ++i) {
				int localY = y;
				struct GBAVideoSoftwareSprite* sprite = &renderer->sprites[i];
				if (sprite->obj.mosaic) {
					localY = mosaicY;
				}
				if ((localY < sprite->y && (sprite->endY - 256 < 0 || localY >= sprite->endY - 256)) || localY >= sprite->endY) {
					continue;
				}
				if (sprite->obj.transformed) {
					drawn = _preprocessTransformedSprite(renderer, &sprite->tobj, localY);
				} else {
					drawn = _preprocessSprite(renderer, &sprite->obj, localY);
				}
				spriteLayers |= drawn << sprite->obj.priority;
			}
		}
	}

	int priority;
	for (priority = 0; priority < 4; ++priority) {
		if (spriteLayers & (1 << priority)) {
			_postprocessSprite(renderer, priority);
		}
		renderer->end = 0;
		for (w = 0; w < renderer->nWindows; ++w) {
			renderer->start = renderer->end;
			renderer->end = renderer->windows[w].endX;
			renderer->currentWindow = renderer->windows[w].control;
			if (TEST_LAYER_ENABLED(0) && GBARegisterDISPCNTGetMode(renderer->dispcnt) < 2) {
				_drawBackgroundMode0(renderer, &renderer->bg[0], y);
			}
			if (TEST_LAYER_ENABLED(1) && GBARegisterDISPCNTGetMode(renderer->dispcnt) < 2) {
				_drawBackgroundMode0(renderer, &renderer->bg[1], y);
			}
			if (TEST_LAYER_ENABLED(2)) {
				switch (GBARegisterDISPCNTGetMode(renderer->dispcnt)) {
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
			}
			if (TEST_LAYER_ENABLED(3)) {
				switch (GBARegisterDISPCNTGetMode(renderer->dispcnt)) {
				case 0:
					_drawBackgroundMode0(renderer, &renderer->bg[3], y);
					break;
				case 2:
					_drawBackgroundMode2(renderer, &renderer->bg[3], y);
					break;
				}
			}
		}
	}
	renderer->bg[2].sx += renderer->bg[2].dmx;
	renderer->bg[2].sy += renderer->bg[2].dmy;
	renderer->bg[3].sx += renderer->bg[3].dmx;
	renderer->bg[3].sy += renderer->bg[3].dmy;
}

// We stash the priority on the top bits so we can do a one-operator comparison
// The lower the number, the higher the priority, and sprites take precendence over backgrounds
// We want to do special processing if the color pixel is target 1, however

static inline void _compositeBlendObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color, uint32_t current) {
	if (color >= current) {
		if (current & FLAG_TARGET_1 && color & FLAG_TARGET_2) {
			color = _mix(renderer->blda, current, renderer->bldb, color);
		} else {
			color = current & 0x00FFFFFF;
		}
	} else {
		color = (color & ~FLAG_TARGET_2) | (current & FLAG_OBJWIN);
	}
	*pixel = color;
}

static inline void _compositeBlendNoObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color, uint32_t current) {
	// We stash the priority on the top bits so we can do a one-operator comparison
	// The lower the number, the higher the priority, and sprites take precendence over backgrounds
	// We want to do special processing if the color pixel is target 1, however
	if (color >= current) {
		if (current & FLAG_TARGET_1 && color & FLAG_TARGET_2) {
			color = _mix(renderer->blda, current, renderer->bldb, color);
		} else {
			color = current & 0x00FFFFFF;
		}
	} else {
		color = color & ~FLAG_TARGET_2;
	}
	*pixel = color;
}

static inline void _compositeNoBlendObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color, uint32_t current) {
	UNUSED(renderer);
	if (color < current) {
		*pixel = color | (current & FLAG_OBJWIN);
	}
}

static inline void _compositeNoBlendNoObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color, uint32_t current) {
	UNUSED(renderer);
	if (color < current) {
		*pixel = color;
	}
}

#define COMPOSITE_16_OBJWIN(BLEND) \
	if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) { \
		unsigned color = (current & FLAG_OBJWIN) ? objwinPalette[paletteData | pixelData] : palette[pixelData]; \
		_composite ## BLEND ## Objwin(renderer, pixel, color | flags, current); \
	}

#define COMPOSITE_16_NO_OBJWIN(BLEND) \
	_composite ## BLEND ## NoObjwin(renderer, pixel, palette[pixelData] | flags, current);

#define COMPOSITE_256_OBJWIN(BLEND) \
	if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) { \
		unsigned color = (current & FLAG_OBJWIN) ? objwinPalette[pixelData] : palette[pixelData]; \
		_composite ## BLEND ## Objwin(renderer, pixel, color | flags, current); \
	}

#define COMPOSITE_256_NO_OBJWIN(BLEND) \
	COMPOSITE_16_NO_OBJWIN(BLEND)

#define BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN) \
	pixelData = tileData & 0xF; \
	current = *pixel; \
	if (pixelData && IS_WRITABLE(current)) { \
		COMPOSITE_16_ ## OBJWIN (BLEND); \
	} \
	tileData >>= 4;

#define BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN) \
	pixelData = tileData & 0xFF; \
	current = *pixel; \
	if (pixelData && IS_WRITABLE(current)) { \
		COMPOSITE_256_ ## OBJWIN (BLEND); \
	} \
	tileData >>= 8;

#define BACKGROUND_TEXT_SELECT_CHARACTER \
	localX = tileX * 8 + inX; \
	xBase = localX & 0xF8; \
	if (background->size & 1) { \
		xBase += (localX & 0x100) << 5; \
	} \
	screenBase = yBase + (xBase >> 3); \
	LOAD_16(mapData, screenBase << 1, vram); \
	localY = inY & 0x7; \
	if (GBA_TEXT_MAP_VFLIP(mapData)) { \
		localY = 7 - localY; \
	}

#define PREPARE_OBJWIN \
	int objwinSlowPath = GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt); \
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

#define DRAW_BACKGROUND_MODE_0_TILE_SUFFIX_16(BLEND, OBJWIN) \
	paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
	palette = &mainPalette[paletteData]; \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
	LOAD_32(tileData, charBase, vram); \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		tileData >>= 4 * mod8; \
		for (; outX < end; ++outX) { \
			uint32_t* pixel = &renderer->row[outX]; \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
		} \
	} else { \
		for (outX = end - 1; outX >= renderer->start; --outX) { \
			uint32_t* pixel = &renderer->row[outX]; \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_PREFIX_16(BLEND, OBJWIN) \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
	LOAD_32(tileData, charBase, vram); \
	paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
	palette = &mainPalette[paletteData]; \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		outX = renderer->end - mod8; \
		if (outX < renderer->start) { \
			tileData >>= 4 * (renderer->start - outX); \
			outX = renderer->start; \
		} \
		for (; outX < renderer->end; ++outX) { \
			uint32_t* pixel = &renderer->row[outX]; \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
		} \
	} else { \
		tileData >>= 4 * (0x8 - mod8); \
		int end2 = renderer->end - 8; \
		if (end2 < -1) { \
			end2 = -1; \
		} \
		for (outX = renderer->end - 1; outX > end2; --outX) { \
			uint32_t* pixel = &renderer->row[outX]; \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_MOSAIC_16(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
		tileData = carryData; \
		for (x = 0; x < 8; ++x) { \
			if (!mosaicWait) { \
				paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
				palette = &mainPalette[paletteData]; \
				LOAD_32(tileData, charBase, vram); \
				if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
					tileData >>= x * 4; \
				} else { \
					tileData >>= (7 - x) * 4; \
				} \
				tileData &= 0xF; \
				tileData |= tileData << 4; \
				tileData |= tileData << 8; \
				tileData |= tileData << 12; \
				tileData |= tileData << 16; \
				tileData |= tileData << 20; \
				tileData |= tileData << 24; \
				tileData |= tileData << 28; \
				carryData = tileData; \
				mosaicWait = mosaicH; \
			} \
			--mosaicWait; \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
			++pixel; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILES_16(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
		palette = &mainPalette[paletteData]; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
		LOAD_32(tileData, charBase, vram); \
		if (tileData) { \
			if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				++pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				++pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				++pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				++pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				++pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				++pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				++pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				++pixel; \
			} else { \
				pixel += 7; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN); \
				pixel += 8; \
			} \
		} else { \
			pixel += 8; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_SUFFIX_256(BLEND, OBJWIN) \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
	int end2 = end - 4; \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		int shift = inX & 0x3; \
		if (end2 > outX) { \
			LOAD_32(tileData, charBase, vram); \
			tileData >>= 8 * shift; \
			shift = 0; \
			for (; outX < end2; ++outX) { \
				pixel = &renderer->row[outX]; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
			} \
		} \
		\
		LOAD_32(tileData, charBase + 4, vram); \
		tileData >>= 8 * shift; \
		for (; outX < end; ++outX) { \
			pixel = &renderer->row[outX]; \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
		} \
	} else { \
		int start = outX; \
		outX = end - 1; \
		if (end2 > start) { \
			LOAD_32(tileData, charBase, vram); \
			for (; outX >= end2; --outX) { \
				pixel = &renderer->row[outX]; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
			} \
			charBase += 4; \
		} \
		\
		LOAD_32(tileData, charBase, vram); \
		for (; outX >= renderer->start; --outX) { \
			pixel = &renderer->row[outX]; \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
		} \
		outX = end; \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_PREFIX_256(BLEND, OBJWIN) \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
	outX = renderer->end - 8 + end; \
	int end2 = 4 - end; \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		if (end2 > 0) { \
			LOAD_32(tileData, charBase, vram); \
			for (; outX < renderer->end - end2; ++outX) { \
				pixel = &renderer->row[outX]; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
			} \
			charBase += 4; \
		} \
		\
		LOAD_32(tileData, charBase, vram); \
		for (; outX < renderer->end; ++outX) { \
			pixel = &renderer->row[outX]; \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
		} \
	} else { \
		int shift = end & 0x3; \
		int start = outX; \
		outX = renderer->end - 1; \
		if (end2 > 0) { \
			LOAD_32(tileData, charBase, vram); \
			tileData >>= 8 * shift; \
			for (; outX >= start + 4; --outX) { \
				pixel = &renderer->row[outX]; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
			} \
			shift = 0; \
		} \
		\
		LOAD_32(tileData, charBase + 4, vram); \
		tileData >>= 8 * shift; \
		for (; outX >= start; --outX) { \
			pixel = &renderer->row[outX]; \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILES_256(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
		if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
			LOAD_32(tileData, charBase, vram); \
			if (tileData) { \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
					++pixel; \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
					++pixel; \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
					++pixel; \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
					++pixel; \
			} else { \
				pixel += 4; \
			} \
			LOAD_32(tileData, charBase + 4, vram); \
			if (tileData) { \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
					++pixel; \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
					++pixel; \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
					++pixel; \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
					++pixel; \
			} else { \
				pixel += 4; \
			} \
		} else { \
			LOAD_32(tileData, charBase + 4, vram); \
			if (tileData) { \
				pixel += 3; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
			} \
			pixel += 4; \
			LOAD_32(tileData, charBase, vram); \
			if (tileData) { \
				pixel += 3; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
				--pixel; \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
			} \
			pixel += 4; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_MOSAIC_256(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
		tileData = carryData; \
		for (x = 0; x < 8; ++x) { \
			if (!mosaicWait) { \
				if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
					if (x >= 4) { \
						LOAD_32(tileData, charBase + 4, vram); \
						tileData >>= (x - 4) * 8; \
					} else { \
						LOAD_32(tileData, charBase, vram); \
						tileData >>= x * 8; \
					} \
				} else { \
					if (x >= 4) { \
						LOAD_32(tileData, charBase, vram); \
						tileData >>= (7 - x) * 8; \
					} else { \
						LOAD_32(tileData, charBase + 4, vram); \
						tileData >>= (3 - x) * 8; \
					} \
				} \
				tileData &= 0xFF; \
				carryData = tileData; \
				mosaicWait = mosaicH; \
			} \
			tileData |= tileData << 8; \
			--mosaicWait; \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN); \
			++pixel; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0(BPP, BLEND, OBJWIN) \
	uint32_t* pixel = &renderer->row[outX]; \
	if (background->mosaic && renderer->mosaic.bgH) { \
		int mosaicH = renderer->mosaic.bgH + 1; \
		int x; \
		int mosaicWait = outX % mosaicH; \
		int carryData = 0; \
		paletteData = 0; /* Quiets compiler warning */ \
		DRAW_BACKGROUND_MODE_0_MOSAIC_ ## BPP (BLEND, OBJWIN) \
		return; \
	} \
	\
	if (inX & 0x7) { \
		int mod8 = inX & 0x7; \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		\
		int end = outX + 0x8 - mod8; \
		if (end > renderer->end) { \
			/* TODO: ensure tiles are properly aligned from this*/ \
			end = renderer->end; \
		} \
		if (end == outX) { \
			return; \
		} \
		DRAW_BACKGROUND_MODE_0_TILE_SUFFIX_ ## BPP (BLEND, OBJWIN) \
	} \
	if (inX & 0x7 || (renderer->end - renderer->start) & 0x7) { \
		tileX = tileEnd; \
		int pixelData; \
		int mod8 = (inX + renderer->end - renderer->start) & 0x7; \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		\
		int end = 0x8 - mod8; \
		UNUSED(end); \
		DRAW_BACKGROUND_MODE_0_TILE_PREFIX_ ## BPP (BLEND, OBJWIN) \
		\
		tileX = (inX & 0x7) != 0; \
		outX = renderer->start + tileX * 8 - (inX & 0x7); \
	} \
	\
	pixel = &renderer->row[outX]; \
	DRAW_BACKGROUND_MODE_0_TILES_ ## BPP (BLEND, OBJWIN)

static void _drawBackgroundMode0(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y) {
	int inX = renderer->start + background->x;
	if (background->mosaic) {
		int mosaicV = renderer->mosaic.bgV + 1;
		y -= y % mosaicV;
	}
	int inY = y + background->y;
	uint16_t mapData;

	unsigned yBase = inY & 0xF8;
	if (background->size == 2) {
		yBase += inY & 0x100;
	} else if (background->size == 3) {
		yBase += (inY & 0x100) << 1;
	}
	yBase = (background->screenBase >> 1) + (yBase << 2);

	int localX;
	int localY;

	unsigned xBase;

	int flags = (background->priority << OFFSET_PRIORITY) | (background->index << OFFSET_INDEX) | FLAG_IS_BACKGROUND;
	flags |= FLAG_TARGET_1 * (background->target1 && renderer->blendEffect == BLEND_ALPHA);
	flags |= FLAG_TARGET_2 * background->target2;

	uint32_t screenBase;
	uint32_t charBase;
	int variant = background->target1 && renderer->currentWindow.blendEnable && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	color_t* mainPalette = renderer->normalPalette;
	if (variant) {
		mainPalette = renderer->variantPalette;
	}
	color_t* palette = mainPalette;
	PREPARE_OBJWIN;

	int outX = renderer->start;

	uint32_t tileData;
	uint32_t current;
	int pixelData;
	int paletteData;
	int tileX = 0;
	int tileEnd = (renderer->end - renderer->start + (inX & 0x7)) >> 3;
	uint16_t* vram = renderer->d.vram;

	if (!objwinSlowPath) {
		if (!(flags & FLAG_TARGET_2)) {
			if (!background->multipalette) {
				DRAW_BACKGROUND_MODE_0(16, NoBlend, NO_OBJWIN);
			} else {
				DRAW_BACKGROUND_MODE_0(256, NoBlend, NO_OBJWIN);
			}
		} else {
			if (!background->multipalette) {
				DRAW_BACKGROUND_MODE_0(16, Blend, NO_OBJWIN);
			} else {
				DRAW_BACKGROUND_MODE_0(256, Blend, NO_OBJWIN);
			}
		}
	} else {
		if (!(flags & FLAG_TARGET_2)) {
			if (!background->multipalette) {
				DRAW_BACKGROUND_MODE_0(16, NoBlend, OBJWIN);
			} else {
				DRAW_BACKGROUND_MODE_0(256, NoBlend, OBJWIN);
			}
		} else {
			if (!background->multipalette) {
				DRAW_BACKGROUND_MODE_0(16, Blend, OBJWIN);
			} else {
				DRAW_BACKGROUND_MODE_0(256, Blend, OBJWIN);
			}
		}
	}
}

#define BACKGROUND_BITMAP_INIT \
	UNUSED(unused); \
	int32_t x = background->sx + (renderer->start - 1) * background->dx; \
	int32_t y = background->sy + (renderer->start - 1) * background->dy; \
	int32_t localX; \
	int32_t localY; \
	\
	int flags = (background->priority << OFFSET_PRIORITY) | (background->index << OFFSET_INDEX) | FLAG_IS_BACKGROUND; \
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
	uint32_t* pixel;
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) {
		x += background->dx;
		y += background->dy;

		if (background->overflow) {
			localX = x & (sizeAdjusted - 1);
			localY = y & (sizeAdjusted - 1);
		} else if ((x | y) & ~(sizeAdjusted - 1)) {
			continue;
		} else {
			localX = x;
			localY = y;
		}
		mapData = ((uint8_t*)renderer->d.vram)[screenBase + (localX >> 11) + (((localY >> 7) & 0x7F0) << background->size)];
		tileData = ((uint8_t*)renderer->d.vram)[charBase + (mapData << 6) + ((localY & 0x700) >> 5) + ((localX & 0x700) >> 8)];

		uint32_t current = *pixel;
		if (tileData && IS_WRITABLE(current)) {
			if (!objwinSlowPath) {
				_compositeBlendNoObjwin(renderer, pixel, palette[tileData] | flags, current);
			} else if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) {
				color_t* currentPalette = (current & FLAG_OBJWIN) ? objwinPalette : palette;
				_compositeBlendObjwin(renderer, pixel, currentPalette[tileData] | flags, current);
			}
		}
	}
}

static void _drawBackgroundMode3(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int unused) {
	BACKGROUND_BITMAP_INIT;

	uint32_t color;

	int outX;
	uint32_t* pixel;
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) {
		BACKGROUND_BITMAP_ITERATE(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);

		LOAD_16(color, (localX >> 8) + (localY >> 8) * VIDEO_HORIZONTAL_PIXELS << 1, renderer->d.vram);
#ifndef COLOR_16_BIT
		unsigned color32;
		color32 = 0;
		color32 |= (color << 3) & 0xF8;
		color32 |= (color << 6) & 0xF800;
		color32 |= (color << 9) & 0xF80000;
		color = color32;
#endif

		uint32_t current = *pixel;
		if (!objwinSlowPath || !(current & FLAG_OBJWIN) != objwinOnly) {
			if (!variant) {
				_compositeBlendObjwin(renderer, pixel, color | flags, current);
			} else if (renderer->blendEffect == BLEND_BRIGHTEN) {
				_compositeBlendObjwin(renderer, pixel, _brighten(color, renderer->bldy) | flags, current);
			} else if (renderer->blendEffect == BLEND_DARKEN) {
				_compositeBlendObjwin(renderer, pixel, _darken(color, renderer->bldy) | flags, current);
			}
		}
	}
}

static void _drawBackgroundMode4(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int unused) {
	BACKGROUND_BITMAP_INIT;

	uint16_t color;
	uint32_t offset = 0;
	if (GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt)) {
		offset = 0xA000;
	}

	int outX;
	uint32_t* pixel;
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) {
		BACKGROUND_BITMAP_ITERATE(VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);

		color = ((uint8_t*)renderer->d.vram)[offset + (localX >> 8) + (localY >> 8) * VIDEO_HORIZONTAL_PIXELS];

		uint32_t current = *pixel;
		if (color && IS_WRITABLE(current)) {
			if (!objwinSlowPath) {
				_compositeBlendNoObjwin(renderer, pixel, palette[color] | flags, current);
			} else if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) {
				color_t* currentPalette = (current & FLAG_OBJWIN) ? objwinPalette : palette;
				_compositeBlendObjwin(renderer, pixel, currentPalette[color] | flags, current);
			}
		}
	}
}

static void _drawBackgroundMode5(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int unused) {
	BACKGROUND_BITMAP_INIT;

	uint32_t color;
	uint32_t offset = 0;
	if (GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt)) {
		offset = 0xA000;
	}

	int outX;
	uint32_t* pixel;
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) {
		BACKGROUND_BITMAP_ITERATE(160, 128);

		LOAD_16(color, offset + (localX >> 8) + (localY >> 8) * 320, renderer->d.vram);
#ifndef COLOR_16_BIT
		unsigned color32 = 0;
		color32 |= (color << 9) & 0xF80000;
		color32 |= (color << 3) & 0xF8;
		color32 |= (color << 6) & 0xF800;
		color = color32;
#endif

		uint32_t current = *pixel;
		if (!objwinSlowPath || !(current & FLAG_OBJWIN) != objwinOnly) {
			if (!variant) {
				_compositeBlendObjwin(renderer, pixel, color | flags, current);
			} else if (renderer->blendEffect == BLEND_BRIGHTEN) {
				_compositeBlendObjwin(renderer, pixel, _brighten(color, renderer->bldy) | flags, current);
			} else if (renderer->blendEffect == BLEND_DARKEN) {
				_compositeBlendObjwin(renderer, pixel, _darken(color, renderer->bldy) | flags, current);
			}
		}
	}
}

#define SPRITE_NORMAL_LOOP(DEPTH, TYPE) \
	SPRITE_YBASE_ ## DEPTH(inY); \
	for (; outX < condition; ++outX, inX += xOffset) { \
		if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
			continue; \
		} \
		SPRITE_XBASE_ ## DEPTH(inX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(inX); \
	}

#define SPRITE_MOSAIC_LOOP(DEPTH, TYPE) \
	SPRITE_YBASE_ ## DEPTH(inY); \
	if (outX % mosaicH) { \
		inX += (mosaicH - (outX % mosaicH)) * xOffset; \
		outX += mosaicH - (outX % mosaicH); \
	} \
	for (; outX < condition; ++outX, inX += xOffset) { \
		if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
			continue; \
		} \
		int localX = inX - xOffset * (outX % mosaicH); \
		SPRITE_XBASE_ ## DEPTH(localX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(localX); \
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
#define SPRITE_YBASE_16(localY) unsigned yBase = (localY & ~0x7) * (GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? width >> 1 : 0x80) + (localY & 0x7) * 4;

#define SPRITE_DRAW_PIXEL_16_NORMAL(localX) \
	unsigned tileData = vramBase[((yBase + charBase + xBase) & 0x7FFF) >> 1]; \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData && (renderer->spriteLayer[outX] & FLAG_ORDER_MASK) > flags) { \
		renderer->spriteLayer[outX] = palette[tileData] | flags; \
	}

#define SPRITE_DRAW_PIXEL_16_OBJWIN(localX) \
	unsigned tileData = vramBase[((yBase + charBase + xBase) & 0x7FFF) >> 1]; \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

#define SPRITE_XBASE_256(localX) unsigned xBase = (localX & ~0x7) * 8 + (localX & 6);
#define SPRITE_YBASE_256(localY) unsigned yBase = (localY & ~0x7) * (GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? width : 0x80) + (localY & 0x7) * 8;

#define SPRITE_DRAW_PIXEL_256_NORMAL(localX) \
	unsigned tileData = vramBase[((yBase + charBase + xBase) & 0x7FFF) >> 1]; \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData && (renderer->spriteLayer[outX] & FLAG_ORDER_MASK) > flags) { \
		renderer->spriteLayer[outX] = palette[tileData] | flags; \
	}

#define SPRITE_DRAW_PIXEL_256_OBJWIN(localX) \
	unsigned tileData = vramBase[((yBase + charBase + xBase) & 0x7FFF) >> 1]; \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

static int _preprocessSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int y) {
	int width = _objSizes[sprite->shape * 8 + sprite->size * 2];
	int height = _objSizes[sprite->shape * 8 + sprite->size * 2 + 1];
	int start = renderer->start;
	int end = renderer->end;
	uint32_t flags = sprite->priority << OFFSET_PRIORITY;
	flags |= FLAG_TARGET_1 * ((renderer->currentWindow.blendEnable && renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || sprite->mode == OBJ_MODE_SEMITRANSPARENT);
	flags |= FLAG_OBJWIN * (sprite->mode == OBJ_MODE_OBJWIN);
	int x = sprite->x;
	uint16_t* vramBase = &renderer->d.vram[BASE_TILE >> 1];
	unsigned charBase = sprite->tile * 0x20;
	int variant = renderer->target1Obj && renderer->currentWindow.blendEnable && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	if (sprite->mode == OBJ_MODE_SEMITRANSPARENT && renderer->target2Bd) {
		// Hack: if a sprite is blended, then the variant palette is not used, but we don't know if it's blended in advance
		variant = 0;
	}
	color_t* palette = &renderer->normalPalette[0x100];
	if (variant) {
		palette = &renderer->variantPalette[0x100];
	}

	int outX = x >= start ? x : start;
	int condition = x + width;
	int mosaicH = 1;
	if (sprite->mosaic) {
		mosaicH = renderer->mosaic.objH + 1;
		if (condition % mosaicH) {
			condition += mosaicH - (condition % mosaicH);
		}
	}
	int inY = y - sprite->y;
	if (sprite->y + height - 256 >= 0) {
		inY += 256;
	}
	if (sprite->vflip) {
		inY = height - inY - 1;
	}
	if (end < condition) {
		condition = end;
	}
	int inX = outX - x;
	int xOffset = 1;
	if (sprite->hflip) {
		inX = width - inX - 1;
		xOffset = -1;
	}
	if (!sprite->multipalette) {
		palette = &palette[sprite->palette << 4];
		if (flags & FLAG_OBJWIN) {
			SPRITE_NORMAL_LOOP(16, OBJWIN);
		} else if (sprite->mosaic) {
			SPRITE_MOSAIC_LOOP(16, NORMAL);
		} else {
			SPRITE_NORMAL_LOOP(16, NORMAL);
		}
	} else {
		if (flags & FLAG_OBJWIN) {
			SPRITE_NORMAL_LOOP(256, OBJWIN);
		} else if (sprite->mosaic) {
			SPRITE_MOSAIC_LOOP(256, NORMAL);
		} else {
			SPRITE_NORMAL_LOOP(256, NORMAL);
		}
	}
	return 1;
}

static int _preprocessTransformedSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBATransformedObj* sprite, int y) {
	int width = _objSizes[sprite->shape * 8 + sprite->size * 2];
	int height = _objSizes[sprite->shape * 8 + sprite->size * 2 + 1];
	int totalWidth = width << sprite->doublesize;
	int totalHeight = height << sprite->doublesize;
	int start = renderer->start;
	int end = renderer->end;
	uint32_t flags = sprite->priority << OFFSET_PRIORITY;
	flags |= FLAG_TARGET_1 * ((renderer->currentWindow.blendEnable && renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || sprite->mode == OBJ_MODE_SEMITRANSPARENT);
	flags |= FLAG_OBJWIN * (sprite->mode == OBJ_MODE_OBJWIN);
	int x = sprite->x;
	uint16_t* vramBase = &renderer->d.vram[BASE_TILE >> 1];
	unsigned charBase = sprite->tile * 0x20;
	struct GBAOAMMatrix* mat = &renderer->d.oam->mat[sprite->matIndex];
	int variant = renderer->target1Obj && renderer->currentWindow.blendEnable && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	if (sprite->mode == OBJ_MODE_SEMITRANSPARENT && renderer->target2Bd) {
		// Hack: if a sprite is blended, then the variant palette is not used, but we don't know if it's blended in advance
		variant = 0;
	}
	color_t* palette = &renderer->normalPalette[0x100];
	if (variant) {
		palette = &renderer->variantPalette[0x100];
	}
	int inY = y - sprite->y;
	if (inY < 0) {
		inY += 256;
	}
	if (!sprite->multipalette) {
		palette = &palette[sprite->palette << 4];
		if (flags & FLAG_OBJWIN) {
			SPRITE_TRANSFORMED_LOOP(16, OBJWIN);
		} else {
			SPRITE_TRANSFORMED_LOOP(16, NORMAL);
		}
	} else {
		if (flags & FLAG_OBJWIN) {
			SPRITE_TRANSFORMED_LOOP(256, OBJWIN);
		} else {
			SPRITE_TRANSFORMED_LOOP(256, NORMAL);
		}
	}
	return 1;
}

static void _postprocessSprite(struct GBAVideoSoftwareRenderer* renderer, unsigned priority) {
	int x;
	uint32_t* pixel = renderer->row;
	uint32_t flags = FLAG_TARGET_2 * renderer->target2Obj;

	int objwinSlowPath = GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt);
	int objwinDisable = 0;
	if (objwinSlowPath) {
		objwinDisable = !renderer->objwin.objEnable;
	}
	if (objwinSlowPath && objwinDisable) {
		for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x, ++pixel) {
			uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
			uint32_t current = *pixel;
			if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && !(current & FLAG_OBJWIN) && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
				_compositeBlendObjwin(renderer, pixel, color | flags, current);
			}
		}
	} else {
		for (x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x, ++pixel) {
			uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
			uint32_t current = *pixel;
			if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
				_compositeBlendNoObjwin(renderer, pixel, color | flags, current);
			}
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

static inline unsigned _brighten(unsigned color, int y) {
	unsigned c = 0;
	unsigned a;
#ifdef COLOR_16_BIT
	a = color & 0x1F;
	c |= (a + ((0x1F - a) * y) / 16) & 0x1F;

#ifdef COLOR_5_6_5
	a = color & 0x7C0;
	c |= (a + ((0x7C0 - a) * y) / 16) & 0x7C0;

	a = color & 0xF800;
	c |= (a + ((0xF800 - a) * y) / 16) & 0xF800;
#else
	a = color & 0x3E0;
	c |= (a + ((0x3E0 - a) * y) / 16) & 0x3E0;

	a = color & 0x7C00;
	c |= (a + ((0x7C00 - a) * y) / 16) & 0x7C00;
#endif
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

static inline unsigned _darken(unsigned color, int y) {
	unsigned c = 0;
	unsigned a;
#ifdef COLOR_16_BIT
	a = color & 0x1F;
	c |= (a - (a * y) / 16) & 0x1F;

#ifdef COLOR_5_6_5
	a = color & 0x7C0;
	c |= (a - (a * y) / 16) & 0x7C0;

	a = color & 0xF800;
	c |= (a - (a * y) / 16) & 0xF800;
#else
	a = color & 0x3E0;
	c |= (a - (a * y) / 16) & 0x3E0;

	a = color & 0x7C00;
	c |= (a - (a * y) / 16) & 0x7C00;
#endif
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

static unsigned _mix(int weightA, unsigned colorA, int weightB, unsigned colorB) {
	unsigned c = 0;
	unsigned a, b;
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	a = colorA & 0xF81F;
	b = colorB & 0xF81F;
	a |= (colorA & 0x7C0) << 16;
	b |= (colorB & 0x7C0) << 16;
	c = ((a * weightA + b * weightB) / 16);
	if (c & 0x08000000) {
		c = (c & ~0x0FC00000) | 0x07C00000;
	}
	if (c & 0x0020) {
		c = (c & ~0x003F) | 0x001F;
	}
	if (c & 0x10000) {
		c = (c & ~0x1F800) | 0xF800;
	}
	c = (c & 0xF81F) | ((c >> 16) & 0x07C0);
#else
	a = colorA & 0x7C1F;
	b = colorB & 0x7C1F;
	a |= (colorA & 0x3E0) << 16;
	b |= (colorB & 0x3E0) << 16;
	c = ((a * weightA + b * weightB) / 16);
	if (c & 0x04000000) {
		c = (c & ~0x07E00000) | 0x03E00000;
	}
	if (c & 0x0020) {
		c = (c & ~0x003F) | 0x001F;
	}
	if (c & 0x10000) {
		c = (c & ~0x1F800) | 0xF800;
	}
	c = (c & 0x7C1F) | ((c >> 16) & 0x03E0);
#endif
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
