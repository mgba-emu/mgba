/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/renderers/tile-cache.h>

#include <mgba/core/tile-cache.h>
#include <mgba/internal/gba/video.h>

void GBAVideoTileCacheInit(struct mTileCache* cache) {
	mTileCacheInit(cache);
	mTileCacheConfiguration config = 0;
	config = mTileCacheSystemInfoSetPalette0BPP(config, 2); // 2^(2^2) = 16 entries
	config = mTileCacheSystemInfoSetPalette0Count(config, 5); // 32 palettes
	config = mTileCacheSystemInfoSetPalette1BPP(config, 3); // 2^(2^3) = 256 entries
	config = mTileCacheSystemInfoSetPalette1Count(config, 1); // 2 palettes
	config = mTileCacheSystemInfoSetMaxTiles(config, 3072);
	mTileCacheConfigureSystem(cache, config);
}

void GBAVideoTileCacheAssociate(struct mTileCache* cache, struct GBAVideo* video) {
	cache->vram = video->vram;
	cache->palette = video->palette;
	video->renderer->cache = cache;
}
