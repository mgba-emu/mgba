/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "software.h"

#include "gb/io.h"
#include "util/memory.h"

static void GBVideoSoftwareRendererInit(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererReset(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address);
static void GBVideoSoftwareRendererWriteOAM(struct GBVideoRenderer* renderer, uint8_t oam);
static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoSoftwareRendererDrawScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererGetPixels(struct GBVideoRenderer* renderer, unsigned* stride, const void** pixels);
static void GBVideoSoftwareRendererPutPixels(struct GBVideoRenderer* renderer, unsigned stride, void* pixels);

static void GBVideoSoftwareRendererDrawBackground(struct GBVideoSoftwareRenderer* renderer, uint8_t* maps, int x, int y, int sx, int sy);
static void GBVideoSoftwareRendererDrawObj(struct GBVideoSoftwareRenderer* renderer, struct GBObj* obj, int y);

static void _cleanOAM(struct GBVideoSoftwareRenderer* renderer);

#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
static const color_t GB_PALETTE[4] = { 0xFFFF, 0x39C7, 0x18C3, 0x0000};
#else
static const color_t GB_PALETTE[4] = { 0x7FFF, 0x1DE7, 0x0C63, 0x0000};
#endif
#else
static const color_t GB_PALETTE[4] = { 0xFFFFFF, 0x808080, 0x404040, 0x000000};
#endif

void GBVideoSoftwareRendererCreate(struct GBVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBVideoSoftwareRendererInit;
	renderer->d.reset = GBVideoSoftwareRendererReset;
	renderer->d.deinit = GBVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writeVRAM = GBVideoSoftwareRendererWriteVRAM;
	renderer->d.writeOAM = GBVideoSoftwareRendererWriteOAM;
	renderer->d.drawScanline = GBVideoSoftwareRendererDrawScanline;
	renderer->d.finishFrame = GBVideoSoftwareRendererFinishFrame;
	renderer->d.getPixels = 0;
	renderer->d.putPixels = 0;

	renderer->temporaryBuffer = 0;
}

static void GBVideoSoftwareRendererInit(struct GBVideoRenderer* renderer) {
	GBVideoSoftwareRendererReset(renderer);

	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	int y;
	for (y = 0; y < GB_VIDEO_VERTICAL_PIXELS; ++y) {
		color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
		int x;
		for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS; ++x) {
			row[x] = GB_PALETTE[0];
		}
	}
}

static void GBVideoSoftwareRendererReset(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	softwareRenderer->scy = 0;
	softwareRenderer->scx = 0;
	softwareRenderer->wy = 0;
	softwareRenderer->wx = 0;
	softwareRenderer->oamMax = 0;
	softwareRenderer->oamDirty = false;
}

static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	UNUSED(softwareRenderer);
}

static void GBVideoSoftwareRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	// TODO
}

static void GBVideoSoftwareRendererWriteOAM(struct GBVideoRenderer* renderer, uint8_t oam) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	UNUSED(oam);
	softwareRenderer->oamDirty = true;
}

static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	switch (address) {
	case REG_LCDC:
		softwareRenderer->lcdc = value;
		break;
	case REG_BGP:
		softwareRenderer->bgPalette[0] = GB_PALETTE[value & 3];
		softwareRenderer->bgPalette[1] = GB_PALETTE[(value >> 2) & 3];
		softwareRenderer->bgPalette[2] = GB_PALETTE[(value >> 4) & 3];
		softwareRenderer->bgPalette[3] = GB_PALETTE[(value >> 6) & 3];
		break;
	case REG_OBP0:
		softwareRenderer->objPalette[0][0] = GB_PALETTE[value & 3];
		softwareRenderer->objPalette[0][1] = GB_PALETTE[(value >> 2) & 3];
		softwareRenderer->objPalette[0][2] = GB_PALETTE[(value >> 4) & 3];
		softwareRenderer->objPalette[0][3] = GB_PALETTE[(value >> 6) & 3];
		break;
	case REG_OBP1:
		softwareRenderer->objPalette[1][0] = GB_PALETTE[value & 3];
		softwareRenderer->objPalette[1][1] = GB_PALETTE[(value >> 2) & 3];
		softwareRenderer->objPalette[1][2] = GB_PALETTE[(value >> 4) & 3];
		softwareRenderer->objPalette[1][3] = GB_PALETTE[(value >> 6) & 3];
		break;
	case REG_SCY:
		softwareRenderer->scy = value;
		break;
	case REG_SCX:
		softwareRenderer->scx = value;
		break;
	case REG_WY:
		softwareRenderer->wy = value;
		break;
	case REG_WX:
		softwareRenderer->wx = value;
		break;
	}
	return value;
}

