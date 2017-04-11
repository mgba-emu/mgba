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
		if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
			continue; \
		} \
		renderer->spriteCyclesRemaining -= 1; \
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
	unsigned widthMask = ~(width - 1); \
	unsigned heightMask = ~(height - 1); \
	for (; outX < condition; ++outX, ++inX) { \
		if (!(renderer->row[outX] & FLAG_UNWRITTEN)) { \
			continue; \
		} \
		renderer->spriteCyclesRemaining -= 2; \
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

#define SPRITE_XBASE_16(localX) unsigned xBase = (localX & ~0x7) * 4 + ((localX >> 1) & 2);
#define SPRITE_YBASE_16(localY) unsigned yBase = (localY & ~0x7) * stride + (localY & 0x7) * 4;

#define SPRITE_DRAW_PIXEL_16_NORMAL(localX) \
	uint32_t spriteBase = ((yBase + charBase + xBase) & 0x3FFFE); \
	uint16_t* vramBase = renderer->d.vramOBJ[spriteBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vramBase)) { \
		return 0; \
	} \
	LOAD_16(tileData, spriteBase & VRAM_BLOCK_MASK, vramBase); \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			renderer->spriteLayer[outX] = palette[tileData] | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~FLAG_ORDER_MASK) | GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY; \
		} \
	}

#define SPRITE_DRAW_PIXEL_16_NORMAL_OBJWIN(localX) \
	uint32_t spriteBase = ((yBase + charBase + xBase) & 0x3FFFE); \
	uint16_t* vramBase = renderer->d.vramOBJ[spriteBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vramBase)) { \
		return 0; \
	} \
	LOAD_16(tileData, spriteBase & VRAM_BLOCK_MASK, vramBase); \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			unsigned color = (renderer->row[outX] & FLAG_OBJWIN) ? objwinPalette[tileData] : palette[tileData]; \
			renderer->spriteLayer[outX] = color | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~FLAG_ORDER_MASK) | GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY; \
		} \
	}

#define SPRITE_DRAW_PIXEL_16_OBJWIN(localX) \
	uint32_t spriteBase = ((yBase + charBase + xBase) & 0x3FFFE); \
	uint16_t* vramBase = renderer->d.vramOBJ[spriteBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vramBase)) { \
		return 0; \
	} \
	LOAD_16(tileData, spriteBase & VRAM_BLOCK_MASK, vramBase); \
	tileData = (tileData >> ((localX & 3) << 2)) & 0xF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

#define SPRITE_XBASE_256(localX) unsigned xBase = (localX & ~0x7) * 8 + (localX & 6);
#define SPRITE_YBASE_256(localY) unsigned yBase = (localY & ~0x7) * stride + (localY & 0x7) * 8;

#define SPRITE_DRAW_PIXEL_256_NORMAL(localX) \
	uint32_t spriteBase = ((yBase + charBase + xBase) & 0x3FFFE); \
	uint16_t* vramBase = renderer->d.vramOBJ[spriteBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vramBase)) { \
		return 0; \
	} \
	LOAD_16(tileData, spriteBase & VRAM_BLOCK_MASK, vramBase); \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			renderer->spriteLayer[outX] = palette[tileData] | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~FLAG_ORDER_MASK) | GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY; \
		} \
	}

#define SPRITE_DRAW_PIXEL_256_NORMAL_OBJWIN(localX) \
	uint32_t spriteBase = ((yBase + charBase + xBase) & 0x3FFFE); \
	uint16_t* vramBase = renderer->d.vramOBJ[spriteBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vramBase)) { \
		return 0; \
	} \
	LOAD_16(tileData, spriteBase & VRAM_BLOCK_MASK, vramBase); \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData) { \
			unsigned color = (renderer->row[outX] & FLAG_OBJWIN) ? objwinPalette[tileData] : palette[tileData]; \
			renderer->spriteLayer[outX] = color | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~FLAG_ORDER_MASK) | GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY; \
		} \
	}

