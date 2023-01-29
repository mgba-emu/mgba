/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/renderers/software-private.h"

#define SPRITE_NORMAL_LOOP(DEPTH, TYPE) \
	SPRITE_YBASE_ ## DEPTH(inY); \
	unsigned tileData; \
	for (; outX < condition; ++outX, inX += xOffset) { \
		SPRITE_XBASE_ ## DEPTH(inX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(inX); \
	}

#define SPRITE_MOSAIC_LOOP(DEPTH, TYPE) \
	SPRITE_YBASE_ ## DEPTH(inY); \
	unsigned tileData; \
	for (; outX < condition; ++outX, inX += xOffset) { \
		int localX = inX - xOffset * (outX % mosaicH); \
		if (localX < 0) { \
			localX = 0; \
		} else if (localX > width - 1) {\
			localX = width - 1; \
		} \
		SPRITE_XBASE_ ## DEPTH(localX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(localX); \
	}

#define SPRITE_TRANSFORMED_LOOP(DEPTH, TYPE) \
	unsigned tileData; \
	unsigned widthMask = ~(width - 1); \
	unsigned heightMask = ~(height - 1); \
	for (; outX < condition; ++outX, ++inX) { \
		xAccum += mat.a; \
		yAccum += mat.c; \
		int localX = xAccum >> 8; \
		int localY = yAccum >> 8; \
		\
		if (localX & widthMask || localY & heightMask) { \
			break; \
		} \
		\
		SPRITE_YBASE_ ## DEPTH(localY); \
		SPRITE_XBASE_ ## DEPTH(localX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(localX); \
	}

#define SPRITE_TRANSFORMED_MOSAIC_LOOP(DEPTH, TYPE) \
	unsigned tileData; \
	unsigned widthMask = ~(width - 1); \
	unsigned heightMask = ~(height - 1); \
	int localX = xAccum >> 8; \
	int localY = yAccum >> 8; \
	for (; outX < condition; ++outX, ++inX) { \
		xAccum += mat.a; \
		yAccum += mat.c; \
		\
		if (outX % mosaicH == 0) { \
			localX = xAccum >> 8; \
			localY = yAccum >> 8; \
		} \
		\
		if (localX & widthMask || localY & heightMask) { \
			continue; \
		} \
		\
		SPRITE_YBASE_ ## DEPTH(localY); \
		SPRITE_XBASE_ ## DEPTH(localX); \
		SPRITE_DRAW_PIXEL_ ## DEPTH ## _ ## TYPE(localX); \
	}

#define SPRITE_XBASE_16(localX) unsigned xBase = (localX & ~0x7) * 4 + ((localX >> 1) & 2);
#define SPRITE_YBASE_16(localY) unsigned yBase = (localY & ~0x7) * stride + (localY & 0x7) * 4 + maskHi;

#define SPRITE_DRAW_PIXEL_16_NORMAL(localX) \
	LOAD_16(tileData, (yBase + ((xBase + charBase) & maskLo)) & 0x7FFE, vramBase); \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			renderer->spriteLayer[outX] = palette[tileData] | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~(FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)) | (flags & (FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)); \
		} \
	}

#define SPRITE_DRAW_PIXEL_16_NORMAL_OBJWIN(localX) \
	LOAD_16(tileData, (yBase + ((xBase + charBase) & maskLo)) & 0x7FFE, vramBase); \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			unsigned color = (renderer->row[outX] & FLAG_OBJWIN) ? objwinPalette[tileData] : palette[tileData]; \
			renderer->spriteLayer[outX] = color | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~(FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)) | (flags & (FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)); \
		} \
	}

#define SPRITE_DRAW_PIXEL_16_OBJWIN(localX) \
	LOAD_16(tileData, (yBase + ((xBase + charBase) & maskLo)) & 0x7FFE, vramBase); \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

#define SPRITE_XBASE_256(localX) unsigned xBase = (localX & ~0x7) * 8 + (localX & 6);
#define SPRITE_YBASE_256(localY) unsigned yBase = (localY & ~0x7) * stride + (localY & 0x7) * 8 + maskHi;

#define SPRITE_DRAW_PIXEL_256_NORMAL(localX) \
	LOAD_16(tileData, (yBase + ((xBase + charBase) & maskLo)) & 0x7FFE, vramBase); \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			renderer->spriteLayer[outX] = palette[tileData] | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~(FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)) | (flags & (FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)); \
		} \
	}

