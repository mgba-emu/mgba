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
static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoSoftwareRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj** obj, size_t oamMax);
static void GBVideoSoftwareRendererFinishScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererGetPixels(struct GBVideoRenderer* renderer, unsigned* stride, const void** pixels);
static void GBVideoSoftwareRendererPutPixels(struct GBVideoRenderer* renderer, unsigned stride, void* pixels);

static void GBVideoSoftwareRendererDrawBackground(struct GBVideoSoftwareRenderer* renderer, uint8_t* maps, int startX, int endX, int y, int sx, int sy);
static void GBVideoSoftwareRendererDrawObj(struct GBVideoSoftwareRenderer* renderer, struct GBObj* obj, int startX, int endX, int y);

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
	renderer->d.drawRange = GBVideoSoftwareRendererDrawRange;
	renderer->d.finishScanline = GBVideoSoftwareRendererFinishScanline;
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
	softwareRenderer->currentWy = 0;
	softwareRenderer->wx = 0;
}

static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	UNUSED(softwareRenderer);
}

static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	switch (address) {
	case REG_LCDC:
		softwareRenderer->lcdc = value;
		break;
	case REG_BGP:
		softwareRenderer->palette[0] = GB_PALETTE[value & 3];
		softwareRenderer->palette[1] = GB_PALETTE[(value >> 2) & 3];
		softwareRenderer->palette[2] = GB_PALETTE[(value >> 4) & 3];
		softwareRenderer->palette[3] = GB_PALETTE[(value >> 6) & 3];
		break;
	case REG_OBP0:
		softwareRenderer->palette[8 * 4 + 0] = GB_PALETTE[value & 3];
		softwareRenderer->palette[8 * 4 + 1] = GB_PALETTE[(value >> 2) & 3];
		softwareRenderer->palette[8 * 4 + 2] = GB_PALETTE[(value >> 4) & 3];
		softwareRenderer->palette[8 * 4 + 3] = GB_PALETTE[(value >> 6) & 3];
		break;
	case REG_OBP1:
		softwareRenderer->palette[9 * 4 + 0] = GB_PALETTE[value & 3];
		softwareRenderer->palette[9 * 4 + 1] = GB_PALETTE[(value >> 2) & 3];
		softwareRenderer->palette[9 * 4 + 2] = GB_PALETTE[(value >> 4) & 3];
		softwareRenderer->palette[9 * 4 + 3] = GB_PALETTE[(value >> 6) & 3];
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

static void GBVideoSoftwareRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj** obj, size_t oamMax) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	uint8_t* maps = &softwareRenderer->d.vram[GB_BASE_MAP];
	if (GBRegisterLCDCIsTileMap(softwareRenderer->lcdc)) {
		maps += GB_SIZE_MAP;
	}
	if (GBRegisterLCDCIsBgEnable(softwareRenderer->lcdc)) {
		GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, startX, endX, y, softwareRenderer->scx, softwareRenderer->scy);

		if (GBRegisterLCDCIsWindow(softwareRenderer->lcdc) && softwareRenderer->wy <= y && endX >= softwareRenderer->wx - 7) {
			maps = &softwareRenderer->d.vram[GB_BASE_MAP];
			if (GBRegisterLCDCIsWindowTileMap(softwareRenderer->lcdc)) {
				maps += GB_SIZE_MAP;
			}
			GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, softwareRenderer->wx - 7, endX, y, 7 - softwareRenderer->wx, (softwareRenderer->currentWy - y) - softwareRenderer->wy);
		}
	} else {
		int x;
		for (x = startX; x < endX; ++x) {
			softwareRenderer->row[x] = 0;
		}
	}

	if (GBRegisterLCDCIsObjEnable(softwareRenderer->lcdc)) {
		size_t i;
		for (i = 0; i < oamMax; ++i) {
			GBVideoSoftwareRendererDrawObj(softwareRenderer, obj[i], startX, endX, y);
		}
	}

	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
	int x;
	for (x = startX; x < endX; ++x) {
		row[x] = softwareRenderer->palette[softwareRenderer->row[x]];
	}
}

static void GBVideoSoftwareRendererFinishScanline(struct GBVideoRenderer* renderer, int y) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	if (GBRegisterLCDCIsBgEnable(softwareRenderer->lcdc) && GBRegisterLCDCIsWindow(softwareRenderer->lcdc) && softwareRenderer->wy <= y && softwareRenderer->wx - 7 < GB_VIDEO_HORIZONTAL_PIXELS) {
		++softwareRenderer->currentWy;
	}
}

static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	if (softwareRenderer->temporaryBuffer) {
		mappedMemoryFree(softwareRenderer->temporaryBuffer, GB_VIDEO_HORIZONTAL_PIXELS * GB_VIDEO_VERTICAL_PIXELS * 4);
		softwareRenderer->temporaryBuffer = 0;
	}
	softwareRenderer->currentWy = softwareRenderer->wy;
}

static void GBVideoSoftwareRendererDrawBackground(struct GBVideoSoftwareRenderer* renderer, uint8_t* maps, int startX, int endX, int y, int sx, int sy) {
	uint8_t* data = renderer->d.vram;
	if (!GBRegisterLCDCIsTileData(renderer->lcdc)) {
		data += 0x1000;
	}
	int topY = (((y + sy) >> 3) & 0x1F) * 0x20;
	int bottomY = (y + sy) & 7;
	int x;
	for (x = startX; x < endX; ++x) {
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
		renderer->row[x] = ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
	}
}

static void GBVideoSoftwareRendererDrawObj(struct GBVideoSoftwareRenderer* renderer, struct GBObj* obj, int startX, int endX, int y) {
	int ix = obj->x - 8;
	if (endX < ix || startX >= ix + 8) {
		return;
	}
	if (obj->x < endX) {
		endX = obj->x;
	}
	if (obj->x - 8 > startX) {
		startX = obj->x - 8;
	}
	uint8_t* data = renderer->d.vram;
	int tileOffset = 0;
	int bottomY;
	if (GBObjAttributesIsYFlip(obj->attr)) {
		bottomY = 7 - ((y - obj->y - 16) & 7);
		if (GBRegisterLCDCIsObjSize(renderer->lcdc) && y - obj->y < -8) {
			++tileOffset;
		}
	} else {
		bottomY = (y - obj->y - 16) & 7;
		if (GBRegisterLCDCIsObjSize(renderer->lcdc) && y - obj->y >= -8) {
			++tileOffset;
		}
	}
	uint8_t mask = GBObjAttributesIsPriority(obj->attr) ? 0 : 0x20;
	int p = (GBObjAttributesGetPalette(obj->attr) + 8) * 4;
	int bottomX;
	int x;
	for (x = startX; x < endX; ++x) {
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
		if (((tileDataUpper | tileDataLower) & 1) && current <= mask) {
			renderer->row[x] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		}
	}
}
