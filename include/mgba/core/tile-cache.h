/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_TILE_CACHE_H
#define M_TILE_CACHE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>

DECL_BITFIELD(mTileCacheConfiguration, uint32_t);
DECL_BIT(mTileCacheConfiguration, ShouldStore, 0);

DECL_BITFIELD(mTileCacheSystemInfo, uint32_t);
DECL_BITS(mTileCacheSystemInfo, PaletteBPP, 0, 2);
DECL_BITS(mTileCacheSystemInfo, PaletteCount, 2, 4);
DECL_BITS(mTileCacheSystemInfo, MaxTiles, 16, 13);

struct mTileCacheEntry {
	uint32_t paletteVersion;
	uint32_t vramVersion;
	uint8_t vramClean;
	uint8_t paletteId;
	uint16_t padding;
};

struct mTileCache {
	color_t* cache;
	struct mTileCacheEntry* status;
	uint32_t* globalPaletteVersion;

	uint32_t tileBase;
	uint32_t paletteBase;
	unsigned entriesPerTile;
	unsigned bpp;

	uint16_t* vram;
	color_t* palette;
	color_t temporaryTile[64];

	mTileCacheConfiguration config;
	mTileCacheSystemInfo sysConfig;
};

void mTileCacheInit(struct mTileCache* cache);
void mTileCacheDeinit(struct mTileCache* cache);
void mTileCacheConfigure(struct mTileCache* cache, mTileCacheConfiguration config);
void mTileCacheConfigureSystem(struct mTileCache* cache, mTileCacheSystemInfo config, uint32_t tileBase, uint32_t paletteBase);
void mTileCacheWriteVRAM(struct mTileCache* cache, uint32_t address);
void mTileCacheWritePalette(struct mTileCache* cache, uint32_t entry, color_t color);

const color_t* mTileCacheGetTile(struct mTileCache* cache, unsigned tileId, unsigned paletteId);
const color_t* mTileCacheGetTileIfDirty(struct mTileCache* cache, struct mTileCacheEntry* entry, unsigned tileId, unsigned paletteId);
const color_t* mTileCacheGetPalette(struct mTileCache* cache, unsigned paletteId);
const uint16_t* mTileCacheGetVRAM(struct mTileCache* cache, unsigned tileId);

CXX_GUARD_END

#endif
