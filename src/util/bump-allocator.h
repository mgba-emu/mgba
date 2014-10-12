/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef BUMP_ALLOCATOR_H
#define BUMP_ALLOCATOR_H

#include "util/common.h"

struct BumpAllocator {
	size_t unitSize;
	void** pools;
	size_t nPools;
	size_t sizePools;
	void* currentPool;
	size_t poolFilled;
};

void BumpAllocatorInit(struct BumpAllocator*, size_t unitSize);
void BumpAllocatorDeinit(struct BumpAllocator*);

void* BumpAllocatorAlloc(struct BumpAllocator*);

#endif
