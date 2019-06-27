/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_BITMAP_CACHE_H
#define M_BITMAP_CACHE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/interface.h>

DECL_BITFIELD(mBitmapCacheConfiguration, uint32_t);
DECL_BIT(mBitmapCacheConfiguration, ShouldStore, 0);

DECL_BITFIELD(mBitmapCacheSystemInfo, uint32_t);
DECL_BITS(mBitmapCacheSystemInfo, EntryBPP, 0, 3);
DECL_BIT(mBitmapCacheSystemInfo, UsesPalette, 3);
DECL_BITS(mBitmapCacheSystemInfo, Width, 4, 10);
DECL_BITS(mBitmapCacheSystemInfo, Height, 14, 10);
DECL_BITS(mBitmapCacheSystemInfo, Buffers, 24, 2);

struct mBitmapCacheEntry {
	uint32_t paletteVersion;
	uint32_t vramVersion;
	uint8_t vramClean;
};

struct mBitmapCache {
	color_t* cache;
	struct mBitmapCacheEntry* status;

	uint32_t globalPaletteVersion;

	uint8_t* vram;
	color_t* palette;

	uint32_t bitsSize;
	uint32_t bitsStart[2];
	uint32_t stride;
	uint8_t buffer;

	mBitmapCacheConfiguration config;
	mBitmapCacheSystemInfo sysConfig;

	void* context;
};

void mBitmapCacheInit(struct mBitmapCache* cache);
void mBitmapCacheDeinit(struct mBitmapCache* cache);
void mBitmapCacheConfigure(struct mBitmapCache* cache, mBitmapCacheConfiguration config);
void mBitmapCacheConfigureSystem(struct mBitmapCache* cache, mBitmapCacheSystemInfo config);
void mBitmapCacheWriteVRAM(struct mBitmapCache* cache, uint32_t address);
void mBitmapCacheWritePalette(struct mBitmapCache* cache, uint32_t entry, color_t color);

void mBitmapCacheCleanRow(struct mBitmapCache* cache, struct mBitmapCacheEntry* entry, unsigned y);
bool mBitmapCacheCheckRow(struct mBitmapCache* cache, const struct mBitmapCacheEntry* entry, unsigned y);
const color_t* mBitmapCacheGetRow(struct mBitmapCache* cache, unsigned y);

CXX_GUARD_END

#endif
