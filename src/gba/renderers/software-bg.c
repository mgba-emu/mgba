/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/renderers/software-private.h"

#include <mgba/core/interface.h>
#include <mgba/internal/gba/gba.h>

#define BACKGROUND_BITMAP_ITERATE(W, H) \
	x += background->dx; \
	y += background->dy; \
	if ((x < 0 || y < 0 || (x >> 8) >= W || (y >> 8) >= H) && !mosaicWait) { \
		continue; \
	} \
	localX = x; \
	localY = y;

#define MODE_2_COORD_OVERFLOW \
	localX = x & (sizeAdjusted - 1); \
	localY = y & (sizeAdjusted - 1); \

#define MODE_2_COORD_NO_OVERFLOW \
	if ((x | y) & ~(sizeAdjusted - 1)) { \
		continue; \
	} \
	localX = x; \
	localY = y;

#define MODE_2_NO_MOSAIC(COORD) \
	COORD \
	mapData = screenBase[(localX >> 11) + (((localY >> 7) & 0x7F0) << background->size)]; \
	pixelData = charBase[(mapData << 6) + ((localY & 0x700) >> 5) + ((localX & 0x700) >> 8)];

#define MODE_2_MOSAIC(COORD) \
		if (!mosaicWait) { \
			MODE_2_NO_MOSAIC(COORD) \
			mosaicWait = mosaicH; \
		} else { \
			--mosaicWait; \
		}

#define MODE_2_LOOP(MOSAIC, COORD, BLEND, OBJWIN) \
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) { \
		x += background->dx; \
		y += background->dy; \
		\
		uint32_t current = *pixel; \
		MOSAIC(COORD) \
		if (pixelData && IS_WRITABLE(current)) { \
			COMPOSITE_256_ ## OBJWIN (BLEND, 0); \
		} \
	}

#define DRAW_BACKGROUND_MODE_2(BLEND, OBJWIN) \
	if (background->overflow) { \
		if (mosaicH > 1) { \
			localX &= sizeAdjusted - 1; \
			localY &= sizeAdjusted - 1; \
			MODE_2_NO_MOSAIC(); \
			MODE_2_LOOP(MODE_2_MOSAIC, MODE_2_COORD_OVERFLOW, BLEND, OBJWIN); \
		} else { \
			MODE_2_LOOP(MODE_2_NO_MOSAIC, MODE_2_COORD_OVERFLOW, BLEND, OBJWIN); \
		} \
	} else { \
		if (mosaicH > 1) { \
			if (!((x | y) & ~(sizeAdjusted - 1))) { \
				localX &= sizeAdjusted - 1; \
				localY &= sizeAdjusted - 1; \
				MODE_2_NO_MOSAIC(); \
			} \
			MODE_2_LOOP(MODE_2_MOSAIC, MODE_2_COORD_NO_OVERFLOW, BLEND, OBJWIN); \
		} else { \
			MODE_2_LOOP(MODE_2_NO_MOSAIC, MODE_2_COORD_NO_OVERFLOW, BLEND, OBJWIN); \
		} \
	}

void GBAVideoSoftwareRendererDrawBackgroundMode2(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY) {
	int sizeAdjusted = 0x8000 << background->size;

	BACKGROUND_BITMAP_INIT;

	uint8_t* screenBase = &((uint8_t*) renderer->d.vram)[background->screenBase];
	uint8_t* charBase = &((uint8_t*) renderer->d.vram)[background->charBase];
	uint8_t mapData;
	uint8_t pixelData = 0;

	int outX;
	uint32_t* pixel;

	if (!objwinSlowPath) {
		if (!(flags & FLAG_TARGET_2)) {
			DRAW_BACKGROUND_MODE_2(NoBlend, NO_OBJWIN);
		} else {
			DRAW_BACKGROUND_MODE_2(Blend, NO_OBJWIN);
		}
	} else {
		if (!(flags & FLAG_TARGET_2)) {
			DRAW_BACKGROUND_MODE_2(NoBlend, OBJWIN);
		} else {
			DRAW_BACKGROUND_MODE_2(Blend, OBJWIN);
		}
	}
}

