/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SOFTWARE_PRIVATE_H
#define SOFTWARE_PRIVATE_H

#include "video-software.h"

#ifdef NDEBUG
#define VIDEO_CHECKS false
#else
#define VIDEO_CHECKS true
#endif

void GBAVideoSoftwareRendererDrawBackgroundMode0(struct GBAVideoSoftwareRenderer* renderer,
                                                 struct GBAVideoSoftwareBackground* background, int y);
void GBAVideoSoftwareRendererDrawBackgroundMode2(struct GBAVideoSoftwareRenderer* renderer,
                                                 struct GBAVideoSoftwareBackground* background, int y);
void GBAVideoSoftwareRendererDrawBackgroundMode3(struct GBAVideoSoftwareRenderer* renderer,
                                                 struct GBAVideoSoftwareBackground* background, int y);
void GBAVideoSoftwareRendererDrawBackgroundMode4(struct GBAVideoSoftwareRenderer* renderer,
                                                 struct GBAVideoSoftwareBackground* background, int y);
void GBAVideoSoftwareRendererDrawBackgroundMode5(struct GBAVideoSoftwareRenderer* renderer,
                                                 struct GBAVideoSoftwareBackground* background, int y);

int GBAVideoSoftwareRendererPreprocessSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int y);
void GBAVideoSoftwareRendererPostprocessSprite(struct GBAVideoSoftwareRenderer* renderer, unsigned priority);

static inline unsigned _brighten(unsigned color, int y);
static inline unsigned _darken(unsigned color, int y);
static unsigned _mix(int weightA, unsigned colorA, int weightB, unsigned colorB);


// We stash the priority on the top bits so we can do a one-operator comparison
// The lower the number, the higher the priority, and sprites take precendence over backgrounds
// We want to do special processing if the color pixel is target 1, however

static inline void _compositeBlendObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color, uint32_t current) {
	if (color >= current) {
		if (current & FLAG_TARGET_1 && color & FLAG_TARGET_2) {
			color = _mix(renderer->blda, current, renderer->bldb, color);
		} else {
			color = current & (0x00FFFFFF | FLAG_TARGET_1);
		}
	} else {
		color = (color & ~FLAG_TARGET_2) | (current & FLAG_OBJWIN);
	}
	*pixel = color;
}

static inline void _compositeBlendNoObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color, uint32_t current) {
	if (color >= current) {
		if (current & FLAG_TARGET_1 && color & FLAG_TARGET_2) {
			color = _mix(renderer->blda, current, renderer->bldb, color);
		} else {
			color = current & (0x00FFFFFF | FLAG_TARGET_1);
		}
	} else {
		color = color & ~FLAG_TARGET_2;
	}
	*pixel = color;
}

static inline void _compositeNoBlendObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color,
                                           uint32_t current) {
	UNUSED(renderer);
	if (color < current) {
		color |= (current & FLAG_OBJWIN);
	} else {
		color = current & (0x00FFFFFF | FLAG_TARGET_1);
	}
	*pixel = color;
}

static inline void _compositeNoBlendNoObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color,
                                             uint32_t current) {
	UNUSED(renderer);
	if (color >= current) {
		color = current & (0x00FFFFFF | FLAG_TARGET_1);
	}
	*pixel = color;
}

#define COMPOSITE_16_OBJWIN(BLEND)                                                                              \
	if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) {                                          \
		unsigned color = (current & FLAG_OBJWIN) ? objwinPalette[paletteData | pixelData] : palette[pixelData]; \
		unsigned mergedFlags = flags; \
		if (current & FLAG_OBJWIN) { \
			mergedFlags = objwinFlags; \
		} \
		_composite ## BLEND ## Objwin(renderer, pixel, color | mergedFlags, current); \
	}

#define COMPOSITE_16_NO_OBJWIN(BLEND) \
	_composite ## BLEND ## NoObjwin(renderer, pixel, palette[pixelData] | flags, current);

