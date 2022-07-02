/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/map-cache.h>

#include <mgba-util/memory.h>

void mMapCacheInit(struct mMapCache* cache) {
	// TODO: Reconfigurable cache for space savings
	cache->cache = NULL;
	cache->config = mMapCacheConfigurationFillShouldStore(0);
	cache->sysConfig = 0;
	cache->status = NULL;
}

static void _freeCache(struct mMapCache* cache) {
	size_t tiles = (1 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig)) * (1 << mMapCacheSystemInfoGetTilesHigh(cache->sysConfig));
	if (cache->cache) {
		mappedMemoryFree(cache->cache, 8 * 8 * sizeof(color_t) * tiles);
		cache->cache = NULL;
	}
	if (cache->status) {
		mappedMemoryFree(cache->status, tiles * sizeof(*cache->status));
		cache->status = NULL;
	}
}

static void _redoCacheSize(struct mMapCache* cache) {
	if (!mMapCacheConfigurationIsShouldStore(cache->config)) {
		return;
	}

	size_t tiles = mMapCacheTileCount(cache);
	cache->cache = anonymousMemoryMap(8 * 8 * sizeof(color_t) * tiles);
	cache->status = anonymousMemoryMap(tiles * sizeof(*cache->status));
}

void mMapCacheConfigure(struct mMapCache* cache, mMapCacheConfiguration config) {
	if (config == cache->config) {
		return;
	}
	_freeCache(cache);
	cache->config = config;
	_redoCacheSize(cache);
}

void mMapCacheConfigureSystem(struct mMapCache* cache, mMapCacheSystemInfo config) {
	if (config == cache->sysConfig) {
		return;
	}
	_freeCache(cache);
	cache->sysConfig = config;
	_redoCacheSize(cache);

	size_t mapSize = mMapCacheTileCount(cache);
	cache->mapSize = mapSize << mMapCacheSystemInfoGetMapAlign(cache->sysConfig);
}

void mMapCacheConfigureMap(struct mMapCache* cache, uint32_t mapStart) {
	size_t tiles = mMapCacheTileCount(cache);
	memset(cache->status, 0, tiles * sizeof(*cache->status));
	cache->mapStart = mapStart;
}

void mMapCacheDeinit(struct mMapCache* cache) {
	_freeCache(cache);
}

void mMapCacheWriteVRAM(struct mMapCache* cache, uint32_t address) {
	if (address >= cache->mapStart && address < cache->mapStart + cache->mapSize) {
		uint32_t align = 1 << (mMapCacheSystemInfoGetWriteAlign(cache->sysConfig) - mMapCacheSystemInfoGetMapAlign(cache->sysConfig));
		address -= cache->mapStart;
		address >>= mMapCacheSystemInfoGetMapAlign(cache->sysConfig);

		uint32_t i;
		for (i = 0; i < align; ++i) {
			if (address + i >= (cache->mapSize >> mMapCacheSystemInfoGetMapAlign(cache->sysConfig))) {
				break;
			}
			struct mMapCacheEntry* status = &cache->status[address + i];
			++status->vramVersion;
			status->flags = mMapCacheEntryFlagsClearVramClean(status->flags);
			status->tileStatus[mMapCacheEntryFlagsGetPaletteId(status->flags)].vramClean = 0;
		}
	}
}

static inline void _cleanTile(struct mMapCache* cache, const color_t* tile, color_t* mapOut, const struct mMapCacheEntry* status) {
	size_t stride = 8 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig);
	int x, y;
	switch (mMapCacheEntryFlagsGetMirror(status->flags)) {
	case 0:
		memcpy(mapOut, tile, sizeof(color_t) * 8);
		memcpy(&mapOut[stride], &tile[0x08], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 2], &tile[0x10], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 3], &tile[0x18], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 4], &tile[0x20], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 5], &tile[0x28], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 6], &tile[0x30], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 7], &tile[0x38], sizeof(color_t) * 8);
		break;
	case 1:
		for (y = 0; y < 8; ++y) {
			for (x = 0; x < 8; ++x) {
				mapOut[y * stride + (7 - x)] = tile[y * 8 + x];
			}
		}
		break;
	case 2:
		memcpy(&mapOut[stride * 7], tile, sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 6], &tile[0x08], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 5], &tile[0x10], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 4], &tile[0x18], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 3], &tile[0x20], sizeof(color_t) * 8);
		memcpy(&mapOut[stride * 2], &tile[0x28], sizeof(color_t) * 8);
		memcpy(&mapOut[stride], &tile[0x30], sizeof(color_t) * 8);
		memcpy(mapOut, &tile[0x38], sizeof(color_t) * 8);
		break;
	case 3:
		for (y = 0; y < 8; ++y) {
			for (x = 0; x < 8; ++x) {
				mapOut[(7 - y) * stride + (7 - x)] = tile[y * 8 + x];
			}
		}
		break;
	}
}

