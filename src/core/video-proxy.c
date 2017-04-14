/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/video-proxy.h>

#include <mgba-util/memory.h>

static inline size_t _roundUp(size_t value, int shift) {
	value += (1 << shift) - 1;
	return value >> shift;
}

void mVideoProxyRendererInit(struct mVideoProxy* proxy) {
	proxy->palette = anonymousMemoryMap(proxy->paletteSize);
	proxy->vram = anonymousMemoryMap(proxy->vramSize);
	proxy->oam = anonymousMemoryMap(proxy->oamSize);

	proxy->vramDirtyBitmap = calloc(_roundUp(proxy->vramSize, 17), sizeof(uint32_t));
	proxy->oamDirtyBitmap = calloc(_roundUp(proxy->oamSize, 6), sizeof(uint32_t));
}

void mVideoProxyRendererDeinit(struct mVideoProxy* proxy) {
	mappedMemoryFree(proxy->palette, proxy->paletteSize);
	mappedMemoryFree(proxy->vram, proxy->vramSize);
	mappedMemoryFree(proxy->oam, proxy->oamSize);

	free(proxy->vramDirtyBitmap);
	free(proxy->oamDirtyBitmap);
}

void mVideoProxyRendererReset(struct mVideoProxy* proxy) {
	memset(proxy->vramDirtyBitmap, 0, sizeof(uint32_t) * _roundUp(proxy->vramSize, 17));
	memset(proxy->oamDirtyBitmap, 0, sizeof(uint32_t) * _roundUp(proxy->oamSize, 6));
}

void mVideoProxyRendererWriteVideoRegister(struct mVideoProxy* proxy, uint32_t address, uint16_t value) {
	struct mVideoProxyDirtyInfo dirty = {
		DIRTY_REGISTER,
		address,
		value,
		0xDEADBEEF,
	};
	proxy->writeData(proxy, &dirty, sizeof(dirty));
}

void mVideoProxyRendererWriteVRAM(struct mVideoProxy* proxy, uint32_t address) {
	int bit = 1 << (address >> 12);
	if (proxy->vramDirtyBitmap[address >> 17] & bit) {
		return;
	}
	proxy->vramDirtyBitmap[address >> 17] |= bit;
}

void mVideoProxyRendererWritePalette(struct mVideoProxy* proxy, uint32_t address, uint16_t value) {
	struct mVideoProxyDirtyInfo dirty = {
		DIRTY_PALETTE,
		address,
		value,
		0xDEADBEEF,
	};
	proxy->writeData(proxy, &dirty, sizeof(dirty));
}

void mVideoProxyRendererWriteOAM(struct mVideoProxy* proxy, uint32_t address, uint16_t value) {
	struct mVideoProxyDirtyInfo dirty = {
		DIRTY_OAM,
		address,
		value,
		0xDEADBEEF,
	};
	proxy->writeData(proxy, &dirty, sizeof(dirty));
}

void mVideoProxyRendererDrawScanline(struct mVideoProxy* proxy, int y) {
	size_t i;
	for (i = 0; i < _roundUp(proxy->vramSize, 17); ++i) {
		if (proxy->vramDirtyBitmap[i]) {
			uint32_t bitmap = proxy->vramDirtyBitmap[i];
			proxy->vramDirtyBitmap[i] = 0;
			int j;
			for (j = 0; j < 32; ++j) {
				if (!(bitmap & (1 << j))) {
					continue;
				}
				struct mVideoProxyDirtyInfo dirty = {
					DIRTY_VRAM,
					j * 0x1000,
					0xABCD,
					0xDEADBEEF,
				};
				proxy->writeData(proxy, &dirty, sizeof(dirty));
				proxy->writeData(proxy, proxy->vramBlock(proxy, j * 0x1000), 0x1000);
			}
		}
	}
	struct mVideoProxyDirtyInfo dirty = {
		DIRTY_SCANLINE,
		y,
		0,
		0xDEADBEEF,
	};
	proxy->writeData(proxy, &dirty, sizeof(dirty));
}

void mVideoProxyRendererFlush(struct mVideoProxy* proxy) {
	struct mVideoProxyDirtyInfo dirty = {
		DIRTY_FLUSH,
		0,
		0,
		0xDEADBEEF,
	};
	proxy->writeData(proxy, &dirty, sizeof(dirty));
}

bool mVideoProxyRendererRun(struct mVideoProxy* proxy) {
	struct mVideoProxyDirtyInfo item = {0};
	while (proxy->readData(proxy, &item, sizeof(item), false)) {
		switch (item.type) {
		case DIRTY_REGISTER:
		case DIRTY_PALETTE:
		case DIRTY_OAM:
		case DIRTY_VRAM:
		case DIRTY_SCANLINE:
		case DIRTY_FLUSH:
			if (!proxy->parsePacket(proxy, &item)) {
				return true;
			}
			break;
		default:
			return false;
		}
	}
	return true;
}