#define SPRITE_DRAW_PIXEL_256_OBJWIN(localX) \
	uint32_t spriteBase = ((yBase + charBase + xBase) & 0x3FFFE); \
	uint16_t* vramBase = renderer->d.vramOBJ[spriteBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vramBase)) { \
		return 0; \
	} \
	LOAD_16(tileData, spriteBase & VRAM_BLOCK_MASK, vramBase); \
	tileData = (tileData >> ((localX & 1) << 3)) & 0xFF; \
	if (tileData) { \
		renderer->row[outX] |= FLAG_OBJWIN; \
	}

#ifndef COLOR_16_BIT
#define TILE_TO_COLOR(tileData) \
	unsigned color32; \
	color32 = 0; \
	color32 |= (tileData << 3) & 0xF8; \
	color32 |= (tileData << 6) & 0xF800; \
	color32 |= (tileData << 9) & 0xF80000; \
	color32 |= (color32 >> 5) & 0x070707; \
	color = color32;
#elif COLOR_5_6_5
#define TILE_TO_COLOR(tileData) \
	uint16_t color16 = 0; \
	color16 |= (tileData & 0x001F) << 11; \
	color16 |= (tileData & 0x03E0) << 1; \
	color16 |= (tileData & 0x7C00) >> 10; \
	color = color16;
#else
#define TILE_TO_COLOR(tileData) \
	color = tileData;
#endif

#define SPRITE_XBASE_BITMAP(localX) unsigned xBase = (localX & (stride - 1)) << 1;
#define SPRITE_YBASE_BITMAP(localY) unsigned yBase = localY * (stride << 1);

#define SPRITE_DRAW_PIXEL_BITMAP_NORMAL(localX) \
	uint32_t spriteBase = ((yBase + charBase + xBase) & 0x3FFFE); \
	uint16_t* vramBase = renderer->d.vramOBJ[spriteBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vramBase)) { \
		return 0; \
	} \
	LOAD_16(tileData, spriteBase & VRAM_BLOCK_MASK, vramBase); \
	current = renderer->spriteLayer[outX]; \
	if ((current & FLAG_ORDER_MASK) > flags) { \
		if (tileData & 0x8000) { \
			uint32_t color; \
			TILE_TO_COLOR(tileData); \
			renderer->spriteLayer[outX] = color | flags; \
		} else if (current != FLAG_UNWRITTEN) { \
			renderer->spriteLayer[outX] = (current & ~FLAG_ORDER_MASK) | GBAObjAttributesCGetPriority(sprite->c) << OFFSET_PRIORITY; \
		} \
	}

#define SPRITE_DRAW_PIXEL_BITMAP_NORMAL_OBJWIN(localX) SPRITE_DRAW_PIXEL_BITMAP_NORMAL(localX)

