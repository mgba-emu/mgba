/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_TILE_CACHE_H
#define GBA_TILE_CACHE_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct GBAVideo;
struct mTileCache;

void GBAVideoTileCacheInit(struct mTileCache* cache);
void GBAVideoTileCacheAssociate(struct mTileCache* cache, struct GBAVideo* video);

CXX_GUARD_END

#endif
