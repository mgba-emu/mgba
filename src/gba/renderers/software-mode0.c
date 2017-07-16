/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/renderers/software-private.h"

#include <mgba/internal/gba/gba.h>

#define BACKGROUND_TEXT_SELECT_CHARACTER \
	localX = tileX * 8 + inX; \
	xBase = localX & 0xF8; \
	if (background->size & 1) { \
		xBase += (localX & 0x100) << 5; \
	} \
	screenBase = background->screenBase + yBase + (xBase >> 2); \
	uint16_t* screenBlock = renderer->d.vramBG[screenBase >> VRAM_BLOCK_OFFSET]; \
	LOAD_16(mapData, screenBase & VRAM_BLOCK_MASK, screenBlock); \
	localY = inY & 0x7; \
	if (GBA_TEXT_MAP_VFLIP(mapData)) { \
		localY = 7 - localY; \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_SUFFIX_16(BLEND, OBJWIN) \
	paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
	palette = &mainPalette[paletteData]; \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
	vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
	LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		tileData >>= 4 * mod8; \
		for (; outX < end; ++outX) { \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 0); \
		} \
	} else { \
		for (outX = end - 1; outX >= renderer->start; --outX) { \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 0); \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_PREFIX_16(BLEND, OBJWIN) \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
	vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
	LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
	paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
	palette = &mainPalette[paletteData]; \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		if (outX < renderer->start) { \
			tileData >>= 4 * (renderer->start - outX); \
			outX = renderer->start; \
		} \
		for (; outX < renderer->end; ++outX) { \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 0); \
		} \
	} else { \
		tileData >>= 4 * (0x8 - mod8); \
		int end = renderer->end - 8; \
		if (end < -1) { \
			end = -1; \
		} \
		outX = renderer->end - 1; \
		for (; outX > end; --outX) { \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 0); \
		} \
		/* Needed for consistency checks */ \
		if (VIDEO_CHECKS) { \
			outX = renderer->end; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_MOSAIC_16(BLEND, OBJWIN) \
	x = inX & 7; \
	if (mosaicWait) { \
		int baseX = x - (mosaicH - mosaicWait); \
		if (baseX < 0) { \
			int disturbX = (16 + baseX) >> 3; \
			inX -= disturbX << 3; \
			BACKGROUND_TEXT_SELECT_CHARACTER; \
			baseX -= disturbX << 3; \
			inX += disturbX << 3; \
		} else { \
			BACKGROUND_TEXT_SELECT_CHARACTER; \
		} \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
		vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
		paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
		palette = &mainPalette[paletteData]; \
		LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
		if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
			tileData >>= 4 * baseX; \
		} else { \
			tileData >>= 4 * (7 - baseX); \
		} \
		tileData &= 0xF; \
		tileData |= tileData << 4; \
		tileData |= tileData << 8; \
		tileData |= tileData << 16; \
		carryData = tileData; \
	} \
	for (; length; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
		vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
		tileData = carryData; \
		for (; x < 8 && length; ++x, --length) { \
			if (!mosaicWait) { \
				paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
				palette = &mainPalette[paletteData]; \
				LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
				if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
					tileData >>= x * 4; \
				} else { \
					tileData >>= (7 - x) * 4; \
				} \
				tileData &= 0xF; \
				tileData |= tileData << 4; \
				tileData |= tileData << 8; \
				tileData |= tileData << 16; \
				carryData = tileData; \
				mosaicWait = mosaicH; \
			} \
			--mosaicWait; \
			BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 0); \
			++outX; \
		} \
		x = 0; \
	}

