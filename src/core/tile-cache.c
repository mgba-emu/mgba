/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/tile-cache.h>

#include <mgba-util/memory.h>

void mTileCacheInit(struct mTileCache* cache) {
	// TODO: Reconfigurable cache for space savings
	cache->cache = NULL;
	cache->config = mTileCacheConfigurationFillShouldStore(0);
	cache->status = NULL;
	cache->activePalette = 0;
	memset(cache->globalPaletteVersion, 0, sizeof(cache->globalPaletteVersion));
}

static void _freeCache(struct mTileCache* cache) {
	unsigned count0;
	count0 = 1 << mTileCacheSystemInfoGetPalette0Count(cache->sysConfig);
	unsigned count1;
	count1 = 1 << mTileCacheSystemInfoGetPalette1Count(cache->sysConfig);
	unsigned tiles = mTileCacheSystemInfoGetMaxTiles(cache->sysConfig);
	unsigned size = count0 > count1 ? count0 : count1;
	if (cache->cache) {
		mappedMemoryFree(cache->cache, 8 * 8 * 2 * tiles * size);
		cache->cache = NULL;
	}
	if (cache->status) {
		mappedMemoryFree(cache->status, tiles * size * sizeof(*cache->status));
		cache->status = NULL;
	}
	free(cache->globalPaletteVersion[0]);
	free(cache->globalPaletteVersion[1]);
	memset(cache->globalPaletteVersion, 0, sizeof(cache->globalPaletteVersion));
}

static void _redoCacheSize(struct mTileCache* cache) {
	if (!mTileCacheConfigurationIsShouldStore(cache->config)) {
		return;
	}
	unsigned count0 = mTileCacheSystemInfoGetPalette0Count(cache->sysConfig);
	unsigned bpp0 = mTileCacheSystemInfoGetPalette0BPP(cache->sysConfig);
	bpp0 = 1 << (1 << bpp0);
	if (count0) {
		count0 = 1 << count0;
	}
	unsigned count1 = mTileCacheSystemInfoGetPalette1Count(cache->sysConfig);
	unsigned bpp1 = mTileCacheSystemInfoGetPalette1BPP(cache->sysConfig);
	bpp1 = 1 << (1 << bpp1);
	if (count1) {
		count1 = 1 << count1;
	}
	unsigned size = count0 > count1 ? count0 : count1;
	if (!size) {
		return;
	}
	cache->entriesPerTile = size;
	unsigned tiles = mTileCacheSystemInfoGetMaxTiles(cache->sysConfig);
	cache->cache = anonymousMemoryMap(8 * 8 * 2 * tiles * size);
	cache->status = anonymousMemoryMap(tiles * size * sizeof(*cache->status));
	if (count0) {
		cache->globalPaletteVersion[0] = malloc(count0 * bpp0 * sizeof(*cache->globalPaletteVersion[0]));
	}
	if (count1) {
		cache->globalPaletteVersion[1] = malloc(count1 * bpp1 * sizeof(*cache->globalPaletteVersion[1]));
	}
}

void mTileCacheConfigure(struct mTileCache* cache, mTileCacheConfiguration config) {
	_freeCache(cache);
	cache->config = config;
	_redoCacheSize(cache);
}

void mTileCacheConfigureSystem(struct mTileCache* cache, mTileCacheSystemInfo config) {
	_freeCache(cache);
	cache->sysConfig = config;
	_redoCacheSize(cache);
}

void mTileCacheDeinit(struct mTileCache* cache) {
	_freeCache(cache);
}

void mTileCacheWriteVRAM(struct mTileCache* cache, uint32_t address) {
	unsigned bpp = cache->bpp + 3;
	unsigned count = cache->entriesPerTile;
	size_t i;
	for (i = 0; i < count; ++i) {
		cache->status[(address >> bpp) * count + i].vramClean = 0;
		++cache->status[(address >> bpp) * count + i].vramVersion;
	}
}

void mTileCacheWritePalette(struct mTileCache* cache, uint32_t address) {
	if (cache->globalPaletteVersion[0]) {
		++cache->globalPaletteVersion[0][address >> 1];
	}
	if (cache->globalPaletteVersion[1]) {
		++cache->globalPaletteVersion[1][address >> 1];
	}
}

void mTileCacheSetPalette(struct mTileCache* cache, int palette) {
	cache->activePalette = palette;
	if (palette == 0) {
		cache->bpp = mTileCacheSystemInfoGetPalette0BPP(cache->sysConfig);
		cache->count = 1 << mTileCacheSystemInfoGetPalette0Count(cache->sysConfig);
	} else {
		cache->bpp = mTileCacheSystemInfoGetPalette1BPP(cache->sysConfig);
		cache->count = 1 << mTileCacheSystemInfoGetPalette1Count(cache->sysConfig);
	}
	cache->entries = 1 << (1 << cache->bpp);
}

static void _regenerateTile4(struct mTileCache* cache, uint16_t* tile, unsigned tileId, unsigned paletteId) {
	uint8_t* start = (uint8_t*) &cache->vram[tileId << 3];
	paletteId <<= 2;
	uint16_t* palette = &cache->palette[paletteId];
	int i;
	for (i = 0; i < 8; ++i) {
		uint8_t tileDataLower = start[0];
		uint8_t tileDataUpper = start[1];
		start += 2;
		int pixel;
		pixel = ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
		tile[0] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
		tile[1] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
		tile[2] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
		tile[3] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
		tile[4] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
		tile[5] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
		tile[6] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		tile[7] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		tile += 8;
	}
}

