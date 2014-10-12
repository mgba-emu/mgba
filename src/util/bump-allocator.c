/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "bump-allocator.h"

#define BUMP_POOL_SIZE 0x4000

void BumpAllocatorInit(struct BumpAllocator* allocator, size_t unitSize) {
	allocator->unitSize = unitSize;
	allocator->pools = malloc(sizeof(void*));
	allocator->currentPool = malloc(BUMP_POOL_SIZE);
	allocator->nPools = 1;
	allocator->sizePools = 1;
	allocator->pools[0] = allocator->currentPool;
	allocator->poolFilled = 0;
}

void BumpAllocatorDeinit(struct BumpAllocator* allocator) {
	size_t i;
	for (i = 0; i < allocator->nPools; ++i) {
		free(allocator->pools[i]);
	}
	free(allocator->pools);
	allocator->pools = 0;
	allocator->currentPool = 0;
	allocator->sizePools = 0;
	allocator->nPools = 0;
}

void* BumpAllocatorAlloc(struct BumpAllocator* allocator) {
	void* memory = &((uint8_t*) allocator->currentPool)[allocator->poolFilled];
	allocator->poolFilled += allocator->unitSize;
	if (allocator->poolFilled + allocator->unitSize > BUMP_POOL_SIZE) {
		allocator->poolFilled = 0;
		if (allocator->nPools == allocator->sizePools) {
			allocator->sizePools *= 2;
			allocator->pools = realloc(allocator->pools, allocator->sizePools * sizeof(void*));
		}
		allocator->currentPool = malloc(BUMP_POOL_SIZE);
		allocator->pools[allocator->nPools] = allocator->currentPool;
		++allocator->nPools;
	}
	return memory;
}
