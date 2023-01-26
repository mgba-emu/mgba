/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SOFTWARE_PRIVATE_H
#define SOFTWARE_PRIVATE_H

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/renderers/video-software.h>

#ifdef NDEBUG
#define VIDEO_CHECKS false
#else
#define VIDEO_CHECKS true
#endif

#define ENABLED_MAX 4

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

int GBAVideoSoftwareRendererPreprocessSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int index, int y);
void GBAVideoSoftwareRendererPostprocessSprite(struct GBAVideoSoftwareRenderer* renderer, unsigned priority);

static inline unsigned _brighten(unsigned color, int y);
static inline unsigned _darken(unsigned color, int y);

// We stash the priority on the top bits so we can do a one-operator comparison
// The lower the number, the higher the priority, and sprites take precedence over backgrounds
// We want to do special processing if the color pixel is target 1, however

static inline void _compositeBlendObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color, uint32_t current) {
	if (color >= current) {
		if (current & FLAG_TARGET_1 && color & FLAG_TARGET_2) {
			color = mColorMix5Bit(renderer->blda, current, renderer->bldb, color);
		} else {
			color = current & (0x00FFFFFF | FLAG_REBLEND | FLAG_OBJWIN);
		}
	} else {
		color = (color & ~FLAG_TARGET_2) | (current & FLAG_OBJWIN);
	}
	*pixel = color;
}

