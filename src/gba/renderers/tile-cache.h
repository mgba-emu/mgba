/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_TILE_CACHE_H
#define GBA_TILE_CACHE_H

#include "util/common.h"

struct GBAVideo;

DECL_BITFIELD(GBAVideoTileCacheConfiguration, uint32_t);
DECL_BIT(GBAVideoTileCacheConfiguration, ShouldStore, 0);

struct GBAVideoTileCache {
	uint16_t* cache;
	struct GBAVideoTileCacheEntry {
		uint32_t paletteVersion;
		uint8_t vramClean;
		uint8_t palette256;
	} status[1024 * 3][16];
	uint32_t globalPaletteVersion[32];
	uint32_t globalPalette256Version[2];

	uint16_t* vram;
	uint16_t* palette;
	uint16_t temporaryTile[64];

	GBAVideoTileCacheConfiguration config;
};

void GBAVideoTileCacheInit(struct GBAVideoTileCache* cache);
void GBAVideoTileCacheDeinit(struct GBAVideoTileCache* cache);
void GBAVideoTileCacheConfigure(struct GBAVideoTileCache* cache, GBAVideoTileCacheConfiguration config);
void GBAVideoTileCacheAssociate(struct GBAVideoTileCache* cache, struct GBAVideo* video);
void GBAVideoTileCacheWriteVRAM(struct GBAVideoTileCache* cache, uint32_t address);
void GBAVideoTileCacheWritePalette(struct GBAVideoTileCache* cache, uint32_t address);
const uint16_t* GBAVideoTileCacheGetTile16(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId);
const uint16_t* GBAVideoTileCacheGetTile16IfDirty(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId);
const uint16_t* GBAVideoTileCacheGetTile256(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId);
const uint16_t* GBAVideoTileCacheGetTile256IfDirty(struct GBAVideoTileCache* cache, unsigned tileId, unsigned paletteId);

#endif