static void GBVideoSoftwareRendererDrawScanline(struct GBVideoRenderer* renderer, int y) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	size_t x;
	for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS; ++x) {
		softwareRenderer->row[x] = GB_PALETTE[0];
	}

	if (softwareRenderer->oamDirty) {
		_cleanOAM(softwareRenderer);
	}

	uint8_t* maps = &softwareRenderer->d.vram[GB_BASE_MAP];
	if (GBRegisterLCDCIsTileMap(softwareRenderer->lcdc)) {
		maps += GB_SIZE_MAP;
	}
	GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, 0, y, softwareRenderer->scx, softwareRenderer->scy);

	if (GBRegisterLCDCIsWindow(softwareRenderer->lcdc) && softwareRenderer->wy <= y) {
		maps = &softwareRenderer->d.vram[GB_BASE_MAP];
		if (GBRegisterLCDCIsWindowTileMap(softwareRenderer->lcdc)) {
			maps += GB_SIZE_MAP;
		}
		GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, 0, y, 7 - softwareRenderer->wx, -softwareRenderer->wy);
	}

	int spriteHeight = 8;
	if (GBRegisterLCDCIsObjSize(softwareRenderer->lcdc)) {
		spriteHeight = 16;
	}
	int i;
	for (i = 0; i < softwareRenderer->oamMax; ++i) {
		// TODO: Sprite sizes
		if (y >= softwareRenderer->obj[i]->y - 16 && y < softwareRenderer->obj[i]->y - 16 + spriteHeight) {
			GBVideoSoftwareRendererDrawObj(softwareRenderer, softwareRenderer->obj[i], y);
		}
	}

	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
#ifdef COLOR_16_BIT
#if defined(__ARM_NEON) && !defined(__APPLE__)
	_to16Bit(row, softwareRenderer->row, GB_VIDEO_HORIZONTAL_PIXELS);
#else
	for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS; ++x) {
		row[x] = softwareRenderer->row[x];
	}
#endif
#else
	memcpy(row, softwareRenderer->row, GB_VIDEO_HORIZONTAL_PIXELS * sizeof(*row));
#endif
}

static void _cleanOAM(struct GBVideoSoftwareRenderer* renderer) {
	// TODO: GBC differences
	renderer->oamMax = 0;
	int o = 0;
	int i;
	for (i = 0; i < 40; ++i) {
		uint8_t y = renderer->d.oam->obj[i].y;
		if (y < 16 || y >= GB_VIDEO_VERTICAL_PIXELS + 16) {
			continue;
		}
		// TODO: Sort
		renderer->obj[o] = &renderer->d.oam->obj[i];
		++o;
	}
	renderer->oamMax = o;
	renderer->oamDirty = false;
}

static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	if (softwareRenderer->temporaryBuffer) {
		mappedMemoryFree(softwareRenderer->temporaryBuffer, GB_VIDEO_HORIZONTAL_PIXELS * GB_VIDEO_VERTICAL_PIXELS * 4);
		softwareRenderer->temporaryBuffer = 0;
	}
}

static void GBVideoSoftwareRendererDrawBackground(struct GBVideoSoftwareRenderer* renderer, uint8_t* maps, int x, int y, int sx, int sy) {
	uint8_t* data = renderer->d.vram;
	if (!GBRegisterLCDCIsTileData(renderer->lcdc)) {
		data += 0x1000;
	}
	int topY = (((y + sy) >> 3) & 0x1F) * 0x20;
	int bottomY = (y + sy) & 7;
	for (; x < GB_VIDEO_HORIZONTAL_PIXELS; ++x) {
		int topX = ((x + sx) >> 3) & 0x1F;
		int bottomX = 7 - ((x + sx) & 7);
		int bgTile;
		if (GBRegisterLCDCIsTileData(renderer->lcdc)) {
			bgTile = maps[topX + topY];
		} else {
			bgTile = ((int8_t*) maps)[topX + topY];
		}
		uint8_t tileDataLower = data[(bgTile * 8 + bottomY) * 2];
		uint8_t tileDataUpper = data[(bgTile * 8 + bottomY) * 2 + 1];
		tileDataUpper >>= bottomX;
		tileDataLower >>= bottomX;
		renderer->row[x] = renderer->bgPalette[((tileDataUpper & 1) << 1) | (tileDataLower & 1)];
	}
}

static void GBVideoSoftwareRendererDrawObj(struct GBVideoSoftwareRenderer* renderer, struct GBObj* obj, int y) {
	uint8_t* data = renderer->d.vram;
	int tileOffset = 0;
	int bottomY;
	if (GBObjAttributesIsYFlip(obj->attr)) {
		bottomY = 7 - ((y - obj->y - 16) & 7);
		if (y - obj->y < -8) {
			++tileOffset;
		}
	} else {
		bottomY = (y - obj->y - 16) & 7;
		if (y - obj->y >= -8) {
			++tileOffset;
		}
	}
	int end = GB_VIDEO_HORIZONTAL_PIXELS;
	if (obj->x < end) {
		end = obj->x;
	}
	int x = obj->x - 8;
	if (x < 0) {
		x = 0;
	}
	for (; x < end; ++x) {
		int bottomX;
		if (GBObjAttributesIsXFlip(obj->attr)) {
			bottomX = (x - obj->x) & 7;
		} else {
			bottomX = 7 - ((x - obj->x) & 7);
		}
		int objTile = obj->tile + tileOffset;
		uint8_t tileDataLower = data[(objTile * 8 + bottomY) * 2];
		uint8_t tileDataUpper = data[(objTile * 8 + bottomY) * 2 + 1];
		tileDataUpper >>= bottomX;
		tileDataLower >>= bottomX;
		color_t current = renderer->row[x];
		if (((tileDataUpper | tileDataLower) & 1) && (!GBObjAttributesIsPriority(obj->attr) || current == GB_PALETTE[0])) {
			renderer->row[x] = renderer->bgPalette[((tileDataUpper & 1) << 1) | (tileDataLower & 1)];
		}
	}
}
