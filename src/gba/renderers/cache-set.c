/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/renderers/cache-set.h>

#include <mgba/core/cache-set.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/video.h>

void GBAVideoCacheInit(struct mCacheSet* cache) {
	mCacheSetInit(cache, 4, 2, 4);
	mTileCacheSystemInfo sysconfig = 0;
	mTileCacheConfiguration config = mTileCacheConfigurationFillShouldStore(0);
	sysconfig = mTileCacheSystemInfoSetPaletteBPP(sysconfig, 2); // 2^(2^2) = 16 entries
	sysconfig = mTileCacheSystemInfoSetPaletteCount(sysconfig, 4); // 16 palettes
	sysconfig = mTileCacheSystemInfoSetMaxTiles(sysconfig, 2048);
	mTileCacheConfigureSystem(mTileCacheSetGetPointer(&cache->tiles, 0), sysconfig, 0, 0);
	mTileCacheConfigure(mTileCacheSetGetPointer(&cache->tiles, 0), config);
	sysconfig = mTileCacheSystemInfoSetMaxTiles(sysconfig, 1024);
	mTileCacheConfigureSystem(mTileCacheSetGetPointer(&cache->tiles, 2), sysconfig, 0x10000, 0x100);
	mTileCacheConfigure(mTileCacheSetGetPointer(&cache->tiles, 2), config);

	sysconfig = mTileCacheSystemInfoSetPaletteBPP(sysconfig, 3); // 2^(2^3) = 256 entries
	sysconfig = mTileCacheSystemInfoSetPaletteCount(sysconfig, 0); // 1 palettes
	sysconfig = mTileCacheSystemInfoSetMaxTiles(sysconfig, 1024);
	mTileCacheConfigureSystem(mTileCacheSetGetPointer(&cache->tiles, 1), sysconfig, 0, 0);
	mTileCacheConfigure(mTileCacheSetGetPointer(&cache->tiles, 1), config);
	sysconfig = mTileCacheSystemInfoSetMaxTiles(sysconfig, 512);
	mTileCacheConfigureSystem(mTileCacheSetGetPointer(&cache->tiles, 3), sysconfig, 0x10000, 0x100);
	mTileCacheConfigure(mTileCacheSetGetPointer(&cache->tiles, 3), config);

	mBitmapCacheSystemInfo bitConfig;
	bitConfig = mBitmapCacheSystemInfoSetEntryBPP(0, 4);
	bitConfig = mBitmapCacheSystemInfoClearUsesPalette(bitConfig);
	bitConfig = mBitmapCacheSystemInfoSetHeight(bitConfig, 160);
	bitConfig = mBitmapCacheSystemInfoSetWidth(bitConfig, 240);
	bitConfig = mBitmapCacheSystemInfoSetBuffers(bitConfig, 1);
	mBitmapCacheConfigureSystem(mBitmapCacheSetGetPointer(&cache->bitmaps, 0), bitConfig);
	mBitmapCacheSetGetPointer(&cache->bitmaps, 0)->bitsStart[0] = 0;
	mBitmapCacheSetGetPointer(&cache->bitmaps, 0)->bitsStart[1] = 0xA000;

	bitConfig = mBitmapCacheSystemInfoSetEntryBPP(0, 3);
	bitConfig = mBitmapCacheSystemInfoFillUsesPalette(bitConfig);
	bitConfig = mBitmapCacheSystemInfoSetHeight(bitConfig, 160);
	bitConfig = mBitmapCacheSystemInfoSetWidth(bitConfig, 240);
	bitConfig = mBitmapCacheSystemInfoSetBuffers(bitConfig, 2);
	mBitmapCacheConfigureSystem(mBitmapCacheSetGetPointer(&cache->bitmaps, 1), bitConfig);
	mBitmapCacheSetGetPointer(&cache->bitmaps, 1)->bitsStart[0] = 0;
	mBitmapCacheSetGetPointer(&cache->bitmaps, 1)->bitsStart[1] = 0xA000;

	mMapCacheSetGetPointer(&cache->maps, 0)->context = NULL;
	mMapCacheSetGetPointer(&cache->maps, 1)->context = NULL;
	mMapCacheSetGetPointer(&cache->maps, 2)->context = NULL;
	mMapCacheSetGetPointer(&cache->maps, 3)->context = NULL;
}

void GBAVideoCacheAssociate(struct mCacheSet* cache, struct GBAVideo* video) {
	mCacheSetAssignVRAM(cache, video->vram);
	video->renderer->cache = cache;
	size_t i;
	for (i = 0; i < GBA_SIZE_PALETTE_RAM / 2; ++i) {
		mCacheSetWritePalette(cache, i, mColorFrom555(video->palette[i]));
	}
	GBAVideoCacheWriteVideoRegister(cache, REG_DISPCNT, video->p->memory.io[REG_DISPCNT >> 1]);
	GBAVideoCacheWriteVideoRegister(cache, REG_BG0CNT, video->p->memory.io[REG_BG0CNT >> 1]);
	GBAVideoCacheWriteVideoRegister(cache, REG_BG1CNT, video->p->memory.io[REG_BG1CNT >> 1]);
	GBAVideoCacheWriteVideoRegister(cache, REG_BG2CNT, video->p->memory.io[REG_BG2CNT >> 1]);
	GBAVideoCacheWriteVideoRegister(cache, REG_BG3CNT, video->p->memory.io[REG_BG3CNT >> 1]);
}