void GBAVideoSoftwareRendererDrawBackgroundMode3(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY) {
	BACKGROUND_BITMAP_INIT;

	uint32_t color = renderer->normalPalette[0];
	if (mosaicWait && localX >= 0 && localY >= 0 && (localX >> 8) < GBA_VIDEO_HORIZONTAL_PIXELS && (localY >> 8) < GBA_VIDEO_VERTICAL_PIXELS) {
		LOAD_16(color, ((localX >> 8) + (localY >> 8) * GBA_VIDEO_HORIZONTAL_PIXELS) << 1, renderer->d.vram);
		color = mColorFrom555(color);
	}

	int outX;
	uint32_t* pixel;
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) {
		BACKGROUND_BITMAP_ITERATE(GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);

		if (!mosaicWait) {
			LOAD_16(color, ((localX >> 8) + (localY >> 8) * GBA_VIDEO_HORIZONTAL_PIXELS) << 1, renderer->d.vram);
			color = mColorFrom555(color);
			mosaicWait = mosaicH;
		} else {
			--mosaicWait;
		}

		uint32_t current = *pixel;
		if (!objwinSlowPath || (!(current & FLAG_OBJWIN)) != objwinOnly) {
			unsigned mergedFlags = flags;
			if (current & FLAG_OBJWIN) {
				mergedFlags = objwinFlags;
			}
			if (!variant) {
				_compositeBlendObjwin(renderer, pixel, color | mergedFlags, current);
			} else if (renderer->blendEffect == BLEND_BRIGHTEN) {
				_compositeBlendObjwin(renderer, pixel, _brighten(color, renderer->bldy) | mergedFlags, current);
			} else if (renderer->blendEffect == BLEND_DARKEN) {
				_compositeBlendObjwin(renderer, pixel, _darken(color, renderer->bldy) | mergedFlags, current);
			}
		}
	}
}

void GBAVideoSoftwareRendererDrawBackgroundMode4(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY) {
	BACKGROUND_BITMAP_INIT;

	uint16_t color = 0;
	uint32_t offset = 0;
	if (GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt)) {
		offset = 0xA000;
	}
	if (mosaicWait && localX >= 0 && localY >= 0 && (localX >> 8) < GBA_VIDEO_HORIZONTAL_PIXELS && (localY >> 8) < GBA_VIDEO_VERTICAL_PIXELS) {
		color = ((uint8_t*)renderer->d.vram)[offset + (localX >> 8) + (localY >> 8) * GBA_VIDEO_HORIZONTAL_PIXELS];
	}

	int outX;
	uint32_t* pixel;
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) {
		BACKGROUND_BITMAP_ITERATE(GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);

		if (!mosaicWait) {
			color = ((uint8_t*)renderer->d.vram)[offset + (localX >> 8) + (localY >> 8) * GBA_VIDEO_HORIZONTAL_PIXELS];

			mosaicWait = mosaicH;
		} else {
			--mosaicWait;
		}

		uint32_t current = *pixel;
		if (color && IS_WRITABLE(current)) {
			if (!objwinSlowPath) {
				_compositeBlendNoObjwin(renderer, pixel, palette[color] | flags, current);
			} else if (objwinForceEnable || (!(current & FLAG_OBJWIN)) == objwinOnly) {
				color_t* currentPalette = (current & FLAG_OBJWIN) ? objwinPalette : palette;
				unsigned mergedFlags = flags;
				if (current & FLAG_OBJWIN) {
					mergedFlags = objwinFlags;
				}
				_compositeBlendObjwin(renderer, pixel, currentPalette[color] | mergedFlags, current);
			}
		}
	}
}

void GBAVideoSoftwareRendererDrawBackgroundMode5(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int inY) {
	BACKGROUND_BITMAP_INIT;

	uint32_t color = renderer->normalPalette[0];
	uint32_t offset = 0;
	if (GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt)) {
		offset = 0xA000;
	}
	if (mosaicWait && localX >= 0 && localY >= 0 && (localX >> 8) < 160 && (localY >> 8) < 128) {
		LOAD_16(color, offset + (localX >> 8) * 2 + (localY >> 8) * 320, renderer->d.vram);
		color = mColorFrom555(color);
	}

	int outX;
	uint32_t* pixel;
	for (outX = renderer->start, pixel = &renderer->row[outX]; outX < renderer->end; ++outX, ++pixel) {
		BACKGROUND_BITMAP_ITERATE(160, 128);

		if (!mosaicWait) {
			LOAD_16(color, offset + (localX >> 8) * 2 + (localY >> 8) * 320, renderer->d.vram);
			color = mColorFrom555(color);
			mosaicWait = mosaicH;
		} else {
			--mosaicWait;
		}

		uint32_t current = *pixel;
		if (!objwinSlowPath || (!(current & FLAG_OBJWIN)) != objwinOnly) {
			unsigned mergedFlags = flags;
			if (current & FLAG_OBJWIN) {
				mergedFlags = objwinFlags;
			}
			if (!variant) {
				_compositeBlendObjwin(renderer, pixel, color | mergedFlags, current);
			} else if (renderer->blendEffect == BLEND_BRIGHTEN) {
				_compositeBlendObjwin(renderer, pixel, _brighten(color, renderer->bldy) | mergedFlags, current);
			} else if (renderer->blendEffect == BLEND_DARKEN) {
				_compositeBlendObjwin(renderer, pixel, _darken(color, renderer->bldy) | mergedFlags, current);
			}
		}
	}
}
