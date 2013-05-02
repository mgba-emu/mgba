#include "video-software.h"

#include "gba.h"
#include "gba-io.h"

#include <string.h>

static void GBAVideoSoftwareRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer);
static void GBAVideoSoftwareRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer);

static void GBAVideoSoftwareRendererUpdateDISPCNT(struct GBAVideoSoftwareRenderer* renderer);
static void GBAVideoSoftwareRendererWriteBGCNT(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* bg, uint16_t value);
static void GBAVideoSoftwareRendererWriteBLDCNT(struct GBAVideoSoftwareRenderer* renderer, uint16_t value);

static void _drawScanline(struct GBAVideoSoftwareRenderer* renderer, int y);
static void _drawBackgroundMode0(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y);
static void _drawTransformedSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBATransformedObj* sprite, int y);
static void _drawSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int y);

static void _updatePalettes(struct GBAVideoSoftwareRenderer* renderer);
static inline uint32_t _brighten(uint32_t color, int y);
static inline uint32_t _darken(uint32_t color, int y);
static uint32_t _mix(int weightA, uint32_t colorA, int weightB, uint32_t colorB);

void GBAVideoSoftwareRendererCreate(struct GBAVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBAVideoSoftwareRendererInit;
	renderer->d.deinit = GBAVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBAVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writePalette = GBAVideoSoftwareRendererWritePalette;
	renderer->d.drawScanline = GBAVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = GBAVideoSoftwareRendererFinishFrame;

	renderer->d.turbo = 0;
	renderer->d.framesPending = 0;

	{
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
		renderer->mutex = mutex;
		pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
		renderer->upCond = cond;
		renderer->downCond = cond;
	}
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

	softwareRenderer->blda = 0;
	softwareRenderer->bldb = 0;
	softwareRenderer->bldy = 0;

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

	pthread_mutex_init(&softwareRenderer->mutex, 0);
	pthread_cond_init(&softwareRenderer->upCond, 0);
	pthread_cond_init(&softwareRenderer->downCond, 0);
}

static void GBAVideoSoftwareRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	pthread_mutex_destroy(&softwareRenderer->mutex);
	pthread_cond_destroy(&softwareRenderer->upCond);
	pthread_cond_destroy(&softwareRenderer->downCond);
}

static uint16_t GBAVideoSoftwareRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	switch (address) {
	case REG_DISPCNT:
		value &= 0xFFFB;
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
	default:
		GBALog(GBA_LOG_STUB, "Stub video register write: %03x", address);
	}
	return value;
}

static void GBAVideoSoftwareRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	uint32_t color32 = 0;
	if (address & 0x1F) {
		color32 = 0xFF000000;
	}
	color32 |= (value << 3) & 0xF8;
	color32 |= (value << 6) & 0xF800;
	color32 |= (value << 9) & 0xF80000;
	softwareRenderer->normalPalette[address >> 1] = color32;
	if (softwareRenderer->blendEffect == BLEND_BRIGHTEN) {
		softwareRenderer->variantPalette[address >> 1] = _brighten(color32, softwareRenderer->bldy);
	} else if (softwareRenderer->blendEffect == BLEND_DARKEN) {
		softwareRenderer->variantPalette[address >> 1] = _darken(color32, softwareRenderer->bldy);
	}
}

static void GBAVideoSoftwareRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;
	uint32_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
	if (softwareRenderer->dispcnt.forcedBlank) {
		for (int x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GBA_COLOR_WHITE;
		}
		return;
	} else {
		uint32_t backdrop;
		if (!softwareRenderer->target1Bd || softwareRenderer->blendEffect == BLEND_NONE || softwareRenderer->blendEffect == BLEND_ALPHA) {
			backdrop = softwareRenderer->normalPalette[0];
		} else {
			backdrop = softwareRenderer->variantPalette[0];
		}
		for (int x = 0; x < VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = backdrop;
		}
	}

	memset(softwareRenderer->flags, 0, sizeof(softwareRenderer->flags));
	memset(softwareRenderer->spriteLayer, 0, sizeof(softwareRenderer->spriteLayer));
	softwareRenderer->row = row;

	softwareRenderer->start = 0;
	softwareRenderer->end = VIDEO_HORIZONTAL_PIXELS;
	_drawScanline(softwareRenderer, y);
}

