/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/renderers/cache-set.h>

#include <mgba/core/cache-set.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/video.h>

void GBVideoCacheInit(struct mCacheSet* cache) {
	mCacheSetInit(cache, 2, 0, 1);
	mTileCacheSystemInfo sysconfig = 0;
	mTileCacheConfiguration config = mTileCacheConfigurationFillShouldStore(0);
	sysconfig = mTileCacheSystemInfoSetPaletteBPP(sysconfig, 1); // 2^(2^1) = 4 entries
	sysconfig = mTileCacheSystemInfoSetPaletteCount(sysconfig, 4); // 16 palettes
	sysconfig = mTileCacheSystemInfoSetMaxTiles(sysconfig, 1024);
	mTileCacheConfigureSystem(mTileCacheSetGetPointer(&cache->tiles, 0), sysconfig, 0, 0);
	mTileCacheConfigure(mTileCacheSetGetPointer(&cache->tiles, 0), config);

	mMapCacheSetGetPointer(&cache->maps, 0)->tileCache = mTileCacheSetGetPointer(&cache->tiles, 0);
	mMapCacheSetGetPointer(&cache->maps, 1)->tileCache = mTileCacheSetGetPointer(&cache->tiles, 0);
}

void GBVideoCacheAssociate(struct mCacheSet* cache, struct GBVideo* video) {
	mCacheSetAssignVRAM(cache, video->vram);
	video->renderer->cache = cache;
	size_t i;
	for (i = 0; i < 64; ++i) {
		mCacheSetWritePalette(cache, i, mColorFrom555(video->palette[i]));
	}
	mMapCacheSystemInfo sysconfig = mMapCacheSystemInfoSetPaletteCount(0, 0);
	if (video->p->model >= GB_MODEL_CGB) {
		sysconfig = mMapCacheSystemInfoSetPaletteCount(0, 2);
	}
	mMapCacheConfigureSystem(mMapCacheSetGetPointer(&cache->maps, 0), sysconfig);
	mMapCacheConfigureSystem(mMapCacheSetGetPointer(&cache->maps, 1), sysconfig);

	GBVideoCacheWriteVideoRegister(cache, GB_REG_LCDC, video->p->memory.io[GB_REG_LCDC]);
}

static void mapParserDMG0(struct mMapCache* cache, struct mMapCacheEntry* entry, void* vram) {
	UNUSED(cache);
	int map = *(uint8_t*) vram;
	entry->tileId = map;
	entry->flags = mMapCacheEntryFlagsClearHMirror(entry->flags);
	entry->flags = mMapCacheEntryFlagsClearVMirror(entry->flags);
	entry->flags = mMapCacheEntryFlagsSetPaletteId(entry->flags, 0);
}

static void mapParserDMG1(struct mMapCache* cache, struct mMapCacheEntry* entry, void* vram) {
	UNUSED(cache);
	int map = *(int8_t*) vram;
	entry->tileId = map + 128;
	entry->flags = mMapCacheEntryFlagsClearHMirror(entry->flags);
	entry->flags = mMapCacheEntryFlagsClearVMirror(entry->flags);
	entry->flags = mMapCacheEntryFlagsSetPaletteId(entry->flags, 0);
}

static void mapParserCGB0(struct mMapCache* cache, struct mMapCacheEntry* entry, void* vram) {
	UNUSED(cache);
	int map = *(uint8_t*) vram;
	uint8_t attr = ((uint8_t*) vram)[0x2000];
	entry->tileId = map + GBObjAttributesGetBank(attr) * 512;
	entry->flags = mMapCacheEntryFlagsSetHMirror(entry->flags, GBObjAttributesGetXFlip(attr));
	entry->flags = mMapCacheEntryFlagsSetVMirror(entry->flags, GBObjAttributesGetYFlip(attr));
	entry->flags = mMapCacheEntryFlagsSetPaletteId(entry->flags, GBObjAttributesGetCGBPalette(attr));
}

static void mapParserCGB1(struct mMapCache* cache, struct mMapCacheEntry* entry, void* vram) {
	UNUSED(cache);
	int map = *(int8_t*) vram;
	uint8_t attr = ((uint8_t*) vram)[0x2000];
	entry->tileId = map + 128 + GBObjAttributesGetBank(attr) * 512;
	entry->flags = mMapCacheEntryFlagsSetHMirror(entry->flags, GBObjAttributesGetXFlip(attr));
	entry->flags = mMapCacheEntryFlagsSetVMirror(entry->flags, GBObjAttributesGetYFlip(attr));
	entry->flags = mMapCacheEntryFlagsSetPaletteId(entry->flags, GBObjAttributesGetCGBPalette(attr));
}

void GBVideoCacheWriteVideoRegister(struct mCacheSet* cache, uint16_t address, uint8_t value) {
	if (address != GB_REG_LCDC) {
		return;
	}
	struct mMapCache* map = mMapCacheSetGetPointer(&cache->maps, 0);
	struct mMapCache* window = mMapCacheSetGetPointer(&cache->maps, 1);

	mMapCacheSystemInfo sysconfig = mMapCacheSystemInfoIsPaletteCount(map->sysConfig);
	int tileStart = 0;
	int mapStart = GB_BASE_MAP;
	int windowStart = GB_BASE_MAP;
	if (GBRegisterLCDCIsTileMap(value)) {
		mapStart += GB_SIZE_MAP;
	}
	if (GBRegisterLCDCIsWindowTileMap(value)) {
		windowStart += GB_SIZE_MAP;
	}
	if (GBRegisterLCDCIsTileData(value)) {
		if (!sysconfig) {
			map->mapParser = mapParserDMG0;
			window->mapParser = mapParserDMG0;
		} else {
			map->mapParser = mapParserCGB0;
			window->mapParser = mapParserCGB0;
		}
	} else {
		if (!sysconfig) {
			map->mapParser = mapParserDMG1;
			window->mapParser = mapParserDMG1;
		} else {
			map->mapParser = mapParserCGB1;
			window->mapParser = mapParserCGB1;
		}
		tileStart = 0x80;
	}
	map->tileStart = tileStart;
	window->tileStart = tileStart;
	sysconfig = mMapCacheSystemInfoSetMacroTileSize(sysconfig, 5);
	sysconfig = mMapCacheSystemInfoSetPaletteBPP(sysconfig, 1);
	sysconfig = mMapCacheSystemInfoSetMapAlign(sysconfig, 0);
	sysconfig = mMapCacheSystemInfoSetWriteAlign(sysconfig, 0);
	sysconfig = mMapCacheSystemInfoSetTilesHigh(sysconfig, 5);
	sysconfig = mMapCacheSystemInfoSetTilesWide(sysconfig, 5);
	mMapCacheConfigureSystem(map, sysconfig);
	mMapCacheConfigureSystem(window, sysconfig);
	mMapCacheConfigureMap(map, mapStart);
	mMapCacheConfigureMap(window, windowStart);
}
