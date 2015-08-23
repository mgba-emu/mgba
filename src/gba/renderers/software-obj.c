/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "software-private.h"

#define SPRITE_NORMAL_LOOP(DEPTH, TYPE) \
	SPRITE_YBASE_ ## DEPTH(inY); \
	unsigned tileData; \
	for (; outX < condition; ++outX, inX += xOffset) { \
		if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
			continue; \
		} \
		SPRITE_XBASE_ ## DEPTH(inX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(inX); \
	}

#define SPRITE_MOSAIC_LOOP(DEPTH, TYPE) \
	SPRITE_YBASE_ ## DEPTH(inY); \
	unsigned tileData; \
	if (outX % mosaicH) { \
		if (!inX && xOffset > 0) { \
			inX = mosaicH - (outX % mosaicH); \
			outX += mosaicH - (outX % mosaicH); \
		} else if (inX == width - xOffset) { \
			inX = mosaicH + (outX % mosaicH); \
			outX += mosaicH - (outX % mosaicH); \
		} \
	} \
	for (; outX < condition; ++outX, inX += xOffset) { \
		if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
			continue; \
		} \
		int localX = inX - xOffset * (outX % mosaicH); \
		if (localX < 0 || localX > width - 1) { \
			continue; \
		} \
		SPRITE_XBASE_ ## DEPTH(localX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(localX); \
	}

#define SPRITE_TRANSFORMED_LOOP(DEPTH, TYPE) \
	unsigned tileData; \
	for (; outX < x + totalWidth && outX < end; ++outX, ++inX) { \
		if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
			continue; \
		} \
		xAccum += mat.a; \
		yAccum += mat.c; \
		int localX = (xAccum >> 8) + (width >> 1); \
		int localY = (yAccum >> 8) + (height >> 1); \
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
	LOAD_16(tileData, ((yBase + charBase + xBase) & 0x7FFE), vramBase); \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			renderer->spriteLayer[outX] = palette[tileData] | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~FLAG_ORDER_MASK) | GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY; \
		} \
	}

#define SPRITE_DRAW_PIXEL_16_OBJWIN(localX) \
	LOAD_16(tileData, ((yBase + charBase + xBase) & 0x7FFE), vramBase); \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

#define SPRITE_XBASE_256(localX) unsigned xBase = (localX & ~0x7) * 8 + (localX & 6);
#define SPRITE_YBASE_256(localY) unsigned yBase = (localY & ~0x7) * (GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? width : 0x80) + (localY & 0x7) * 8;

#define SPRITE_DRAW_PIXEL_256_NORMAL(localX) \
	LOAD_16(tileData, ((yBase + charBase + xBase) & 0x7FFE), vramBase); \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			renderer->spriteLayer[outX] = palette[tileData] | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~FLAG_ORDER_MASK) | GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY; \
		} \
	}

#define SPRITE_DRAW_PIXEL_256_OBJWIN(localX) \
	LOAD_16(tileData, ((yBase + charBase + xBase) & 0x7FFE), vramBase); \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

int GBAVideoSoftwareRendererPreprocessSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int y) {
	int width = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][0];
	int height = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][1];
	int start = renderer->start;
	int end = renderer->end;
	uint32_t flags = GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY;
	flags |= FLAG_TARGET_1 * ((GBAWindowControlIsBlendEnable(renderer->currentWindow.packed) && renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT);
	flags |= FLAG_OBJWIN * (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_OBJWIN);
	int32_t x = GBAObjAttributesBGetX(sprite->b) << 23;
	x >>= 23;
	uint16_t* vramBase = &renderer->d.vram[BASE_TILE >> 1];
	unsigned charBase = GBAObjAttributesCGetTile(sprite->c) * 0x20;
	if (GBARegisterDISPCNTGetMode(renderer->dispcnt) >= 3 && GBAObjAttributesCGetTile(sprite->c) < 512) {
		return 0;
	}
	int variant = renderer->target1Obj && GBAWindowControlIsBlendEnable(renderer->currentWindow.packed) && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT) {
		int target2 = renderer->target2Bd << 4;
		target2 |= renderer->bg[0].target2 << (renderer->bg[0].priority);
		target2 |= renderer->bg[1].target2 << (renderer->bg[1].priority);
		target2 |= renderer->bg[2].target2 << (renderer->bg[2].priority);
		target2 |= renderer->bg[3].target2 << (renderer->bg[3].priority);
		if (GBAObjAttributesCGetPriority(sprite->c) < target2) {
			variant = 0;
		}
	}
	color_t* palette = &renderer->normalPalette[0x100];
	if (variant) {
		palette = &renderer->variantPalette[0x100];
	}

	int inY = y - (int) GBAObjAttributesAGetY(sprite->a);

	uint32_t current;
	if (GBAObjAttributesAIsTransformed(sprite->a)) {
		int totalWidth = width << GBAObjAttributesAGetDoubleSize(sprite->a);
		int totalHeight = height << GBAObjAttributesAGetDoubleSize(sprite->a);
		struct GBAOAMMatrix mat;
		LOAD_16(mat.a, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].a);
		LOAD_16(mat.b, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].b);
		LOAD_16(mat.c, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].c);
		LOAD_16(mat.d, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].d);

		if (inY < 0) {
			inY += 256;
		}
		int outX = x >= start ? x : start;
		int inX = outX - x;
		int xAccum = mat.a * (inX - 1 - (totalWidth >> 1)) + mat.b * (inY - (totalHeight >> 1));
		int yAccum = mat.c * (inX - 1 - (totalWidth >> 1)) + mat.d * (inY - (totalHeight >> 1));

		if (!GBAObjAttributesAIs256Color(sprite->a)) {
			palette = &palette[GBAObjAttributesCGetPalette(sprite->c) << 4];
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
	} else {
		int outX = x >= start ? x : start;
		int condition = x + width;
		int mosaicH = 1;
		if (GBAObjAttributesAIsMosaic(sprite->a)) {
			mosaicH = GBAMosaicControlGetObjH(renderer->mosaic) + 1;
			if (condition % mosaicH) {
				condition += mosaicH - (condition % mosaicH);
			}
		}
		if ((int) GBAObjAttributesAGetY(sprite->a) + height - 256 >= 0) {
			inY += 256;
		}
		if (GBAObjAttributesBIsVFlip(sprite->b)) {
			inY = height - inY - 1;
		}
		if (end < condition) {
			condition = end;
		}
		int inX = outX - x;
		int xOffset = 1;
		if (GBAObjAttributesBIsHFlip(sprite->b)) {
			inX = width - inX - 1;
			xOffset = -1;
		}
		if (!GBAObjAttributesAIs256Color(sprite->a)) {
			palette = &palette[GBAObjAttributesCGetPalette(sprite->c) << 4];
			if (flags & FLAG_OBJWIN) {
				SPRITE_NORMAL_LOOP(16, OBJWIN);
			} else if (GBAObjAttributesAIsMosaic(sprite->a)) {
				SPRITE_MOSAIC_LOOP(16, NORMAL);
			} else {
				SPRITE_NORMAL_LOOP(16, NORMAL);
			}
		} else {
			if (flags & FLAG_OBJWIN) {
				SPRITE_NORMAL_LOOP(256, OBJWIN);
			} else if (GBAObjAttributesAIsMosaic(sprite->a)) {
				SPRITE_MOSAIC_LOOP(256, NORMAL);
			} else {
				SPRITE_NORMAL_LOOP(256, NORMAL);
			}
		}
	}
	return 1;
}

void GBAVideoSoftwareRendererPostprocessSprite(struct GBAVideoSoftwareRenderer* renderer, unsigned priority) {
	int x;
	uint32_t* pixel = &renderer->row[renderer->start];
	uint32_t flags = FLAG_TARGET_2 * renderer->target2Obj;

	int objwinSlowPath = GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt);
	bool objwinDisable = false;
	bool objwinOnly = false;
	if (objwinSlowPath) {
		objwinDisable = !GBAWindowControlIsObjEnable(renderer->objwin.packed);
		objwinOnly = !objwinDisable && !GBAWindowControlIsObjEnable(renderer->currentWindow.packed);
		if (objwinDisable && !GBAWindowControlIsObjEnable(renderer->currentWindow.packed)) {
			return;
		}

		if (objwinDisable) {
			for (x = renderer->start; x < renderer->end; ++x, ++pixel) {
				uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
				uint32_t current = *pixel;
				if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && !(current & FLAG_OBJWIN) && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
					_compositeBlendObjwin(renderer, pixel, color | flags, current);
				}
			}
			return;
		} else if (objwinOnly) {
			for (x = renderer->start; x < renderer->end; ++x, ++pixel) {
				uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
				uint32_t current = *pixel;
				if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && (current & FLAG_OBJWIN) && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
					_compositeBlendObjwin(renderer, pixel, color | flags, current);
				}
			}
			return;
		} else {
			for (x = renderer->start; x < renderer->end; ++x, ++pixel) {
				uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
				uint32_t current = *pixel;
				if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
					_compositeBlendObjwin(renderer, pixel, color | flags, current);
				}
			}
			return;
		}
	} else if (!GBAWindowControlIsObjEnable(renderer->currentWindow.packed)) {
		return;
	}
	for (x = renderer->start; x < renderer->end; ++x, ++pixel) {
		uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
		uint32_t current = *pixel;
		if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
			_compositeBlendNoObjwin(renderer, pixel, color | flags, current);
		}
	}
}