static void GBAVideoSoftwareRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoSoftwareRenderer* softwareRenderer = (struct GBAVideoSoftwareRenderer*) renderer;

	pthread_mutex_lock(&softwareRenderer->mutex);
	renderer->framesPending++;
	pthread_cond_broadcast(&softwareRenderer->upCond);
	if (!renderer->turbo) {
		pthread_cond_wait(&softwareRenderer->downCond, &softwareRenderer->mutex);
	}
	pthread_mutex_unlock(&softwareRenderer->mutex);
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

static void _drawScanline(struct GBAVideoSoftwareRenderer* renderer, int y) {
	int i;
	if (renderer->dispcnt.objEnable) {
		for (i = 0; i < 128; ++i) {
			struct GBAObj* sprite = &renderer->d.oam->obj[i];
			if (sprite->transformed) {
				_drawTransformedSprite(renderer, &renderer->d.oam->tobj[i], y);
			} else if (!sprite->disable) {
				_drawSprite(renderer, sprite, y);
			}
		}
	}

	int priority;
	for (priority = 0; priority < 4; ++priority) {
		for (i = 0; i < 4; ++i) {
			if (renderer->bg[i].enabled && renderer->bg[i].priority == priority) {
				_drawBackgroundMode0(renderer, &renderer->bg[i], y);
			}
		}
	}
}

static void _composite(struct GBAVideoSoftwareRenderer* renderer, int offset, uint32_t color, struct PixelFlags flags) {
	struct PixelFlags currentFlags = renderer->flags[offset];
	if (currentFlags.isSprite && flags.priority >= currentFlags.priority) {
		if (currentFlags.target1) {
			if (currentFlags.written && currentFlags.target2) {
				renderer->row[offset] = _mix(renderer->blda, renderer->row[offset], renderer->bldb, renderer->spriteLayer[offset]);
			} else if (flags.target2) {
				renderer->row[offset] = _mix(renderer->bldb, color, renderer->blda, renderer->spriteLayer[offset]);
			}
		} else if (!currentFlags.written) {
			renderer->row[offset] = renderer->spriteLayer[offset];
		}
		renderer->flags[offset].finalized = 1;
		return;
	}
	if (renderer->blendEffect != BLEND_ALPHA) {
		renderer->row[offset] = color;
		renderer->flags[offset].finalized = 1;
	} else if (renderer->blendEffect == BLEND_ALPHA) {
		if (currentFlags.written) {
			if (currentFlags.target1 && flags.target2) {
				renderer->row[offset] = _mix(renderer->bldb, color, renderer->blda, renderer->row[offset]);
			}
			renderer->flags[offset].finalized = 1;
		} else {
			renderer->row[offset] = color;
			renderer->flags[offset].target1 = flags.target1;
		}
	}
	renderer->flags[offset].written = 1;
}

#define BACKGROUND_DRAW_PIXEL_16_NORMAL \
	if (tileData & 0xF && !renderer->flags[outX].finalized) { \
		_composite(renderer, outX, renderer->normalPalette[tileData & 0xF | (mapData.palette << 4)], flags); \
	} \
	tileData >>= 4;

#define BACKGROUND_DRAW_PIXEL_16_VARIANT \
	if (tileData & 0xF && !renderer->flags[outX].finalized) { \
		_composite(renderer, outX, renderer->variantPalette[tileData & 0xF | (mapData.palette << 4)], flags); \
	} \
	tileData >>= 4;

#define BACKGROUND_DRAW_PIXEL_256_NORMAL \
	if (tileData & 0xFF && !renderer->flags[outX].finalized) { \
		_composite(renderer, outX, renderer->normalPalette[tileData & 0xFF], flags); \
	} \
	tileData >>= 8;

