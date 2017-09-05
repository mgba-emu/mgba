/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/renderers/cache-set.h>

#include <mgba/core/cache-set.h>
#include <mgba/internal/gb/video.h>

void GBVideoCacheInit(struct mCacheSet* cache) {
	mCacheSetInit(cache, 0, 1);
	mTileCacheConfiguration config = 0;
	config = mTileCacheSystemInfoSetPaletteBPP(config, 1); // 2^(2^1) = 4 entries
	config = mTileCacheSystemInfoSetPaletteCount(config, 4); // 16 palettes
	config = mTileCacheSystemInfoSetMaxTiles(config, 1024);
	mTileCacheInit(mTileCacheSetGetPointer(&cache->tiles, 0));
	mTileCacheConfigureSystem(mTileCacheSetGetPointer(&cache->tiles, 0), config, 0, 0);
}

void GBVideoCacheAssociate(struct mCacheSet* cache, struct GBVideo* video) {
	mCacheSetAssignVRAM(cache, video->vram);
	video->renderer->cache = cache;
	size_t i;
	for (i = 0; i < 64; ++i) {
		mCacheSetWritePalette(cache, i, mColorFrom555(video->palette[i]));
	}
}