#define SPRITE_DRAW_PIXEL_BITMAP_OBJWIN(localX) \
	uint32_t spriteBase = ((yBase + charBase + xBase) & 0x3FFFE); \
	uint16_t* vramBase = renderer->d.vramOBJ[spriteBase >> VRAM_BLOCK_OFFSET]; \
	if (UNLIKELY(!vramBase)) { \
		return 0; \
	} \
	LOAD_16(tileData, spriteBase & VRAM_BLOCK_MASK, vramBase); \
	if (tileData & 0x8000) { \
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
	if ((flags & FLAG_OBJWIN) && renderer->currentWindow.priority < renderer->objwin.priority) {
		return 0;
	}
	int32_t x = (uint32_t) GBAObjAttributesBGetX(sprite->b) << 23;
	x >>= 23;
	unsigned charBase = GBAObjAttributesCGetTile(sprite->c);
	if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_BITMAP) {
		charBase = (charBase & 0x1F) * 0x10 + (charBase & ~0x1F) * 0x80;
	} else {
		charBase *= renderer->tileStride;
	}
	if (!renderer->d.vramOBJ[charBase >> VRAM_BLOCK_OFFSET]) {
		return 0;
	}
	if (renderer->spriteCyclesRemaining <= 0) {
		return 0;
	}

	int variant = renderer->target1Obj &&
	              GBAWindowControlIsBlendEnable(renderer->currentWindow.packed) &&
	              (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT) {
		int target2 = renderer->target2Bd << 4;
		target2 |= renderer->bg[0].target2 << (renderer->bg[0].priority);
		target2 |= renderer->bg[1].target2 << (renderer->bg[1].priority);
		target2 |= renderer->bg[2].target2 << (renderer->bg[2].priority);
		target2 |= renderer->bg[3].target2 << (renderer->bg[3].priority);
		if ((1 << GBAObjAttributesCGetPriority(sprite->c)) <= target2) {
			variant = 0;
		}
	}
	color_t* palette = &renderer->normalPalette[0x100];
	color_t* objwinPalette = palette;
	int objwinSlowPath = GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt) && GBAWindowControlGetBlendEnable(renderer->objwin.packed) != GBAWindowControlIsBlendEnable(renderer->currentWindow.packed);

	if (GBAObjAttributesAIs256Color(sprite->a) && renderer->objExtPalette) {
		if (!variant) {
			palette = renderer->objExtPalette;
			objwinPalette = palette;
		} else {
			palette = renderer->objExtVariantPalette;
			if (GBAWindowControlIsBlendEnable(renderer->objwin.packed)) {
				objwinPalette = palette;
			}
		}
	} else if (variant) {
		palette = &renderer->variantPalette[0x100];
		if (GBAWindowControlIsBlendEnable(renderer->objwin.packed)) {
			objwinPalette = palette;
		}
	}

	int inY = y - (int) GBAObjAttributesAGetY(sprite->a);
	int stride = GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? (width >> !GBAObjAttributesAIs256Color(sprite->a)) : 0x80;
	if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_BITMAP) {
		stride = 0x100; // TODO: Param
	}

	uint32_t current;
	if (GBAObjAttributesAIsTransformed(sprite->a)) {
		int totalWidth = width << GBAObjAttributesAGetDoubleSize(sprite->a);
		int totalHeight = height << GBAObjAttributesAGetDoubleSize(sprite->a);
		renderer->spriteCyclesRemaining -= 10;
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

		if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_BITMAP) {
			int alpha = GBAObjAttributesCGetPalette(sprite->c);
			if (flags & FLAG_OBJWIN) {
				SPRITE_TRANSFORMED_LOOP(BITMAP, OBJWIN);
			} else if (objwinSlowPath) {
				SPRITE_TRANSFORMED_LOOP(BITMAP, NORMAL_OBJWIN);
			} else {
				SPRITE_TRANSFORMED_LOOP(BITMAP, NORMAL);
			}
		} else if (!GBAObjAttributesAIs256Color(sprite->a)) {
			palette = &palette[GBAObjAttributesCGetPalette(sprite->c) << 4];
			if (flags & FLAG_OBJWIN) {
				SPRITE_TRANSFORMED_LOOP(16, OBJWIN);
			} else if (objwinSlowPath) {
				objwinPalette = &objwinPalette[GBAObjAttributesCGetPalette(sprite->c) << 4];
				SPRITE_TRANSFORMED_LOOP(16, NORMAL_OBJWIN);
			} else {
				SPRITE_TRANSFORMED_LOOP(16, NORMAL);
			}
		} else if (!renderer->objExtPalette) {
			if (flags & FLAG_OBJWIN) {
				SPRITE_TRANSFORMED_LOOP(256, OBJWIN);
			} else if (objwinSlowPath) {
				SPRITE_TRANSFORMED_LOOP(256, NORMAL_OBJWIN);
			} else {
				SPRITE_TRANSFORMED_LOOP(256, NORMAL);
			}
		} else {
			palette = &palette[GBAObjAttributesCGetPalette(sprite->c) << 8];
			if (flags & FLAG_OBJWIN) {
				SPRITE_TRANSFORMED_LOOP(256, OBJWIN);
			} else if (objwinSlowPath) {
				objwinPalette = &objwinPalette[GBAObjAttributesCGetPalette(sprite->c) << 8];
				SPRITE_TRANSFORMED_LOOP(256, NORMAL_OBJWIN);
			} else {
				SPRITE_TRANSFORMED_LOOP(256, NORMAL);
			}
		}
		if (x + totalWidth > renderer->masterEnd) {
			renderer->spriteCyclesRemaining -= (x + totalWidth - renderer->masterEnd) * 2;
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
		if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_BITMAP) {
			int alpha = GBAObjAttributesCGetPalette(sprite->c);
			if (flags & FLAG_OBJWIN) {
				SPRITE_NORMAL_LOOP(BITMAP, OBJWIN);
			} else if (mosaicH > 1) {
				if (objwinSlowPath) {
					SPRITE_MOSAIC_LOOP(BITMAP, NORMAL_OBJWIN);
				} else {
					SPRITE_MOSAIC_LOOP(BITMAP, NORMAL);
				}
			} else if (objwinSlowPath) {
				SPRITE_NORMAL_LOOP(BITMAP, NORMAL_OBJWIN);
			} else {
				SPRITE_NORMAL_LOOP(BITMAP, NORMAL);
			}
		} else if (!GBAObjAttributesAIs256Color(sprite->a)) {
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
		} else if (!renderer->objExtPalette) {
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
		} else {
			palette = &palette[GBAObjAttributesCGetPalette(sprite->c) << 8];
			if (flags & FLAG_OBJWIN) {
				SPRITE_NORMAL_LOOP(256, OBJWIN);
			} else if (mosaicH > 1) {
				if (objwinSlowPath) {
					SPRITE_MOSAIC_LOOP(256, NORMAL_OBJWIN);
				} else {
					SPRITE_MOSAIC_LOOP(256, NORMAL);
				}
			} else if (objwinSlowPath) {
				objwinPalette = &objwinPalette[GBAObjAttributesCGetPalette(sprite->c) << 8];
				SPRITE_NORMAL_LOOP(256, NORMAL_OBJWIN);
			} else {
				SPRITE_NORMAL_LOOP(256, NORMAL);
			}

		}
		if (x + width > renderer->masterEnd) {
			renderer->spriteCyclesRemaining -= x + width - renderer->masterEnd;
		}
	}
	return 1;
}