#define BACKGROUND_DRAW_PIXEL_256_VARIANT \
	if (tileData & 0xFF && !renderer->flags[outX].finalized) { \
		_composite(renderer, outX, renderer->variantPalette[tileData & 0xFF], flags); \
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

#define BACKGROUND_MODE_0_TILE_16_LOOP(TYPE) \
	for (; tileX < 30; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = ((background->charBase + (mapData.tile << 5)) >> 2) + localY; \
		uint32_t tileData = ((uint32_t*)renderer->d.vram)[charBase]; \
		if (tileData) { \
			if (!mapData.hflip) { \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				++outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				++outX; \
			} else { \
				outX += 7; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				--outX; \
				BACKGROUND_DRAW_PIXEL_16_ ## TYPE; \
				outX += 8; \
			} \
		} else { \
			outX += 8; \
		} \
	}

#define BACKGROUND_MODE_0_TILE_256_LOOP(TYPE) \
		for (; tileX < 30; ++tileX) { \
			BACKGROUND_TEXT_SELECT_CHARACTER; \
			charBase = ((background->charBase + (mapData.tile << 6)) >> 2) + (localY << 1); \
			if (!mapData.hflip) { \
				uint32_t tileData = ((uint32_t*)renderer->d.vram)[charBase]; \
				if (tileData) { \
						BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
						++outX; \
						BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
						++outX; \
						BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
						++outX; \
						BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
						++outX; \
				} else { \
					outX += 4; \
				} \
				tileData = ((uint32_t*)renderer->d.vram)[charBase + 1]; \
				if (tileData) { \
						BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
						++outX; \
						BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
						++outX; \
						BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
						++outX; \
						BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
						++outX; \
				} else { \
					outX += 4; \
				} \
			} else { \
				uint32_t tileData = ((uint32_t*)renderer->d.vram)[charBase + 1]; \
				if (tileData) { \
					outX += 3; \
					BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
					--outX; \
					BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
					--outX; \
					BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
					--outX; \
					BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
					outX += 4; \
				} else { \
					outX += 4; \
				} \
				tileData = ((uint32_t*)renderer->d.vram)[charBase]; \
				if (tileData) { \
					outX += 3; \
					BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
					--outX; \
					BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
					--outX; \
					BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
					--outX; \
					BACKGROUND_DRAW_PIXEL_256_ ## TYPE; \
					outX += 4; \
				} else { \
					outX += 4; \
				} \
			} \
		}

static void _drawBackgroundMode0(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y) {
	int inX = background->x;
	int inY = y + background->y;
	union GBATextMapData mapData;

	unsigned yBase = inY & 0xF8;
	if (background->size & 2) {
		yBase += inY & 0x100;
	} else if (background->size == 3) {
		yBase += (inY & 0x100) << 1;
	}

	int localX;
	int localY;

	unsigned xBase;

	struct PixelFlags flags = {
		.target1 = background->target1 && renderer->blendEffect == BLEND_ALPHA,
		.target2 = background->target2,
		.priority = background->priority
	};

	uint32_t screenBase;
	uint32_t charBase;
	int variant = background->target1 && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);

	int outX = 0;
	int tileX = 0;
	if (inX & 0x7) {
		int end = 0x8 - (inX & 0x7);
		uint32_t tileData;
		BACKGROUND_TEXT_SELECT_CHARACTER;
		charBase = ((background->charBase + (mapData.tile << 5)) >> 2) + localY;
		tileData = ((uint32_t*)renderer->d.vram)[charBase];
		tileData >>= 4 * (inX & 0x7);
		if (!variant) {
			for (outX = 0; outX < end; ++outX) {
				BACKGROUND_DRAW_PIXEL_16_NORMAL;
			}
		} else {
			for (outX = 0; outX < end; ++outX) {
				BACKGROUND_DRAW_PIXEL_16_VARIANT;
			}
		}

		tileX = 30;
		BACKGROUND_TEXT_SELECT_CHARACTER;
		charBase = ((background->charBase + (mapData.tile << 5)) >> 2) + localY;
		tileData = ((uint32_t*)renderer->d.vram)[charBase];
		if (!variant) {
			for (outX = VIDEO_HORIZONTAL_PIXELS - 8 + end; outX < VIDEO_HORIZONTAL_PIXELS; ++outX) {
				BACKGROUND_DRAW_PIXEL_16_NORMAL;
			}
		} else {
			for (outX = VIDEO_HORIZONTAL_PIXELS - 8 + end; outX < VIDEO_HORIZONTAL_PIXELS; ++outX) {
				BACKGROUND_DRAW_PIXEL_16_VARIANT;
			}
		}

		tileX = 1;
		outX = end;
	}

	if (!background->multipalette) {
		if (!variant) {
			BACKGROUND_MODE_0_TILE_16_LOOP(NORMAL);
		 } else {
			BACKGROUND_MODE_0_TILE_16_LOOP(VARIANT);
		 }
	} else {
		if (!variant) {
			BACKGROUND_MODE_0_TILE_256_LOOP(NORMAL);
		 } else {
			BACKGROUND_MODE_0_TILE_256_LOOP(VARIANT);
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
		for (int outX = x >= start ? x : start; outX < x + width && outX < end; ++outX) { \
			int inX = outX - x; \
			if (sprite->hflip) { \
				inX = width - inX - 1; \
			} \
			if (renderer->flags[outX].isSprite) { \
				continue; \
			} \
			SPRITE_XBASE_ ## DEPTH(inX); \
			SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(inX); \
		}

#define SPRITE_TRANSFORMED_LOOP(DEPTH, TYPE) \
	for (int outX = x >= start ? x : start; outX < x + totalWidth && outX < end; ++outX) { \
		if (renderer->flags[outX].isSprite) { \
			continue; \
		} \
		int inY = y - sprite->y; \
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
	if (tileData) { \
		renderer->spriteLayer[outX] = renderer->normalPalette[0x100 | tileData | (sprite->palette << 4)]; \
		renderer->flags[outX] = flags; \
	}

#define SPRITE_DRAW_PIXEL_16_VARIANT(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData) { \
		renderer->spriteLayer[outX] = renderer->variantPalette[0x100 | tileData | (sprite->palette << 4)]; \
		renderer->flags[outX] = flags; \
	}

#define SPRITE_XBASE_256(localX) unsigned xBase = (localX & ~0x7) * 8 + (localX & 6);
#define SPRITE_YBASE_256(localY) unsigned yBase = (localY & ~0x7) * (renderer->dispcnt.objCharacterMapping ? width : 0x100) + (localY & 0x7) * 8;

#define SPRITE_DRAW_PIXEL_256_NORMAL(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData) { \
		renderer->spriteLayer[outX] = renderer->normalPalette[0x100 | tileData]; \
		renderer->flags[outX] = flags; \
	}

#define SPRITE_DRAW_PIXEL_256_VARIANT(localX) \
	uint16_t tileData = renderer->d.vram[(yBase + charBase + xBase) >> 1]; \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData) { \
		renderer->spriteLayer[outX] = renderer->variantPalette[0x100 | tileData]; \
		renderer->flags[outX] = flags; \
	}

static void _drawSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int y) {
	int width = _objSizes[sprite->shape * 8 + sprite->size * 2];
	int height = _objSizes[sprite->shape * 8 + sprite->size * 2 + 1];
	int start = renderer->start;
	int end = renderer->end;
	if ((y < sprite->y && (sprite->y + height - 256 < 0 || y >= sprite->y + height - 256)) || y >= sprite->y + height) {
		return;
	}
	struct PixelFlags flags = {
		.priority = sprite->priority,
		.isSprite = 1,
		.target1 = (renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || sprite->mode == OBJ_MODE_SEMITRANSPARENT,
		.target2 = renderer->target2Obj
	};
	int x = sprite->x;
	int inY = y - sprite->y;
	if (sprite->y + height - 256 >= 0) {
		inY += 256;
	}
	if (sprite->vflip) {
		inY = height - inY - 1;
	}
	unsigned charBase = BASE_TILE + sprite->tile * 0x20;
	int variant = renderer->target1Obj && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	if (!sprite->multipalette) {
		if (!variant) {
			SPRITE_NORMAL_LOOP(16, NORMAL);
		} else {
			SPRITE_NORMAL_LOOP(16, VARIANT);
		}
	} else {
		if (!variant) {
			SPRITE_NORMAL_LOOP(256, NORMAL);
		} else {
			SPRITE_NORMAL_LOOP(256, VARIANT);
		}
	}
}

static void _drawTransformedSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBATransformedObj* sprite, int y) {
	int width = _objSizes[sprite->shape * 8 + sprite->size * 2];
	int totalWidth = width << sprite->doublesize;
	int height = _objSizes[sprite->shape * 8 + sprite->size * 2 + 1];
	int totalHeight = height << sprite->doublesize;
	int start = renderer->start;
	int end = renderer->end;
	if ((y < sprite->y && (sprite->y + totalHeight - 256 < 0 || y >= sprite->y + totalHeight - 256)) || y >= sprite->y + totalHeight) {
		return;
	}
	struct PixelFlags flags = {
		.priority = sprite->priority,
		.isSprite = 1,
		.target1 = (renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || sprite->mode == OBJ_MODE_SEMITRANSPARENT,
		.target2 = renderer->target2Obj
	};
	int x = sprite->x;
	unsigned charBase = BASE_TILE + sprite->tile * 0x20;
	struct GBAOAMMatrix* mat = &renderer->d.oam->mat[sprite->matIndex];
	int variant = renderer->target1Obj && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	if (!sprite->multipalette) {
		if (!variant) {
			SPRITE_TRANSFORMED_LOOP(16, NORMAL);
		} else {
			SPRITE_TRANSFORMED_LOOP(16, VARIANT);
		}
	} else {
		if (!variant) {
			SPRITE_TRANSFORMED_LOOP(256, NORMAL);
		} else {
			SPRITE_TRANSFORMED_LOOP(256, VARIANT);
		}
	}
}

static void _updatePalettes(struct GBAVideoSoftwareRenderer* renderer) {
	if (renderer->blendEffect == BLEND_BRIGHTEN) {
		for (int i = 0; i < 512; ++i) {
			renderer->variantPalette[i] = _brighten(renderer->normalPalette[i], renderer->bldy);
		}
	} else if (renderer->blendEffect == BLEND_DARKEN) {
		for (int i = 0; i < 512; ++i) {
			renderer->variantPalette[i] = _darken(renderer->normalPalette[i], renderer->bldy);
		}
	} else {
		for (int i = 0; i < 512; ++i) {
			renderer->variantPalette[i] = renderer->normalPalette[i];
		}
	}
}

static inline uint32_t _brighten(uint32_t color, int y) {
	uint32_t c = 0;
	uint32_t a;
	a = color & 0xF8;
	c |= (a + ((0xF8 - a) * y) / 16) & 0xF8;

	a = color & 0xF800;
	c |= (a + ((0xF800 - a) * y) / 16) & 0xF800;

	a = color & 0xF80000;
	c |= (a + ((0xF80000 - a) * y) / 16) & 0xF80000;
	return c;
}

static inline uint32_t _darken(uint32_t color, int y) {
	uint32_t c = 0;
	uint32_t a;
	a = color & 0xF8;
	c |= (a - (a * y) / 16) & 0xF8;

	a = color & 0xF800;
	c |= (a - (a * y) / 16) & 0xF800;

	a = color & 0xF80000;
	c |= (a - (a * y) / 16) & 0xF80000;
	return c;
}

static uint32_t _mix(int weightA, uint32_t colorA, int weightB, uint32_t colorB) {
	uint32_t c = 0;
	uint32_t a, b;
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
	return c;
}