#define COMPOSITE_256_OBJWIN(BLEND) \
	if (objwinForceEnable || !(current & FLAG_OBJWIN) == objwinOnly) { \
		unsigned color = (current & FLAG_OBJWIN) ? objwinPalette[pixelData] : palette[pixelData]; \
		unsigned mergedFlags = flags; \
		if (current & FLAG_OBJWIN) { \
			mergedFlags = objwinFlags; \
		} \
		_composite ## BLEND ## Objwin(renderer, pixel, color | mergedFlags, current); \
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

// TODO: Remove UNUSEDs after implementing OBJWIN for modes 3 - 5
#define PREPARE_OBJWIN                                                                            \
	int objwinSlowPath = GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt);                     \
	int objwinOnly = 0;                                                                           \
	int objwinForceEnable = 0;                                                                    \
	UNUSED(objwinForceEnable);                                                                    \
	color_t* objwinPalette = renderer->normalPalette;                                             \
	UNUSED(objwinPalette);                                                                        \
	if (objwinSlowPath) {                                                                         \
		if (background->target1 && GBAWindowControlIsBlendEnable(renderer->objwin.packed) &&      \
		    (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN)) { \
			objwinPalette = renderer->variantPalette;                                             \
		}                                                                                         \
		switch (background->index) {                                                              \
		case 0:                                                                                   \
			objwinForceEnable = GBAWindowControlIsBg0Enable(renderer->objwin.packed) &&           \
			    GBAWindowControlIsBg0Enable(renderer->currentWindow.packed);                      \
			objwinOnly = !GBAWindowControlIsBg0Enable(renderer->objwin.packed);                   \
			break;                                                                                \
		case 1:                                                                                   \
			objwinForceEnable = GBAWindowControlIsBg1Enable(renderer->objwin.packed) &&           \
			    GBAWindowControlIsBg1Enable(renderer->currentWindow.packed);                      \
			objwinOnly = !GBAWindowControlIsBg1Enable(renderer->objwin.packed);                   \
			break;                                                                                \
		case 2:                                                                                   \
			objwinForceEnable = GBAWindowControlIsBg2Enable(renderer->objwin.packed) &&           \
			    GBAWindowControlIsBg2Enable(renderer->currentWindow.packed);                      \
			objwinOnly = !GBAWindowControlIsBg2Enable(renderer->objwin.packed);                   \
			break;                                                                                \
		case 3:                                                                                   \
			objwinForceEnable = GBAWindowControlIsBg3Enable(renderer->objwin.packed) &&           \
			    GBAWindowControlIsBg3Enable(renderer->currentWindow.packed);                      \
			objwinOnly = !GBAWindowControlIsBg3Enable(renderer->objwin.packed);                   \
			break;                                                                                \
		}                                                                                         \
	}

#define BACKGROUND_BITMAP_INIT                                                                                        \
	int32_t x = background->sx + (renderer->start - 1) * background->dx;                                              \
	int32_t y = background->sy + (renderer->start - 1) * background->dy;                                              \
	int mosaicH = 0;                                                                                                  \
	int mosaicWait = 0;                                                                                               \
	if (background->mosaic) {                                                                                         \
		int mosaicV = GBAMosaicControlGetBgV(renderer->mosaic) + 1;                                                   \
		y -= (inY % mosaicV) * background->dmy;                                                                       \
		x -= (inY % mosaicV) * background->dmx;                                                                       \
		mosaicH = GBAMosaicControlGetBgH(renderer->mosaic);                                                           \
		mosaicWait = renderer->start % (mosaicH + 1);                                                                 \
	}                                                                                                                 \
	int32_t localX;                                                                                                   \
	int32_t localY;                                                                                                   \
                                                                                                                      \
	int flags = (background->priority << OFFSET_PRIORITY) | (background->index << OFFSET_INDEX) | FLAG_IS_BACKGROUND; \
	flags |= FLAG_TARGET_2 * background->target2;                                                                     \
	int objwinFlags = FLAG_TARGET_1 * (background->target1 && renderer->blendEffect == BLEND_ALPHA &&                 \
	                                   GBAWindowControlIsBlendEnable(renderer->objwin.packed));                       \
	objwinFlags |= flags;                                                                                             \
	flags |= FLAG_TARGET_1 * (background->target1 && renderer->blendEffect == BLEND_ALPHA &&                          \
	                          GBAWindowControlIsBlendEnable(renderer->currentWindow.packed));                         \
	int variant = background->target1 && GBAWindowControlIsBlendEnable(renderer->currentWindow.packed) &&             \
	    (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);                           \
	color_t* palette = renderer->normalPalette;                                                                       \
	if (variant) {                                                                                                    \
		palette = renderer->variantPalette;                                                                           \
	}                                                                                                                 \
	UNUSED(palette);                                                                                                  \
	PREPARE_OBJWIN;

#define BACKGROUND_BITMAP_ITERATE(W, H)                     \
	x += background->dx;                                    \
	y += background->dy;                                    \
                                                            \
	if (x < 0 || y < 0 || (x >> 8) >= W || (y >> 8) >= H) { \
		continue;                                           \
	} else {                                                \
		localX = x;                                         \
		localY = y;                                         \
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

#endif
