/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_MAP_CACHE_H
#define M_MAP_CACHE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>
#include <mgba/core/tile-cache.h>

DECL_BITFIELD(mMapCacheConfiguration, uint32_t);
DECL_BIT(mMapCacheConfiguration, ShouldStore, 0);

DECL_BITFIELD(mMapCacheSystemInfo, uint32_t);
DECL_BITS(mMapCacheSystemInfo, PaletteBPP, 0, 2);
DECL_BITS(mMapCacheSystemInfo, PaletteCount, 2, 4);
DECL_BITS(mMapCacheSystemInfo, TilesWide, 8, 4);
DECL_BITS(mMapCacheSystemInfo, TilesHigh, 12, 4);
DECL_BITS(mMapCacheSystemInfo, MacroTileSize, 16, 7);
DECL_BITS(mMapCacheSystemInfo, MapAlign, 23, 2);
DECL_BITS(mMapCacheSystemInfo, WriteAlign, 25, 2);

DECL_BITFIELD(mMapCacheEntryFlags, uint16_t);
DECL_BITS(mMapCacheEntryFlags, PaletteId, 0, 4);
DECL_BIT(mMapCacheEntryFlags, VramClean, 4);
DECL_BIT(mMapCacheEntryFlags, HMirror, 5);
DECL_BIT(mMapCacheEntryFlags, VMirror, 6);
DECL_BITS(mMapCacheEntryFlags, Mirror, 5, 2);

#define mMapCacheTileCount(C) (1 << mMapCacheSystemInfoGetTilesWide((C)->sysConfig)) * (1 << mMapCacheSystemInfoGetTilesHigh((C)->sysConfig))

struct mMapCacheEntry {
	uint32_t vramVersion;
	uint16_t tileId;
	mMapCacheEntryFlags flags;
	struct mTileCacheEntry tileStatus[16];
};

struct mTileCache;
struct mTileCacheEntry;
struct mMapCache {
	color_t* cache;
	struct mTileCache* tileCache;
	struct mMapCacheEntry* status;

	uint8_t* vram;

	uint32_t mapStart;
	uint32_t mapSize;

	uint32_t tileStart;

	mMapCacheConfiguration config;
	mMapCacheSystemInfo sysConfig;

	void (*mapParser)(struct mMapCache*, struct mMapCacheEntry* entry, void* vram);
	void* context;
};

void mMapCacheInit(struct mMapCache* cache);
void mMapCacheDeinit(struct mMapCache* cache);
void mMapCacheConfigure(struct mMapCache* cache, mMapCacheConfiguration config);
void mMapCacheConfigureSystem(struct mMapCache* cache, mMapCacheSystemInfo config);
void mMapCacheConfigureMap(struct mMapCache* cache, uint32_t mapStart);
void mMapCacheWriteVRAM(struct mMapCache* cache, uint32_t address);

uint32_t mMapCacheTileId(struct mMapCache* cache, unsigned x, unsigned y);

bool mMapCacheCheckTile(struct mMapCache* cache, const struct mMapCacheEntry* entry, unsigned x, unsigned y);
void mMapCacheCleanTile(struct mMapCache* cache, struct mMapCacheEntry* entry, unsigned x, unsigned y);

void mMapCacheCleanRow(struct mMapCache* cache, unsigned y);
const color_t* mMapCacheGetRow(struct mMapCache* cache, unsigned y);

CXX_GUARD_END

#endif
