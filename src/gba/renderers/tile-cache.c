/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "tile-cache.h"

#include "gba/video.h"
#include "util/memory.h"

#define CACHE_SIZE (8 * 8 * 2 * 1024 * 3 * 16)

void GBAVideoTileCacheInit(struct GBAVideoTileCache* cache) {
	// TODO: Reconfigurable cache for space savings
	cache->cache = anonymousMemoryMap(CACHE_SIZE);
	memset(cache->status, 0, sizeof(cache->status));
	memset(cache->globalPaletteVersion, 0, sizeof(cache->globalPaletteVersion));
	memset(cache->globalPalette256Version, 0, sizeof(cache->globalPalette256Version));
}

void GBAVideoTileCacheDeinit(struct GBAVideoTileCache* cache) {
	mappedMemoryFree(cache->cache, CACHE_SIZE);
	cache->cache = NULL;
}

void GBAVideoTileCacheAssociate(struct GBAVideoTileCache* cache, struct GBAVideo* video) {
	cache->vram = video->vram;
	cache->palette = video->palette;
	video->renderer->cache = cache;
}

void GBAVideoTileCacheWriteVRAM(struct GBAVideoTileCache* cache, uint32_t address) {
	cache->status[address >> 5].vramClean = 0;
}

void GBAVideoTileCacheWritePalette(struct GBAVideoTileCache* cache, uint32_t address) {
	++cache->globalPaletteVersion[address >> 5];
	++cache->globalPalette256Version[address >> 9];
}

static void _regenerateTile16(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId) {
	uint16_t* tile = &cache->cache[tileId << 6];
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

static void _regenerateTile256(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId) {
	uint16_t* tile = &cache->cache[tileId << 6];
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

const uint16_t* GBAVideoTileCacheGetTile16(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId) {
	struct GBAVideoTileCacheEntry* status = &cache->status[tileId];
	if (!status->vramClean || status->palette256 || status->paletteVersion[paletteId & 0xF] != cache->globalPaletteVersion[paletteId]) {
		_regenerateTile16(cache, tileId, paletteId);
		status->paletteVersion[paletteId & 0xF] = cache->globalPaletteVersion[paletteId];
		status->palette256 = 0;
		status->vramClean = 1;
	}
	return &cache->cache[tileId << 6];
}

const uint16_t* GBAVideoTileCacheGetTile16IfDirty(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId) {
	struct GBAVideoTileCacheEntry* status = &cache->status[tileId];
	if (!status->vramClean || status->palette256 || status->paletteVersion[paletteId & 0xF] != cache->globalPaletteVersion[paletteId]) {
		_regenerateTile16(cache, tileId, paletteId);
		status->paletteVersion[paletteId & 0xF] = cache->globalPaletteVersion[paletteId];
		status->palette256 = 0;
		status->vramClean = 1;
		return &cache->cache[tileId << 6];
	}
	return NULL;
}


const uint16_t* GBAVideoTileCacheGetTile256(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId) {
	struct GBAVideoTileCacheEntry* status = &cache->status[tileId];
	if (!status->vramClean || !status->palette256 || status->paletteVersion[0] != cache->globalPalette256Version[paletteId]) {
		_regenerateTile256(cache, tileId, paletteId);
		status->paletteVersion[0] = cache->globalPalette256Version[paletteId];
		status->palette256 = 1;
		status->vramClean = 1;
	}
	return &cache->cache[tileId << 6];
}

const uint16_t* GBAVideoTileCacheGetTile256IfDirty(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId) {
	struct GBAVideoTileCacheEntry* status = &cache->status[tileId];
	if (!status->vramClean || !status->palette256 || status->paletteVersion[0] != cache->globalPalette256Version[paletteId]) {
		_regenerateTile256(cache, tileId, paletteId);
		status->paletteVersion[0] = cache->globalPalette256Version[paletteId];
		status->palette256 = 1;
		status->vramClean = 1;
		return &cache->cache[tileId << 6];
	}
	return NULL;
}