static void mapParser0(struct mMapCache* cache, struct mMapCacheEntry* entry, void* vram) {
	uint16_t map = *(uint16_t*) vram;
	entry->tileId = GBA_TEXT_MAP_TILE(map);
	entry->flags = mMapCacheEntryFlagsSetHMirror(entry->flags, !!GBA_TEXT_MAP_HFLIP(map));
	entry->flags = mMapCacheEntryFlagsSetVMirror(entry->flags, !!GBA_TEXT_MAP_VFLIP(map));
	if (mMapCacheSystemInfoGetPaletteBPP(cache->sysConfig) == 3) {
		entry->flags = mMapCacheEntryFlagsClearPaletteId(entry->flags);
	} else {
		entry->flags = mMapCacheEntryFlagsSetPaletteId(entry->flags, GBA_TEXT_MAP_PALETTE(map));
	}
}

static void mapParser2(struct mMapCache* cache, struct mMapCacheEntry* entry, void* vram) {
	UNUSED(cache);
	entry->tileId = *(uint8_t*) vram;
	entry->flags = mMapCacheEntryFlagsClearHMirror(entry->flags);
	entry->flags = mMapCacheEntryFlagsClearVMirror(entry->flags);
	entry->flags = mMapCacheEntryFlagsClearPaletteId(entry->flags);
}

static void GBAVideoCacheWriteDISPCNT(struct mCacheSet* cache, uint16_t value) {
	mBitmapCacheSetGetPointer(&cache->bitmaps, 1)->buffer = GBARegisterDISPCNTGetFrameSelect(value);

	switch (GBARegisterDISPCNTGetMode(value)) {
	case 0:
	default:
		mMapCacheSetGetPointer(&cache->maps, 0)->mapParser = mapParser0;
		mMapCacheSetGetPointer(&cache->maps, 1)->mapParser = mapParser0;
		mMapCacheSetGetPointer(&cache->maps, 2)->mapParser = mapParser0;
		mMapCacheSetGetPointer(&cache->maps, 3)->mapParser = mapParser0;

		mMapCacheSetGetPointer(&cache->maps, 0)->tileCache = mTileCacheSetGetPointer(&cache->tiles,
			mMapCacheSystemInfoGetPaletteBPP(mMapCacheSetGetPointer(&cache->maps, 0)->sysConfig) == 3);
		mMapCacheSetGetPointer(&cache->maps, 1)->tileCache = mTileCacheSetGetPointer(&cache->tiles,
			mMapCacheSystemInfoGetPaletteBPP(mMapCacheSetGetPointer(&cache->maps, 1)->sysConfig) == 3);
		mMapCacheSetGetPointer(&cache->maps, 2)->tileCache = mTileCacheSetGetPointer(&cache->tiles,
			mMapCacheSystemInfoGetPaletteBPP(mMapCacheSetGetPointer(&cache->maps, 2)->sysConfig) == 3);
		mMapCacheSetGetPointer(&cache->maps, 3)->tileCache = mTileCacheSetGetPointer(&cache->tiles,
			mMapCacheSystemInfoGetPaletteBPP(mMapCacheSetGetPointer(&cache->maps, 3)->sysConfig) == 3);
		break;
	case 1:
	case 2:
		mMapCacheSetGetPointer(&cache->maps, 0)->mapParser = mapParser0;
		mMapCacheSetGetPointer(&cache->maps, 1)->mapParser = mapParser0;
		mMapCacheSetGetPointer(&cache->maps, 2)->mapParser = mapParser2;
		mMapCacheSetGetPointer(&cache->maps, 3)->mapParser = mapParser2;

		mMapCacheSetGetPointer(&cache->maps, 0)->tileCache = mTileCacheSetGetPointer(&cache->tiles,
			mMapCacheSystemInfoGetPaletteBPP(mMapCacheSetGetPointer(&cache->maps, 0)->sysConfig) == 3);
		mMapCacheSetGetPointer(&cache->maps, 1)->tileCache = mTileCacheSetGetPointer(&cache->tiles,
			mMapCacheSystemInfoGetPaletteBPP(mMapCacheSetGetPointer(&cache->maps, 1)->sysConfig) == 3);

		mMapCacheSetGetPointer(&cache->maps, 2)->tileCache = mTileCacheSetGetPointer(&cache->tiles, 1);
		mMapCacheSetGetPointer(&cache->maps, 3)->tileCache = mTileCacheSetGetPointer(&cache->tiles, 1);
		break;
	}

	mBitmapCacheSystemInfo bitConfig;
	switch (GBARegisterDISPCNTGetMode(value)) {
	case 3:
		bitConfig = mBitmapCacheSystemInfoSetEntryBPP(0, 4);
		bitConfig = mBitmapCacheSystemInfoClearUsesPalette(bitConfig);
		bitConfig = mBitmapCacheSystemInfoSetHeight(bitConfig, 160);
		bitConfig = mBitmapCacheSystemInfoSetWidth(bitConfig, 240);
		bitConfig = mBitmapCacheSystemInfoSetBuffers(bitConfig, 1);
		mBitmapCacheConfigureSystem(mBitmapCacheSetGetPointer(&cache->bitmaps, 0), bitConfig);
		mBitmapCacheSetGetPointer(&cache->bitmaps, 0)->buffer = 0;
		break;
	case 5:
		bitConfig = mBitmapCacheSystemInfoSetEntryBPP(0, 4);
		bitConfig = mBitmapCacheSystemInfoClearUsesPalette(bitConfig);
		bitConfig = mBitmapCacheSystemInfoSetHeight(bitConfig, 128);
		bitConfig = mBitmapCacheSystemInfoSetWidth(bitConfig, 160);
		bitConfig = mBitmapCacheSystemInfoSetBuffers(bitConfig, 2);
		mBitmapCacheConfigureSystem(mBitmapCacheSetGetPointer(&cache->bitmaps, 0), bitConfig);
		mBitmapCacheSetGetPointer(&cache->bitmaps, 0)->buffer = GBARegisterDISPCNTGetFrameSelect(value);
		break;
	}
}

