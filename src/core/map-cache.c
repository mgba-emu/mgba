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
	cache->status = NULL;
}

static void _freeCache(struct mMapCache* cache) {
	size_t tiles = (1 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig)) * (1 << mMapCacheSystemInfoGetTilesHigh(cache->sysConfig));
	mappedMemoryFree(cache->cache, 8 * 8 * sizeof(color_t) * tiles);
	mappedMemoryFree(cache->status, tiles * sizeof(*cache->status));
	cache->cache = NULL;
	cache->status = NULL;
}

static void _redoCacheSize(struct mMapCache* cache) {
	if (!mMapCacheConfigurationIsShouldStore(cache->config)) {
		return;
	}

	size_t tiles = (1 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig)) * (1 << mMapCacheSystemInfoGetTilesHigh(cache->sysConfig));
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

	size_t mapSize = (1 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig)) * (1 << mMapCacheSystemInfoGetTilesHigh(cache->sysConfig));
	cache->mapSize = mapSize << mMapCacheSystemInfoGetMapAlign(cache->sysConfig);
}

void mMapCacheConfigureMap(struct mMapCache* cache, uint32_t mapStart) {
	size_t tiles = (1 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig)) * (1 << mMapCacheSystemInfoGetTilesHigh(cache->sysConfig));
	memset(cache->status, 0, tiles * sizeof(*cache->status));
	cache->mapStart = mapStart;
}

void mMapCacheDeinit(struct mMapCache* cache) {
	_freeCache(cache);
}

void mMapCacheWriteVRAM(struct mMapCache* cache, uint32_t address) {
	if (address >= cache->mapStart && address < cache->mapStart + cache->mapSize) {
		address -= cache->mapStart;
		struct mMapCacheEntry* status = &cache->status[address >> mMapCacheSystemInfoGetMapAlign(cache->sysConfig)];
		++status->vramVersion;
		status->flags = mMapCacheEntryFlagsClearVramClean(status->flags);
		status->tileStatus[mMapCacheEntryFlagsGetPaletteId(status->flags)].vramClean = 0;
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

void mMapCacheCleanTile(struct mMapCache* cache, struct mMapCacheEntry* entry, unsigned x, unsigned y) {
	size_t location = (1 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig)) * y + x;
	struct mMapCacheEntry* status = &cache->status[location];
	int paletteId = mMapCacheEntryFlagsGetPaletteId(status->flags);
	const color_t* tile = NULL;
	if (!mMapCacheEntryFlagsIsVramClean(status->flags)) {
		status->flags = mMapCacheEntryFlagsFillVramClean(status->flags);
		cache->mapParser(cache, status, &cache->vram[cache->mapStart + (location << mMapCacheSystemInfoGetMapAlign(cache->sysConfig))]);
		tile = mTileCacheGetTileIfDirty(cache->tileCache, &status->tileStatus[paletteId], status->tileId + cache->tileStart, mMapCacheEntryFlagsGetPaletteId(status->flags));
		if (!tile) {
			tile = mTileCacheGetTile(cache->tileCache, status->tileId + cache->tileStart, mMapCacheEntryFlagsGetPaletteId(status->flags));
		}
	} else {
		tile = mTileCacheGetTileIfDirty(cache->tileCache, &status->tileStatus[paletteId], status->tileId + cache->tileStart, mMapCacheEntryFlagsGetPaletteId(status->flags));
		if (!tile && memcmp(status, &entry[location], sizeof(*entry)) == 0) {
			return;
		}
		tile = mTileCacheGetTile(cache->tileCache, status->tileId + cache->tileStart, mMapCacheEntryFlagsGetPaletteId(status->flags));
	}

	size_t stride = 8 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig);
	color_t* mapOut = &cache->cache[(y * stride + x) * 8];
	_cleanTile(cache, tile, mapOut, status);
	entry[location] = *status;
}

bool mMapCacheCheckTile(struct mMapCache* cache, const struct mMapCacheEntry* entry, unsigned x, unsigned y) {
	size_t location = (1 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig)) * y + x;
	struct mMapCacheEntry* status = &cache->status[location];
	int paletteId = mMapCacheEntryFlagsGetPaletteId(status->flags);
	const color_t* tile = NULL;
	if (mMapCacheEntryFlagsIsVramClean(status->flags) && memcmp(status, &entry[location], sizeof(*entry)) == 0) {
		tile = mTileCacheGetTileIfDirty(cache->tileCache, &status->tileStatus[paletteId], status->tileId + cache->tileStart, mMapCacheEntryFlagsGetPaletteId(status->flags));
		return !tile;
	}
	return false;
}

const color_t* mMapCacheGetRow(struct mMapCache* cache, unsigned y) {
	size_t stride = 8 << mMapCacheSystemInfoGetTilesWide(cache->sysConfig);
	return &cache->cache[y * stride];
}