#define DRAW_BACKGROUND_MODE_0_TILES_16(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 4; \
		palette = &mainPalette[paletteData]; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 5)) + (localY << 2); \
		vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
		LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
		if (tileData) { \
			if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 0); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 1); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 2); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 3); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 4); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 5); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 6); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 7); \
			} else { \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 7); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 6); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 5); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 4); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 3); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 2); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 1); \
				BACKGROUND_DRAW_PIXEL_16(BLEND, OBJWIN, 0); \
			} \
		} \
		outX += 8; \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_SUFFIX_256(BLEND, OBJWIN) \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
	vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
	int end2 = end - 4; \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		int shift = inX & 0x3; \
		if (end2 > outX) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			tileData >>= 8 * shift; \
			shift = 0; \
			for (; outX < end2; ++outX) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
		} \
		\
		LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
		tileData >>= 8 * shift; \
		for (; outX < end; ++outX) { \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
		} \
	} else { \
		int start = outX; \
		outX = end - 1; \
		if (end2 > start) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			for (; outX >= end2; --outX) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			charBase += 4; \
		} \
		\
		LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
		for (; outX >= renderer->start; --outX) { \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_PREFIX_256(BLEND, OBJWIN) \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
	vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
	int end = mod8 - 4; \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		if (end > 0) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			for (; outX < renderer->end - end; ++outX) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			charBase += 4; \
		} \
		\
		LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
		for (; outX < renderer->end; ++outX) { \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
		} \
	} else { \
		int shift = (8 - mod8) & 0x3; \
		int start = outX; \
		outX = renderer->end - 1; \
		if (end > 0) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			tileData >>= 8 * shift; \
			for (; outX >= start + 4; --outX) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			shift = 0; \
		} \
		\
		LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
		tileData >>= 8 * shift; \
		for (; outX >= start; --outX) { \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
		} \
		/* Needed for consistency checks */ \
		if (VIDEO_CHECKS) { \
			outX = renderer->end; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILES_256(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
		vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
		if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			if (tileData) { \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 1); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 2); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 3); \
			} \
			outX += 4; \
			LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
			if (tileData) { \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 1); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 2); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 3); \
			} \
			outX += 4; \
		} else { \
			LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
			if (tileData) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 3); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 2); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 1); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			outX += 4; \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			if (tileData) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 3); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 2); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 1); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			outX += 4; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_MOSAIC_256(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
		vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
		tileData = carryData; \
		for (x = 0; x < 8; ++x) { \
			if (!mosaicWait) { \
				if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
					if (x >= 4) { \
						LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
						tileData >>= (x - 4) * 8; \
					} else { \
						LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
						tileData >>= x * 8; \
					} \
				} else { \
					if (x >= 4) { \
						LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
						tileData >>= (7 - x) * 8; \
					} else { \
						LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
						tileData >>= (3 - x) * 8; \
					} \
				} \
				tileData &= 0xFF; \
				carryData = tileData; \
				mosaicWait = mosaicH; \
			} \
			tileData |= tileData << 8; \
			--mosaicWait; \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			++outX; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_SUFFIX_256EXT(BLEND, OBJWIN) \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
	vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
	int end2 = end - 4; \
	paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 8; \
	palette = &mainPalette[paletteData]; \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		int shift = inX & 0x3; \
		if (end2 > outX) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			tileData >>= 8 * shift; \
			shift = 0; \
			for (; outX < end2; ++outX) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
		} \
		\
		LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
		tileData >>= 8 * shift; \
		for (; outX < end; ++outX) { \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
		} \
	} else { \
		int start = outX; \
		outX = end - 1; \
		if (end2 > start) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			for (; outX >= end2; --outX) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			charBase += 4; \
		} \
		\
		LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
		for (; outX >= renderer->start; --outX) { \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILE_PREFIX_256EXT(BLEND, OBJWIN) \
	charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
	vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
	paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 8; \
	palette = &mainPalette[paletteData]; \
	int end = mod8 - 4; \
	if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
		if (end > 0) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			for (; outX < renderer->end - end; ++outX) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			charBase += 4; \
		} \
		\
		LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
		for (; outX < renderer->end; ++outX) { \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
		} \
	} else { \
		int shift = (8 - mod8) & 0x3; \
		int start = outX; \
		outX = renderer->end - 1; \
		if (end > 0) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			tileData >>= 8 * shift; \
			for (; outX >= start + 4; --outX) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			shift = 0; \
		} \
		\
		LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
		tileData >>= 8 * shift; \
		for (; outX >= start; --outX) { \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
		} \
		/* Needed for consistency checks */ \
		if (VIDEO_CHECKS) { \
			outX = renderer->end; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_TILES_256EXT(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 8; \
		palette = &mainPalette[paletteData]; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
		vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
		if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			if (tileData) { \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 1); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 2); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 3); \
			} \
			outX += 4; \
			LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
			if (tileData) { \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 1); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 2); \
					BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 3); \
			} \
			outX += 4; \
		} else { \
			LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
			if (tileData) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 3); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 2); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 1); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			outX += 4; \
			LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
			if (tileData) { \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 3); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 2); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 1); \
				BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			} \
			outX += 4; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0_MOSAIC_256EXT(BLEND, OBJWIN) \
	for (; tileX < tileEnd; ++tileX) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		charBase = (background->charBase + (GBA_TEXT_MAP_TILE(mapData) << 6)) + (localY << 3); \
		vram = renderer->d.vramBG[charBase >> VRAM_BLOCK_OFFSET]; \
		tileData = carryData; \
		for (x = 0; x < 8; ++x) { \
			if (!mosaicWait) { \
				paletteData = GBA_TEXT_MAP_PALETTE(mapData) << 8; \
				palette = &mainPalette[paletteData]; \
				if (!GBA_TEXT_MAP_HFLIP(mapData)) { \
					if (x >= 4) { \
						LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
						tileData >>= (x - 4) * 8; \
					} else { \
						LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
						tileData >>= x * 8; \
					} \
				} else { \
					if (x >= 4) { \
						LOAD_32(tileData, charBase & VRAM_BLOCK_MASK, vram); \
						tileData >>= (7 - x) * 8; \
					} else { \
						LOAD_32(tileData, (charBase + 4) & VRAM_BLOCK_MASK, vram); \
						tileData >>= (3 - x) * 8; \
					} \
				} \
				tileData &= 0xFF; \
				carryData = tileData; \
				mosaicWait = mosaicH; \
			} \
			tileData |= tileData << 8; \
			--mosaicWait; \
			BACKGROUND_DRAW_PIXEL_256(BLEND, OBJWIN, 0); \
			++outX; \
		} \
	}

