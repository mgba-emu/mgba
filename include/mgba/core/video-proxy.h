/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_PROXY_H
#define VIDEO_PROXY_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum mVideoProxyDirtyType {
	DIRTY_DUMMY = 0,
	DIRTY_FLUSH,
	DIRTY_SCANLINE,
	DIRTY_REGISTER,
	DIRTY_OAM,
	DIRTY_PALETTE,
	DIRTY_VRAM
};

struct mVideoProxyDirtyInfo {
	enum mVideoProxyDirtyType type;
	uint32_t address;
	uint16_t value;
	uint32_t padding;
};

struct mVideoProxy {
	bool (*writeData)(struct mVideoProxy* proxy, void* data, size_t length);
	uint16_t* (*vramBlock)(struct mVideoProxy* proxy, uint32_t address);
	void* context;

	size_t vramSize;
	size_t oamSize;
	size_t paletteSize;

	uint32_t* vramDirtyBitmap;
	uint32_t* oamDirtyBitmap;

	uint16_t* vram;
	uint16_t* oam;
	uint16_t* palette;
};

void mVideoProxyRendererInit(struct mVideoProxy* proxy);
void mVideoProxyRendererDeinit(struct mVideoProxy* proxy);
void mVideoProxyRendererReset(struct mVideoProxy* proxy);

void mVideoProxyRendererWriteVideoRegister(struct mVideoProxy* proxy, uint32_t address, uint16_t value);
void mVideoProxyRendererWriteVRAM(struct mVideoProxy* proxy, uint32_t address);
void mVideoProxyRendererWritePalette(struct mVideoProxy* proxy, uint32_t address, uint16_t value);
void mVideoProxyRendererWriteOAM(struct mVideoProxy* proxy, uint32_t address, uint16_t value);

void mVideoProxyRendererDrawScanline(struct mVideoProxy* proxy, int y);
void mVideoProxyRendererFlush(struct mVideoProxy* proxy);

CXX_GUARD_END

#endif
