/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CACHE_SET_H
#define GBA_CACHE_SET_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct GBAVideo;
struct mCacheSet;

void GBAVideoCacheInit(struct mCacheSet* cache);
void GBAVideoCacheAssociate(struct mCacheSet* cache, struct GBAVideo* video);
void GBAVideoCacheWriteVideoRegister(struct mCacheSet* cache, uint32_t address, uint16_t value);

CXX_GUARD_END

#endif