#define DRAW_BACKGROUND_MODE_0(BPP, BLEND, OBJWIN) \
	if (background->mosaic && GBAMosaicControlGetBgH(renderer->mosaic)) { \
		int mosaicH = GBAMosaicControlGetBgH(renderer->mosaic) + 1; \
		int x; \
		int mosaicWait = (mosaicH - outX + renderer->masterEnd * mosaicH) % mosaicH; \
		int carryData = 0; \
		paletteData = 0; /* Quiets compiler warning */ \
		DRAW_BACKGROUND_MODE_0_MOSAIC_ ## BPP (BLEND, OBJWIN) \
		return; \
	} \
	\
	if (inX & 0x7) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		\
		int mod8 = inX & 0x7; \
		int end = outX + 0x8 - mod8; \
		if (end > renderer->end) { \
			end = renderer->end; \
		} \
		if (UNLIKELY(end == outX)) { \
			return; \
		} \
		if (UNLIKELY(end < outX)) { \
			mLOG(GBA_VIDEO, FATAL, "Out of bounds background draw!"); \
			return; \
		} \
		DRAW_BACKGROUND_MODE_0_TILE_SUFFIX_ ## BPP (BLEND, OBJWIN) \
		outX = end; \
		if (tileX < tileEnd) { \
			++tileX; \
		} else if (VIDEO_CHECKS && UNLIKELY(tileX > tileEnd)) { \
			mLOG(GBA_VIDEO, FATAL, "Invariant doesn't hold in background draw! tileX (%u) > tileEnd (%u)", tileX, tileEnd); \
			return; \
		} \
		length -= end - renderer->start; \
	} \
	DRAW_BACKGROUND_MODE_0_TILES_ ## BPP (BLEND, OBJWIN) \
	if (length & 0x7) { \
		BACKGROUND_TEXT_SELECT_CHARACTER; \
		\
		int mod8 = length & 0x7; \
		if (VIDEO_CHECKS && UNLIKELY(outX + mod8 != renderer->end)) { \
			mLOG(GBA_VIDEO, FATAL, "Invariant doesn't hold in background draw!"); \
			return; \
		} \
		DRAW_BACKGROUND_MODE_0_TILE_PREFIX_ ## BPP (BLEND, OBJWIN) \
	} \
	if (VIDEO_CHECKS && UNLIKELY(outX > renderer->masterEnd)) { \
		mLOG(GBA_VIDEO, FATAL, "Out of bounds background draw occurred!"); \
		return; \
	}