static inline void _compositeBlendNoObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color, uint32_t current) {
	if (color >= current) {
		if (current & FLAG_TARGET_1 && color & FLAG_TARGET_2) {
			color = mColorMix5Bit(renderer->blda, current, renderer->bldb, color);
		} else {
			color = current & (0x00FFFFFF | FLAG_REBLEND | FLAG_OBJWIN);
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
		color = current & (0x00FFFFFF | FLAG_REBLEND | FLAG_OBJWIN);
	}
	*pixel = color;
}

static inline void _compositeNoBlendNoObjwin(struct GBAVideoSoftwareRenderer* renderer, uint32_t* pixel, uint32_t color,
                                             uint32_t current) {
	UNUSED(renderer);
	if (color >= current) {
		color = current & (0x00FFFFFF | FLAG_REBLEND | FLAG_OBJWIN);
	}
	*pixel = color;
}

#define COMPOSITE_16_OBJWIN(BLEND, IDX)  \
	if (objwinForceEnable || (!(current & FLAG_OBJWIN)) == objwinOnly) {                                          \
		unsigned color; \
		unsigned mergedFlags = flags; \
		if (current & FLAG_OBJWIN) { \
			mergedFlags = objwinFlags; \
			color = objwinPalette[paletteData | pixelData]; \
		} else if ((current & (FLAG_IS_BACKGROUND | FLAG_REBLEND)) == FLAG_REBLEND) { \
			color = renderer->normalPalette[paletteData | pixelData]; \
		} else { \
			color = palette[paletteData | pixelData]; \
		} \
		_composite ## BLEND ## Objwin(renderer, &pixel[IDX], color | mergedFlags, current); \
	}

#define COMPOSITE_16_NO_OBJWIN(BLEND, IDX) \
	{ \
		unsigned color; \
		if ((current & (FLAG_IS_BACKGROUND | FLAG_REBLEND)) == FLAG_REBLEND) { \
			color = renderer->normalPalette[paletteData | pixelData]; \
		} else { \
			color = palette[paletteData | pixelData]; \
		} \
		_composite ## BLEND ## NoObjwin(renderer, &pixel[IDX], color | flags, current); \
	}

#define COMPOSITE_256_OBJWIN(BLEND, IDX) \
	if (objwinForceEnable || (!(current & FLAG_OBJWIN)) == objwinOnly) { \
		unsigned color; \
		unsigned mergedFlags = flags; \
		if (current & FLAG_OBJWIN) { \
			mergedFlags = objwinFlags; \
			color = objwinPalette[pixelData]; \
		} else if ((current & (FLAG_IS_BACKGROUND | FLAG_REBLEND)) == FLAG_REBLEND) { \
			color = renderer->normalPalette[pixelData]; \
		} else { \
			color = palette[pixelData]; \
		} \
		_composite ## BLEND ## Objwin(renderer, &pixel[IDX], color | mergedFlags, current); \
	}

#define COMPOSITE_256_NO_OBJWIN(BLEND, IDX) \
	{ \
		unsigned color; \
		if ((current & (FLAG_IS_BACKGROUND | FLAG_REBLEND)) == FLAG_REBLEND) { \
			color = renderer->normalPalette[pixelData]; \
		} else { \
			color = palette[pixelData]; \
		} \
		_composite ## BLEND ## NoObjwin(renderer, &pixel[IDX], color | flags, current); \
	}

#define BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, IDX) \
	pixelData = tileData & 0xF; \
	current = pixel[IDX]; \
	if (pixelData && IS_WRITABLE(current)) { \
		COMPOSITE_16_ ## OBJWIN (BLEND, IDX); \
	} \
	tileData >>= 4;

#define BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, IDX) \
	pixelData = tileData & 0xFF; \
	current = pixel[IDX]; \
	if (pixelData && IS_WRITABLE(current)) { \
		COMPOSITE_256_ ## OBJWIN (BLEND, IDX); \
	} \
	tileData >>= 8;

// TODO: Remove UNUSEDs after implementing OBJWIN for modes 3 - 5
#define PREPARE_OBJWIN                                                                            \
	int objwinSlowPath = GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt);                     \
	int objwinOnly = 0;                                                                           \
	int objwinForceEnable = 0;                                                                    \
	UNUSED(objwinForceEnable);                                                                    \
	color_t* objwinPalette = renderer->normalPalette;                                             \
	if (renderer->d.highlightAmount && background->highlight) {                                   \
		objwinPalette = renderer->highlightPalette;                                               \
	}                                                                                             \
	UNUSED(objwinPalette);                                                                        \
	if (objwinSlowPath) {                                                                         \
		if (background->target1 && GBAWindowControlIsBlendEnable(renderer->objwin.packed) &&      \
		    (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN)) { \
			objwinPalette = renderer->variantPalette;                                             \
			if (renderer->d.highlightAmount && background->highlight) {                           \
				palette = renderer->highlightVariantPalette;                                      \
			}                                                                                     \
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
	int32_t localX;                                                                                                   \
	int32_t localY;                                                                                                   \
	if (background->mosaic) {                                                                                         \
		int mosaicV = GBAMosaicControlGetBgV(renderer->mosaic) + 1;                                                   \
		mosaicH = GBAMosaicControlGetBgH(renderer->mosaic) + 1;                                                       \
		mosaicWait = (mosaicH - renderer->start + GBA_VIDEO_HORIZONTAL_PIXELS * mosaicH) % mosaicH;                   \
		int32_t startX = renderer->start - (renderer->start % mosaicH);                                               \
		--mosaicH;                                                                                                    \
		localX = -(inY % mosaicV) * background->dmx;                                                                  \
		localY = -(inY % mosaicV) * background->dmy;                                                                  \
		x += localX;                                                                                                  \
		y += localY;                                                                                                  \
		localX += background->sx + startX * background->dx;                                                           \
		localY += background->sy + startX * background->dy;                                                           \
	}                                                                                                                 \
                                                                                                                      \
	uint32_t flags = background->flags;                                                                               \
	uint32_t objwinFlags = background->objwinFlags;                                                                   \
	bool variant = background->variant;                                                                               \
	color_t* palette = renderer->normalPalette;                                                                       \
	if (renderer->d.highlightAmount && background->highlight) {                                                       \
		palette = renderer->highlightPalette;                                                                         \
	}                                                                                                                 \
	if (variant) {                                                                                                    \
		palette = renderer->variantPalette;                                                                           \
		if (renderer->d.highlightAmount && background->highlight) {                                                   \
			palette = renderer->highlightVariantPalette;                                                              \
		}                                                                                                             \
	}                                                                                                                 \
	UNUSED(palette);                                                                                                  \
	PREPARE_OBJWIN;

#define TEST_LAYER_ENABLED(X) \
	!softwareRenderer->d.disableBG[X] && \
	(softwareRenderer->bg[X].enabled == ENABLED_MAX && \
	(GBAWindowControlIsBg ## X ## Enable(softwareRenderer->currentWindow.packed) || \
	(GBARegisterDISPCNTIsObjwinEnable(softwareRenderer->dispcnt) && GBAWindowControlIsBg ## X ## Enable (softwareRenderer->objwin.packed))) && \
	softwareRenderer->bg[X].priority == priority)

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
	a = color & 0xFF;
	c |= (a + ((0xFF - a) * y) / 16) & 0xFF;

	a = color & 0xFF00;
	c |= (a + ((0xFF00 - a) * y) / 16) & 0xFF00;

	a = color & 0xFF0000;
	c |= (a + ((0xFF0000 - a) * y) / 16) & 0xFF0000;
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
	a = color & 0xFF;
	c |= (a - (a * y) / 16) & 0xFF;

	a = color & 0xFF00;
	c |= (a - (a * y) / 16) & 0xFF00;

	a = color & 0xFF0000;
	c |= (a - (a * y) / 16) & 0xFF0000;
#endif
	return c;
}

#endif