void GBAVideoSoftwareRendererPostprocessSprite(struct GBAVideoSoftwareRenderer* renderer, unsigned priority) {
	int x;
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
			for (x = renderer->start; x < renderer->end; ++x) {
				uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
				uint32_t current = renderer->row[x];
				if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && !(current & FLAG_OBJWIN) && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
					_compositeBlendObjwin(renderer, x, color | flags, current);
				}
			}
			return;
		} else if (objwinOnly) {
			for (x = renderer->start; x < renderer->end; ++x) {
				uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
				uint32_t current = renderer->row[x];
				if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && (current & FLAG_OBJWIN) && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
					_compositeBlendObjwin(renderer, x, color | flags, current);
				}
			}
			return;
		} else {
			for (x = renderer->start; x < renderer->end; ++x) {
				uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
				uint32_t current = renderer->row[x];
				if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
					_compositeBlendObjwin(renderer, x, color | flags, current);
				}
			}
			return;
		}
	} else if (!GBAWindowControlIsObjEnable(renderer->currentWindow.packed)) {
		return;
	}
	for (x = renderer->start; x < renderer->end; ++x) {
		uint32_t color = renderer->spriteLayer[x] & ~FLAG_OBJWIN;
		uint32_t current = renderer->row[x];
		if ((color & FLAG_UNWRITTEN) != FLAG_UNWRITTEN && (color & FLAG_PRIORITY) >> OFFSET_PRIORITY == priority) {
			_compositeBlendNoObjwin(renderer, x, color | flags, current);
		}
	}
}