void GBAVideoSoftwareRendererDrawBackgroundMode0(struct GBAVideoSoftwareRenderer* renderer, struct GBAVideoSoftwareBackground* background, int y) {
	int inX = renderer->start + background->x;
	int length = renderer->end - renderer->start;
	if (background->mosaic) {
		int mosaicV = GBAMosaicControlGetBgV(renderer->mosaic) + 1;
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
	yBase <<= 3;

	int localX;
	int localY;

	unsigned xBase;

	uint32_t flags = (background->priority << OFFSET_PRIORITY) | (background->index << OFFSET_INDEX) | FLAG_IS_BACKGROUND;
	flags |= FLAG_TARGET_2 * background->target2;
	int objwinFlags = FLAG_TARGET_1 * (background->target1 && renderer->blendEffect == BLEND_ALPHA && GBAWindowControlIsBlendEnable(renderer->objwin.packed));
	objwinFlags |= flags;
	flags |= FLAG_TARGET_1 * (background->target1 && renderer->blendEffect == BLEND_ALPHA && GBAWindowControlIsBlendEnable(renderer->currentWindow.packed));
	if (renderer->blendEffect == BLEND_ALPHA && renderer->blda == 0x10 && renderer->bldb == 0) {
		flags &= ~(FLAG_TARGET_1 | FLAG_TARGET_2);
		objwinFlags &= ~(FLAG_TARGET_1 | FLAG_TARGET_2);
	}

	uint32_t screenBase;
	uint32_t charBase;
	int variant = background->target1 && GBAWindowControlIsBlendEnable(renderer->currentWindow.packed) && (renderer->blendEffect == BLEND_BRIGHTEN || renderer->blendEffect == BLEND_DARKEN);
	color_t* mainPalette;
	if (background->multipalette && background->extPalette) {
		mainPalette = background->extPalette;
		if (variant) {
			mainPalette = background->variantPalette;
		}
	} else {
		mainPalette = renderer->normalPalette;
		if (variant) {
			mainPalette = renderer->variantPalette;
		}
	}
	color_t* palette = mainPalette;
	PREPARE_OBJWIN;

	int outX = renderer->start;

	uint32_t tileData;
	uint32_t current;
	int pixelData;
	int paletteData;
	int tileX = 0;
	int tileEnd = ((length + inX) >> 3) - (inX >> 3);
	uint16_t* vram = NULL;

	if (!objwinSlowPath) {
		if (!(flags & FLAG_TARGET_2)) {
			if (!background->multipalette) {
				DRAW_BACKGROUND_MODE_0(16, NoBlend, NO_OBJWIN);
			} else if (!background->extPalette) {
				DRAW_BACKGROUND_MODE_0(256, NoBlend, NO_OBJWIN);
			} else {
				DRAW_BACKGROUND_MODE_0(256EXT, NoBlend, NO_OBJWIN);
			}
		} else {
			if (!background->multipalette) {
				DRAW_BACKGROUND_MODE_0(16, Blend, NO_OBJWIN);
			} else if (!background->extPalette) {
				DRAW_BACKGROUND_MODE_0(256, Blend, NO_OBJWIN);
			} else {
				DRAW_BACKGROUND_MODE_0(256EXT, Blend, NO_OBJWIN);
			}
		}
	} else {
		if (!(flags & FLAG_TARGET_2)) {
			if (!background->multipalette) {
				DRAW_BACKGROUND_MODE_0(16, NoBlend, OBJWIN);
			} else if (!background->extPalette) {
				DRAW_BACKGROUND_MODE_0(256, NoBlend, OBJWIN);
			} else {
				DRAW_BACKGROUND_MODE_0(256EXT, NoBlend, OBJWIN);
			}
		} else {
			if (!background->multipalette) {
				DRAW_BACKGROUND_MODE_0(16, Blend, OBJWIN);
			} else if (!background->extPalette) {
				DRAW_BACKGROUND_MODE_0(256, Blend, OBJWIN);
			} else {
				DRAW_BACKGROUND_MODE_0(256EXT, Blend, OBJWIN);
			}
		}
	}
}
