/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_TILE_CACHE_H
#define GB_TILE_CACHE_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct GBVideo;
struct mCacheSet;

void GBVideoCacheInit(struct mCacheSet* cache);
void GBVideoCacheAssociate(struct mCacheSet* cache, struct GBVideo* video);

void GBVideoCacheWriteVideoRegister(struct mCacheSet* cache, uint16_t address, uint8_t value);

CXX_GUARD_END

#endif
