/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/bitmap-cache.h>

#include <mgba-util/memory.h>

void mBitmapCacheInit(struct mBitmapCache* cache) {
	// TODO: Reconfigurable cache for space savings
	cache->cache = NULL;
	cache->config = mBitmapCacheConfigurationFillShouldStore(0);
	cache->sysConfig = 0;
	cache->status = NULL;
	cache->palette = NULL;
	cache->buffer = 0;
}

static void _freeCache(struct mBitmapCache* cache) {
	size_t size = mBitmapCacheSystemInfoGetHeight(cache->sysConfig) * mBitmapCacheSystemInfoGetBuffers(cache->sysConfig);
	if (cache->cache) {
		mappedMemoryFree(cache->cache, mBitmapCacheSystemInfoGetWidth(cache->sysConfig) * size * sizeof(color_t));
		cache->cache = NULL;
	}
	if (cache->status) {
		mappedMemoryFree(cache->status, size * sizeof(*cache->status));
		cache->status = NULL;
	}
	if (cache->palette) {
		free(cache->palette);
		cache->palette = NULL;
	}
}

static void _redoCacheSize(struct mBitmapCache* cache) {
	if (!mBitmapCacheConfigurationIsShouldStore(cache->config)) {
		return;
	}

	size_t size = mBitmapCacheSystemInfoGetHeight(cache->sysConfig) * mBitmapCacheSystemInfoGetBuffers(cache->sysConfig);
	cache->cache = anonymousMemoryMap(mBitmapCacheSystemInfoGetWidth(cache->sysConfig) * size * sizeof(color_t));
	cache->status = anonymousMemoryMap(size * sizeof(*cache->status));
	if (mBitmapCacheSystemInfoIsUsesPalette(cache->sysConfig)) {
		cache->palette = calloc((1 << (1 << mBitmapCacheSystemInfoGetEntryBPP(cache->sysConfig))), sizeof(color_t));
	} else {
		cache->palette = NULL;
	}
}

void mBitmapCacheConfigure(struct mBitmapCache* cache, mBitmapCacheConfiguration config) {
	if (config == cache->config) {
		return;
	}
	_freeCache(cache);
	cache->config = config;
	_redoCacheSize(cache);
}

void mBitmapCacheConfigureSystem(struct mBitmapCache* cache, mBitmapCacheSystemInfo config) {
	if (config == cache->sysConfig) {
		return;
	}
	_freeCache(cache);
	cache->sysConfig = config;
	_redoCacheSize(cache);

	size_t stride = mBitmapCacheSystemInfoGetWidth(cache->sysConfig);
	size_t size = stride * mBitmapCacheSystemInfoGetHeight(cache->sysConfig);
	size_t bpe = mBitmapCacheSystemInfoGetEntryBPP(cache->sysConfig);
	if (bpe > 3) {
		size <<= bpe - 3;
		stride <<= bpe - 3;
	} else {
		size >>= 3 - bpe;
		stride >>= 3 - bpe;
	}
	cache->bitsSize = size;
	cache->stride = stride;
}

void mBitmapCacheDeinit(struct mBitmapCache* cache) {
	_freeCache(cache);
}

void mBitmapCacheWriteVRAM(struct mBitmapCache* cache, uint32_t address) {
	size_t i;
	for (i = 0; i < mBitmapCacheSystemInfoGetBuffers(cache->sysConfig); ++i) {
		if (address < cache->bitsStart[i]) {
			continue;
		}
		uint32_t offset = address - cache->bitsStart[i];
		if (offset >= cache->bitsSize) {
			continue;
		}
		offset /= cache->stride;
		offset *= mBitmapCacheSystemInfoGetBuffers(cache->sysConfig);
		offset += cache->buffer;
		cache->status[offset].vramClean = 0;
		++cache->status[offset].vramVersion;
	}
}

void mBitmapCacheWritePalette(struct mBitmapCache* cache, uint32_t entry, color_t color) {
	if (!mBitmapCacheSystemInfoIsUsesPalette(cache->sysConfig)) {
		return;
	}
	size_t maxEntry = 1 << (1 << mBitmapCacheSystemInfoGetEntryBPP(cache->sysConfig));
	if (entry >= maxEntry) {
		return;
	}
	cache->palette[entry] = color;
	++cache->globalPaletteVersion;
}

uint32_t _lookupEntry8(void* vram, uint32_t offset) {
	return ((uint8_t*) vram)[offset];
}

uint32_t _lookupEntry15(void* vram, uint32_t offset) {
	return mColorFrom555(((uint16_t*) vram)[offset]);
}

void mBitmapCacheCleanRow(struct mBitmapCache* cache, struct mBitmapCacheEntry* entry, unsigned y) {
	color_t* row = &cache->cache[(cache->buffer * mBitmapCacheSystemInfoGetHeight(cache->sysConfig) + y) * mBitmapCacheSystemInfoGetWidth(cache->sysConfig)];
	size_t location = cache->buffer + mBitmapCacheSystemInfoGetBuffers(cache->sysConfig) * y;
	struct mBitmapCacheEntry* status = &cache->status[location];
	struct mBitmapCacheEntry desiredStatus = {
		.paletteVersion = cache->globalPaletteVersion,
		.vramVersion = entry->vramVersion,
		.vramClean = 1
	};

	if (entry) {
		entry[location] = desiredStatus;
	}

	if (!mBitmapCacheConfigurationIsShouldStore(cache->config) || !memcmp(status, &desiredStatus, sizeof(*entry))) {
		return;
	}

	size_t offset = y * mBitmapCacheSystemInfoGetWidth(cache->sysConfig);
	void* vram;
	int bpe = mBitmapCacheSystemInfoGetEntryBPP(cache->sysConfig);
	uint32_t (*lookupEntry)(void*, uint32_t);
	switch (bpe) {
	case 3:
		lookupEntry = _lookupEntry8;
		vram = &cache->vram[offset + cache->bitsStart[cache->buffer]];
		break;
	case 4:
		lookupEntry = _lookupEntry15;
		vram = &cache->vram[offset * 2 + cache->bitsStart[cache->buffer]];
		break;
	default:
		abort();
		break;
	}

	size_t x;
	if (mBitmapCacheSystemInfoIsUsesPalette(cache->sysConfig)) {
		for (x = 0; x < mBitmapCacheSystemInfoGetWidth(cache->sysConfig); ++x) {
			row[x] = cache->palette[lookupEntry(vram, x)];
		}
	} else {
		for (x = 0; x < mBitmapCacheSystemInfoGetWidth(cache->sysConfig); ++x) {
			row[x] = lookupEntry(vram, x);
		}
	}
	*status = desiredStatus;
}

bool mBitmapCacheCheckRow(struct mBitmapCache* cache, const struct mBitmapCacheEntry* entry, unsigned y) {
	size_t location = cache->buffer + mBitmapCacheSystemInfoGetBuffers(cache->sysConfig) * y;
	struct mBitmapCacheEntry desiredStatus = {
		.paletteVersion = cache->globalPaletteVersion,
		.vramVersion = entry->vramVersion,
		.vramClean = 1
	};

	return memcmp(&entry[location], &desiredStatus, sizeof(*entry)) == 0;
}

const color_t* mBitmapCacheGetRow(struct mBitmapCache* cache, unsigned y) {
	color_t* row = &cache->cache[(cache->buffer * mBitmapCacheSystemInfoGetHeight(cache->sysConfig) + y) * mBitmapCacheSystemInfoGetWidth(cache->sysConfig)];
	return row;
}