#define SPRITE_DRAW_PIXEL_256_NORMAL_OBJWIN(localX) \
	LOAD_16(tileData, (yBase + ((xBase + charBase) & maskLo)) & 0x7FFE, vramBase); \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			unsigned color = (renderer->row[outX] & FLAG_OBJWIN) ? objwinPalette[tileData] : palette[tileData]; \
			renderer->spriteLayer[outX] = color | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~(FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)) | (flags & (FLAG_ORDER_MASK | FLAG_REBLEND | FLAG_TARGET_1)); \
		} \
	}

#define SPRITE_DRAW_PIXEL_256_OBJWIN(localX) \
	LOAD_16(tileData, (yBase + ((xBase + charBase) & maskLo)) & 0x7FFE, vramBase); \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

int GBAVideoSoftwareRendererPreprocessSprite(struct GBAVideoSoftwareRenderer* renderer, struct GBAObj* sprite, int index, int y) {
	int width = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][0];
	int height = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][1];
	int start = renderer->start;
	int end = renderer->end;
	uint32_t flags = GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY;
	flags |= FLAG_TARGET_1 * ((GBAWindowControlIsBlendEnable(renderer->currentWindow.packed) && renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT);
	flags |= FLAG_OBJWIN * (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_OBJWIN);
	if ((flags & FLAG_OBJWIN) && (renderer->currentWindow.priority < renderer->objwin.priority || renderer->d.disableOBJWIN)) {
		return 0;
	}
	int32_t x = (uint32_t) GBAObjAttributesBGetX(sprite->b) << 23;
	x >>= 23;
	x += renderer->objOffsetX;
	uint16_t* vramBase = &renderer->d.vram[BASE_TILE >> 1];
	unsigned align = GBAObjAttributesAIs256Color(sprite->a) && !GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt);
	unsigned charBase = (GBAObjAttributesCGetTile(sprite->c) & ~align) * 0x20;
	unsigned maskLo = GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? 0x7FFE : 0x3FE;
	unsigned maskHi = GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? 0 : charBase & 0x7C00;
	if (GBARegisterDISPCNTGetMode(renderer->dispcnt) >= 3 && GBAObjAttributesCGetTile(sprite->c) < 512) {
		return 0;
	}

	int objwinSlowPath = GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt) && GBAWindowControlGetBlendEnable(renderer->objwin.packed) != GBAWindowControlIsBlendEnable(renderer->currentWindow.packed);
	int variant = renderer->target1Obj &&
	              GBAWindowControlIsBlendEnable(renderer->currentWindow.packed) &&
	              (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT || (renderer->target1Obj && renderer->blendEffect == BLEND_ALPHA) || objwinSlowPath) {
		int target2 = renderer->target2Bd;
		target2 |= renderer->bg[0].target2 && renderer->bg[0].enabled;
		target2 |= renderer->bg[1].target2 && renderer->bg[1].enabled;
		target2 |= renderer->bg[2].target2 && renderer->bg[2].enabled;
		target2 |= renderer->bg[3].target2 && renderer->bg[3].enabled;
		if (target2) {
			renderer->forceTarget1 = true;
			flags |= FLAG_REBLEND;
			variant = 0;
		} else {
			flags &= ~FLAG_TARGET_1;
		}
	}

	color_t* palette = &renderer->normalPalette[0x100];
	if (renderer->d.highlightAmount && renderer->d.highlightOBJ[index]) {
		palette = &renderer->highlightPalette[0x100];
	}
	color_t* objwinPalette = palette;

	if (variant) {
		palette = &renderer->variantPalette[0x100];
		if (renderer->d.highlightAmount && renderer->d.highlightOBJ[index]) {
			palette = &renderer->highlightVariantPalette[0x100];
		}
		if (GBAWindowControlIsBlendEnable(renderer->objwin.packed)) {
			objwinPalette = palette;
		}
	}

	int inY = y - ((int) GBAObjAttributesAGetY(sprite->a) + renderer->objOffsetY);
	int stride = GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? (width >> !GBAObjAttributesAIs256Color(sprite->a)) : 0x80;

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
		int condition = x + totalWidth;
		int inX = outX - x;
		if (end < condition) {
			condition = end;
		}
		int mosaicH = 1;
		if (GBAObjAttributesAIsMosaic(sprite->a)) {
			mosaicH = GBAMosaicControlGetObjH(renderer->mosaic) + 1;
			if (condition != end && condition % mosaicH) {
				condition += mosaicH - (condition % mosaicH);
			}
		}

		int xAccum = mat.a * (inX - 1 - (totalWidth >> 1)) + mat.b * (inY - (totalHeight >> 1)) + (width << 7);
		int yAccum = mat.c * (inX - 1 - (totalWidth >> 1)) + mat.d * (inY - (totalHeight >> 1)) + (height << 7);

		// Clip off early pixels
		// TODO: Transform end coordinates too
		if (mat.a) {
			if ((xAccum >> 8) < 0) {
				int32_t diffX = -xAccum - 1;
				int32_t x = mat.a ? diffX / mat.a : 0;
				xAccum += mat.a * x;
				yAccum += mat.c * x;
				outX += x;
				inX += x;
			} else if ((xAccum >> 8) >= width) {
				int32_t diffX = (width << 8) - xAccum;
				int32_t x = mat.a ? diffX / mat.a : 0;
				xAccum += mat.a * x;
				yAccum += mat.c * x;
				outX += x;
				inX += x;
			}
		}
		if (mat.c) {
			if ((yAccum >> 8) < 0) {
				int32_t diffY = - yAccum - 1;
				int32_t y = mat.c ? diffY / mat.c : 0;
				xAccum += mat.a * y;
				yAccum += mat.c * y;
				outX += y;
				inX += y;
			} else if ((yAccum >> 8) >= height) {
				int32_t diffY = (height << 8) - yAccum;
				int32_t y = mat.c ? diffY / mat.c : 0;
				xAccum += mat.a * y;
				yAccum += mat.c * y;
				outX += y;
				inX += y;
			}
		}

		if (outX < start || outX >= condition) {
			return 0;
		}

		if (!GBAObjAttributesAIs256Color(sprite->a)) {
			palette = &palette[GBAObjAttributesCGetPalette(sprite->c) << 4];
			if (flags & FLAG_OBJWIN) {
				SPRITE_TRANSFORMED_LOOP(16, OBJWIN);
			} else if (mosaicH > 1) {
				if (objwinSlowPath) {
					objwinPalette = &objwinPalette[GBAObjAttributesCGetPalette(sprite->c) << 4];
					SPRITE_TRANSFORMED_MOSAIC_LOOP(16, NORMAL_OBJWIN);
				} else {
					SPRITE_TRANSFORMED_MOSAIC_LOOP(16, NORMAL);
				}
			} else if (objwinSlowPath) {
				objwinPalette = &objwinPalette[GBAObjAttributesCGetPalette(sprite->c) << 4];
				SPRITE_TRANSFORMED_LOOP(16, NORMAL_OBJWIN);
			} else {
				SPRITE_TRANSFORMED_LOOP(16, NORMAL);
			}
		} else {
			if (flags & FLAG_OBJWIN) {
				SPRITE_TRANSFORMED_LOOP(256, OBJWIN);
			} else if (mosaicH > 1) {
				if (objwinSlowPath) {
					SPRITE_TRANSFORMED_MOSAIC_LOOP(256, NORMAL_OBJWIN);
				} else {
					SPRITE_TRANSFORMED_MOSAIC_LOOP(256, NORMAL);
				}
			} else if (objwinSlowPath) {
				SPRITE_TRANSFORMED_LOOP(256, NORMAL_OBJWIN);
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
			} else if (mosaicH > 1) {
				if (objwinSlowPath) {
					objwinPalette = &objwinPalette[GBAObjAttributesCGetPalette(sprite->c) << 4];
					SPRITE_MOSAIC_LOOP(16, NORMAL_OBJWIN);
				} else {
					SPRITE_MOSAIC_LOOP(16, NORMAL);
				}
			} else if (objwinSlowPath) {
				objwinPalette = &objwinPalette[GBAObjAttributesCGetPalette(sprite->c) << 4];
				SPRITE_NORMAL_LOOP(16, NORMAL_OBJWIN);
			} else {
				SPRITE_NORMAL_LOOP(16, NORMAL);
			}
		} else {
			if (flags & FLAG_OBJWIN) {
				SPRITE_NORMAL_LOOP(256, OBJWIN);
			} else if (mosaicH > 1) {
				if (objwinSlowPath) {
					SPRITE_MOSAIC_LOOP(256, NORMAL_OBJWIN);
				} else {
					SPRITE_MOSAIC_LOOP(256, NORMAL);
				}
			} else if (objwinSlowPath) {
				SPRITE_NORMAL_LOOP(256, NORMAL_OBJWIN);
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