static void GBAVideoCacheWriteBGCNT(struct mCacheSet* cache, size_t bg, uint16_t value) {
	struct mMapCache* map = mMapCacheSetGetPointer(&cache->maps, bg);
	map->context = (void*) (uintptr_t) value;

	int tileStart = GBARegisterBGCNTGetCharBase(value) * 256;
	bool p = GBARegisterBGCNTGet256Color(value);
	int size = GBARegisterBGCNTGetSize(value);
	int tilesWide = 0;
	int tilesHigh = 0;
	mMapCacheSystemInfo sysconfig = mMapCacheSystemInfoSetWriteAlign(0, 1);
	if (map->mapParser == mapParser0) {
		map->tileCache = mTileCacheSetGetPointer(&cache->tiles, p);
		sysconfig = mMapCacheSystemInfoSetPaletteBPP(sysconfig, 2 + p);
		sysconfig = mMapCacheSystemInfoSetPaletteCount(sysconfig, 4 * !p);
		sysconfig = mMapCacheSystemInfoSetMacroTileSize(sysconfig, 5);
		sysconfig = mMapCacheSystemInfoSetMapAlign(sysconfig, 1);
		tilesWide = 5;
		tilesHigh = 5;
		if (size & 1) {
			++tilesWide;
		}
		if (size & 2) {
			++tilesHigh;
		}
		map->tileStart = tileStart * (2 - p);
	} else if (map->mapParser == mapParser2) {
		map->tileCache = mTileCacheSetGetPointer(&cache->tiles, 1);
		sysconfig = mMapCacheSystemInfoSetPaletteBPP(sysconfig, 3);
		sysconfig = mMapCacheSystemInfoSetPaletteCount(sysconfig, 0);
		sysconfig = mMapCacheSystemInfoSetMacroTileSize(sysconfig, 4 + size);
		sysconfig = mMapCacheSystemInfoSetMapAlign(sysconfig, 0);

		tilesHigh = 4 + size;
		tilesWide = 4 + size;
		map->tileStart = tileStart;
	}
	sysconfig = mMapCacheSystemInfoSetTilesHigh(sysconfig, tilesHigh);
	sysconfig = mMapCacheSystemInfoSetTilesWide(sysconfig, tilesWide);
	mMapCacheConfigureSystem(map, sysconfig);
	mMapCacheConfigureMap(map, GBARegisterBGCNTGetScreenBase(value) << 11);
}

void GBAVideoCacheWriteVideoRegister(struct mCacheSet* cache, uint32_t address, uint16_t value) {
	switch (address) {
	case REG_DISPCNT:
		GBAVideoCacheWriteDISPCNT(cache, value);
		GBAVideoCacheWriteBGCNT(cache, 0, (uintptr_t) mMapCacheSetGetPointer(&cache->maps, 0)->context);
		GBAVideoCacheWriteBGCNT(cache, 1, (uintptr_t) mMapCacheSetGetPointer(&cache->maps, 1)->context);
		GBAVideoCacheWriteBGCNT(cache, 2, (uintptr_t) mMapCacheSetGetPointer(&cache->maps, 2)->context);
		GBAVideoCacheWriteBGCNT(cache, 3, (uintptr_t) mMapCacheSetGetPointer(&cache->maps, 3)->context);
		break;
	case REG_BG0CNT:
		GBAVideoCacheWriteBGCNT(cache, 0, value);
		break;
	case REG_BG1CNT:
		GBAVideoCacheWriteBGCNT(cache, 1, value);
		break;
	case REG_BG2CNT:
		GBAVideoCacheWriteBGCNT(cache, 2, value);
		break;
	case REG_BG3CNT:
		GBAVideoCacheWriteBGCNT(cache, 3, value);
		break;

	}
}
