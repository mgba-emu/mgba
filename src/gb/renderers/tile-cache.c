/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/renderers/tile-cache.h>

#include <mgba/core/tile-cache.h>
#include <mgba/internal/gb/video.h>
#include <mgba/internal/gb/renderers/tile-cache.h>

void GBVideoTileCacheInit(struct mTileCache* cache) {
	mTileCacheInit(cache);
	mTileCacheConfiguration config = 0;
	config = mTileCacheSystemInfoSetPalette0BPP(config, 1); // 2^(2^2) = 4 entries
	config = mTileCacheSystemInfoSetPalette0Count(config, 4); // 16 palettes
	config = mTileCacheSystemInfoSetPalette1BPP(config, 0); // Disable
	config = mTileCacheSystemInfoSetPalette1Count(config, 0); // Disable
	config = mTileCacheSystemInfoSetMaxTiles(config, 1024);
	mTileCacheConfigureSystem(cache, config);
}

void GBVideoTileCacheAssociate(struct mTileCache* cache, struct GBVideo* video) {
	cache->vram = (uint16_t*) video->vram;
	cache->palette = video->palette;
	video->renderer->cache = cache;
}