static void _regenerateTile16(struct mTileCache* cache, uint16_t* tile, unsigned tileId, unsigned paletteId) {
	uint32_t* start = (uint32_t*) &cache->vram[tileId << 4];
	paletteId <<= 4;
	uint16_t* palette = &cache->palette[paletteId];
	int i;
	for (i = 0; i < 8; ++i) {
		uint32_t line = *start;
		++start;
		int pixel;
		pixel = line & 0xF;
		tile[0] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 4) & 0xF;
		tile[1] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 8) & 0xF;
		tile[2] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 12) & 0xF;
		tile[3] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 16) & 0xF;
		tile[4] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 20) & 0xF;
		tile[5] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 24) & 0xF;
		tile[6] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 28) & 0xF;
		tile[7] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		tile += 8;
	}
}

static void _regenerateTile256(struct mTileCache* cache, uint16_t* tile, unsigned tileId, unsigned paletteId) {
	uint32_t* start = (uint32_t*) &cache->vram[tileId << 5];
	paletteId <<= 8;
	uint16_t* palette = &cache->palette[paletteId];
	int i;
	for (i = 0; i < 8; ++i) {
		uint32_t line = *start;
		++start;
		int pixel;
		pixel = line & 0xFF;
		tile[0] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 8) & 0xFF;
		tile[1] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 16) & 0xFF;
		tile[2] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 24) & 0xFF;
		tile[3] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;

		line = *start;
		++start;
		pixel = line & 0xFF;
		tile[4] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 8) & 0xFF;
		tile[5] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 16) & 0xFF;
		tile[6] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		pixel = (line >> 24) & 0xFF;
		tile[7] = pixel ? palette[pixel] | 0x8000 : palette[pixel] & 0x7FFF;
		tile += 8;
	}
}

static inline uint16_t* _tileLookup(struct mTileCache* cache, unsigned tileId, unsigned paletteId) {
	if (mTileCacheConfigurationIsShouldStore(cache->config)) {
		unsigned tiles = mTileCacheSystemInfoGetMaxTiles(cache->sysConfig);
		return &cache->cache[(tileId + paletteId * tiles) << 6];
	} else {
		return cache->temporaryTile;
	}
}

const uint16_t* mTileCacheGetTile(struct mTileCache* cache, unsigned tileId, unsigned paletteId) {
	unsigned cPaletteId = cache->activePalette;
	unsigned count = cache->entriesPerTile;
	unsigned bpp = cache->bpp;
	struct mTileCacheEntry* status = &cache->status[tileId * count + paletteId];
	struct mTileCacheEntry desiredStatus = {
		.paletteVersion = cache->globalPaletteVersion[cPaletteId][paletteId],
		.vramVersion = status->vramVersion,
		.vramClean = 1,
		.paletteId = paletteId,
		.activePalette = cPaletteId
	};
	uint16_t* tile = _tileLookup(cache, tileId, paletteId);
	if (!mTileCacheConfigurationIsShouldStore(cache->config) || memcmp(status, &desiredStatus, sizeof(*status))) {
		switch (bpp) {
		case 0:
			return NULL;
		case 1:
			_regenerateTile4(cache, tile, tileId, paletteId);
			break;
		case 2:
			_regenerateTile16(cache, tile, tileId, paletteId);
			break;
		case 3:
			_regenerateTile256(cache, tile, tileId, paletteId);
			break;
		}
		*status = desiredStatus;
	}
	return tile;
}

const uint16_t* mTileCacheGetTileIfDirty(struct mTileCache* cache, struct mTileCacheEntry* entry, unsigned tileId, unsigned paletteId) {
	unsigned cPaletteId = cache->activePalette;
	unsigned count = cache->entriesPerTile;
	unsigned bpp = cache->bpp;
	struct mTileCacheEntry* status = &cache->status[tileId * count + paletteId];
	struct mTileCacheEntry desiredStatus = {
		.paletteVersion = cache->globalPaletteVersion[cPaletteId][paletteId],
		.vramVersion = status->vramVersion,
		.vramClean = 1,
		.paletteId = paletteId,
		.activePalette = cPaletteId
	};
	uint16_t* tile = NULL;
	if (memcmp(status, &desiredStatus, sizeof(*status))) {
		tile = _tileLookup(cache, tileId, paletteId);
		switch (bpp) {
		case 0:
			return NULL;
		case 1:
			_regenerateTile4(cache, tile, tileId, paletteId);
			break;
		case 2:
			_regenerateTile16(cache, tile, tileId, paletteId);
			break;
		case 3:
			_regenerateTile256(cache, tile, tileId, paletteId);
			break;
		}
		*status = desiredStatus;
	}
	if (memcmp(status, &entry[paletteId], sizeof(*status))) {
		tile = _tileLookup(cache, tileId, paletteId);
		entry[paletteId] = *status;
	}
	return tile;
}

const uint8_t* mTileCacheGetRawTile(struct mTileCache* cache, unsigned tileId) {
	unsigned bpp = cache->bpp;
	switch (bpp) {
	case 0:
		return NULL;
	default:
		return (uint8_t*) &cache->vram[tileId << (2 + bpp)];
	}
}

const uint16_t* mTileCacheGetPalette(struct mTileCache* cache, unsigned paletteId) {
	unsigned bpp = cache->bpp;
	switch (bpp) {
	default:
		return NULL;
	case 1:
		return &cache->palette[paletteId << 2];
	case 2:
		return &cache->palette[paletteId << 4];
	case 3:
		return &cache->palette[paletteId << 8];
	}
}