uint32_t mMapCacheTileId(struct mMapCache* cache, unsigned x, unsigned y) {
	int tilesWide = mMapCacheSystemInfoGetTilesWide(cache->sysConfig);
	int tilesHigh = mMapCacheSystemInfoGetTilesHigh(cache->sysConfig);
	int stride = 1 << mMapCacheSystemInfoGetMacroTileSize(cache->sysConfig);
	x &= (1 << tilesWide) - 1;
	y &= (1 << tilesHigh) - 1;
	unsigned xMajor = x & ~(stride - 1);
	unsigned yMajor = y >> mMapCacheSystemInfoGetMacroTileSize(cache->sysConfig);
	x &= stride - 1;
	y &= stride - 1;
	yMajor <<= tilesWide;
	y += xMajor + yMajor;
	return stride * y + x;
}

void mMapCacheCleanTile(struct mMapCache* cache, struct mMapCacheEntry* entry, unsigned x, unsigned y) {
	size_t location = mMapCacheTileId(cache, x, y);
	struct mMapCacheEntry* status = &cache->status[location];
	const color_t* tile = NULL;
	if (!mMapCacheEntryFlagsIsVramClean(status->flags)) {
		status->flags = mMapCacheEntryFlagsFillVramClean(status->flags);
		cache->mapParser(cache, status, &cache->vram[cache->mapStart + (location << mMapCacheSystemInfoGetMapAlign(cache->sysConfig))]);
	}
	unsigned tileId = status->tileId + cache->tileStart;
	if (tileId >= mTileCacheSystemInfoGetMaxTiles(cache->tileCache->sysConfig)) {
		tileId = 0;
	}
	tile = mTileCacheGetTileIfDirty(cache->tileCache, status->tileStatus, tileId, mMapCacheEntryFlagsGetPaletteId(status->flags));
	if (!tile) {
		if (mMapCacheEntryFlagsIsVramClean(status->flags) && memcmp(status, &entry[location], sizeof(*entry)) == 0) {
			return;
		}
		tile = mTileCacheGetTile(cache->tileCache, tileId, mMapCacheEntryFlagsGetPaletteId(status->flags));
	}

	size_t stride = 8 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig);
	color_t* mapOut = &cache->cache[(y * stride + x) * 8];
	_cleanTile(cache, tile, mapOut, status);
	entry[location] = *status;
}

bool mMapCacheCheckTile(struct mMapCache* cache, const struct mMapCacheEntry* entry, unsigned x, unsigned y) {
	size_t location = mMapCacheTileId(cache, x, y);
	struct mMapCacheEntry* status = &cache->status[location];
	int paletteId = mMapCacheEntryFlagsGetPaletteId(status->flags);
	const color_t* tile = NULL;
	if (mMapCacheEntryFlagsIsVramClean(status->flags) && memcmp(status, &entry[location], sizeof(*entry)) == 0) {
		unsigned tileId = status->tileId + cache->tileStart;
		if (tileId >= mTileCacheSystemInfoGetMaxTiles(cache->tileCache->sysConfig)) {
			tileId = 0;
		}
		tile = mTileCacheGetTileIfDirty(cache->tileCache, &status->tileStatus[paletteId], tileId, mMapCacheEntryFlagsGetPaletteId(status->flags));
		return !tile;
	}
	return false;
}

void mMapCacheCleanRow(struct mMapCache* cache, unsigned y) {
	// TODO: Cache
	int tilesWide = 1 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig);
	int macroTile = (1 << mMapCacheSystemInfoGetMacroTileSize(cache->sysConfig)) - 1;
	size_t stride = 8 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig);
	int location = 0;
	int x;
	for (x = 0; x < tilesWide; ++x) {
		if (!(x & macroTile)) {
			location = mMapCacheTileId(cache, x, y);
		} else {
			++location;
		}
		struct mMapCacheEntry* status = &cache->status[location];
		if (!mMapCacheEntryFlagsIsVramClean(status->flags)) {
			status->flags = mMapCacheEntryFlagsFillVramClean(status->flags);
			cache->mapParser(cache, status, &cache->vram[cache->mapStart + (location << mMapCacheSystemInfoGetMapAlign(cache->sysConfig))]);
		}
		unsigned tileId = status->tileId + cache->tileStart;
		if (tileId >= mTileCacheSystemInfoGetMaxTiles(cache->tileCache->sysConfig)) {
			tileId = 0;
		}
		const color_t* tile = mTileCacheGetTile(cache->tileCache, tileId, mMapCacheEntryFlagsGetPaletteId(status->flags));
		color_t* mapOut = &cache->cache[(y * stride + x) * 8];
		_cleanTile(cache, tile, mapOut, status);
	}
}

const color_t* mMapCacheGetRow(struct mMapCache* cache, unsigned y) {
	size_t stride = 8 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig);
	return &cache->cache[y * stride];
}
