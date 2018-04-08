/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/renderers/software.h>

#include <mgba/core/tile-cache.h>
#include <mgba/internal/gb/io.h>
#include <mgba-util/memory.h>

static void GBVideoSoftwareRendererInit(struct GBVideoRenderer* renderer, enum GBModel model);
static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer);
static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoSoftwareRendererWritePalette(struct GBVideoRenderer* renderer, int index, uint16_t value);
static void GBVideoSoftwareRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address);
static void GBVideoSoftwareRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam);
static void GBVideoSoftwareRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj* obj, size_t oamMax);
static void GBVideoSoftwareRendererFinishScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoSoftwareRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBVideoSoftwareRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels);

static void GBVideoSoftwareRendererDrawBackground(struct GBVideoSoftwareRenderer* renderer, uint8_t* maps, int startX, int endX, int sx, int sy);
static void GBVideoSoftwareRendererDrawObj(struct GBVideoSoftwareRenderer* renderer, struct GBObj* obj, int startX, int endX, int y);

static void _clearScreen(struct GBVideoSoftwareRenderer* renderer) {
	int y;
	for (y = 0; y < GB_VIDEO_VERTICAL_PIXELS; ++y) {
		color_t* row = &renderer->outputBuffer[renderer->outputBufferStride * y];
		int x;
		for (x = 0; x < GB_VIDEO_HORIZONTAL_PIXELS; x += 4) {
			row[x + 0] = renderer->palette[0];
			row[x + 1] = renderer->palette[0];
			row[x + 2] = renderer->palette[0];
			row[x + 3] = renderer->palette[0];
		}
	}
}

static bool _inWindow(struct GBVideoSoftwareRenderer* renderer) {
	return GBRegisterLCDCIsWindow(renderer->lcdc) && GB_VIDEO_HORIZONTAL_PIXELS + 7 > renderer->wx;
}

void GBVideoSoftwareRendererCreate(struct GBVideoSoftwareRenderer* renderer) {
	renderer->d.init = GBVideoSoftwareRendererInit;
	renderer->d.deinit = GBVideoSoftwareRendererDeinit;
	renderer->d.writeVideoRegister = GBVideoSoftwareRendererWriteVideoRegister;
	renderer->d.writePalette = GBVideoSoftwareRendererWritePalette;
	renderer->d.writeVRAM = GBVideoSoftwareRendererWriteVRAM;
	renderer->d.writeOAM = GBVideoSoftwareRendererWriteOAM;
	renderer->d.drawRange = GBVideoSoftwareRendererDrawRange;
	renderer->d.finishScanline = GBVideoSoftwareRendererFinishScanline;
	renderer->d.finishFrame = GBVideoSoftwareRendererFinishFrame;
	renderer->d.getPixels = GBVideoSoftwareRendererGetPixels;
	renderer->d.putPixels = GBVideoSoftwareRendererPutPixels;

	renderer->d.disableBG = false;
	renderer->d.disableOBJ = false;
	renderer->d.disableWIN = false;

	renderer->temporaryBuffer = 0;
}

static void GBVideoSoftwareRendererInit(struct GBVideoRenderer* renderer, enum GBModel model) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	softwareRenderer->lcdc = 0;
	softwareRenderer->scy = 0;
	softwareRenderer->scx = 0;
	softwareRenderer->wy = 0;
	softwareRenderer->currentWy = 0;
	softwareRenderer->lastY = 0;
	softwareRenderer->hasWindow = false;
	softwareRenderer->wx = 0;
	softwareRenderer->model = model;
}

static void GBVideoSoftwareRendererDeinit(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	UNUSED(softwareRenderer);
}

static void GBVideoSoftwareRendererUpdateWindow(struct GBVideoSoftwareRenderer* renderer, bool before, bool after) {
	if (renderer->lastY >= GB_VIDEO_VERTICAL_PIXELS || after == before) {
		return;
	}
	if (renderer->lastY >= renderer->wy) {
		if (!after) {
			renderer->currentWy -= renderer->lastY;
			renderer->hasWindow = true;
		} else {
			if (!renderer->hasWindow) {
				renderer->currentWy = renderer->lastY - renderer->wy;
			} else {
				renderer->currentWy += renderer->lastY;
			}
		}
	}
}

static uint8_t GBVideoSoftwareRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	bool wasWindow = _inWindow(softwareRenderer);
	switch (address) {
	case REG_LCDC:
		softwareRenderer->lcdc = value;
		GBVideoSoftwareRendererUpdateWindow(softwareRenderer, wasWindow, _inWindow(softwareRenderer));
		break;
	case REG_SCY:
		softwareRenderer->scy = value;
		break;
	case REG_SCX:
		softwareRenderer->scx = value;
		break;
	case REG_WY:
		softwareRenderer->wy = value;
		GBVideoSoftwareRendererUpdateWindow(softwareRenderer, wasWindow, _inWindow(softwareRenderer));
		break;
	case REG_WX:
		softwareRenderer->wx = value;
		GBVideoSoftwareRendererUpdateWindow(softwareRenderer, wasWindow, _inWindow(softwareRenderer));
		break;
	}
	return value;
}

static void GBVideoSoftwareRendererWritePalette(struct GBVideoRenderer* renderer, int index, uint16_t value) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	color_t color = 0;
	color |= (value & 0x001F) << 11;
	color |= (value & 0x03E0) << 1;
	color |= (value & 0x7C00) >> 10;
#else
	color_t color = value;
#endif
#else
	color_t color = 0;
	color |= (value << 3) & 0xF8;
	color |= (value << 6) & 0xF800;
	color |= (value << 9) & 0xF80000;
	color |= (color >> 5) & 0x070707;
#endif
	softwareRenderer->palette[index] = color;
	if (renderer->cache) {
		mTileCacheWritePalette(renderer->cache, index << 1);
	}
}

static void GBVideoSoftwareRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address) {
	if (renderer->cache) {
		mTileCacheWriteVRAM(renderer->cache, address);
	}
}

static void GBVideoSoftwareRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam) {
	UNUSED(renderer);
	UNUSED(oam);
	// Nothing to do
}

static void GBVideoSoftwareRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj* obj, size_t oamMax) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	softwareRenderer->lastY = y;
	uint8_t* maps = &softwareRenderer->d.vram[GB_BASE_MAP];
	if (GBRegisterLCDCIsTileMap(softwareRenderer->lcdc)) {
		maps += GB_SIZE_MAP;
	}
	if (softwareRenderer->d.disableBG) {
		memset(&softwareRenderer->row[startX], 0, endX - startX);
	}
	if (GBRegisterLCDCIsBgEnable(softwareRenderer->lcdc) || softwareRenderer->model >= GB_MODEL_CGB) {
		int wy = softwareRenderer->wy + softwareRenderer->currentWy;
		if (GBRegisterLCDCIsWindow(softwareRenderer->lcdc) && wy <= y && endX >= softwareRenderer->wx - 7) {
			if (softwareRenderer->wx - 7 > 0 && !softwareRenderer->d.disableBG) {
				GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, startX, softwareRenderer->wx - 7, softwareRenderer->scx, softwareRenderer->scy + y);
			}

			maps = &softwareRenderer->d.vram[GB_BASE_MAP];
			if (GBRegisterLCDCIsWindowTileMap(softwareRenderer->lcdc)) {
				maps += GB_SIZE_MAP;
			}
			if (!softwareRenderer->d.disableWIN) {
				GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, softwareRenderer->wx - 7, endX, 7 - softwareRenderer->wx, y - wy);
			}
		} else if (!softwareRenderer->d.disableBG) {
			GBVideoSoftwareRendererDrawBackground(softwareRenderer, maps, startX, endX, softwareRenderer->scx, softwareRenderer->scy + y);
		}
	} else if (!softwareRenderer->d.disableBG) {
		memset(&softwareRenderer->row[startX], 0, endX - startX);
	}

	if (GBRegisterLCDCIsObjEnable(softwareRenderer->lcdc) && !softwareRenderer->d.disableOBJ) {
		size_t i;
		for (i = 0; i < oamMax; ++i) {
			GBVideoSoftwareRendererDrawObj(softwareRenderer, &obj[i], startX, endX, y);
		}
	}
	color_t* row = &softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * y];
	int x;
	for (x = startX; x + 7 < (endX & ~7); x += 8) {
		row[x] = softwareRenderer->palette[softwareRenderer->row[x] & 0x7F];
		row[x + 1] = softwareRenderer->palette[softwareRenderer->row[x + 1] & 0x7F];
		row[x + 2] = softwareRenderer->palette[softwareRenderer->row[x + 2] & 0x7F];
		row[x + 3] = softwareRenderer->palette[softwareRenderer->row[x + 3] & 0x7F];
		row[x + 4] = softwareRenderer->palette[softwareRenderer->row[x + 4] & 0x7F];
		row[x + 5] = softwareRenderer->palette[softwareRenderer->row[x + 5] & 0x7F];
		row[x + 6] = softwareRenderer->palette[softwareRenderer->row[x + 6] & 0x7F];
		row[x + 7] = softwareRenderer->palette[softwareRenderer->row[x + 7] & 0x7F];
	}
	for (; x < endX; ++x) {
		row[x] = softwareRenderer->palette[softwareRenderer->row[x] & 0x7F];
	}
}

static void GBVideoSoftwareRendererFinishScanline(struct GBVideoRenderer* renderer, int y) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
}

static void GBVideoSoftwareRendererFinishFrame(struct GBVideoRenderer* renderer) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;

	if (softwareRenderer->temporaryBuffer) {
		mappedMemoryFree(softwareRenderer->temporaryBuffer, GB_VIDEO_HORIZONTAL_PIXELS * GB_VIDEO_VERTICAL_PIXELS * 4);
		softwareRenderer->temporaryBuffer = 0;
	}
	if (!GBRegisterLCDCIsEnable(softwareRenderer->lcdc)) {
		_clearScreen(softwareRenderer);
	}
	softwareRenderer->lastY = GB_VIDEO_VERTICAL_PIXELS;
	softwareRenderer->currentWy = 0;
	softwareRenderer->hasWindow = false;
}

static void GBVideoSoftwareRendererDrawBackground(struct GBVideoSoftwareRenderer* renderer, uint8_t* maps, int startX, int endX, int sx, int sy) {
	uint8_t* data = renderer->d.vram;
	uint8_t* attr = &maps[GB_SIZE_VRAM_BANK0];
	if (!GBRegisterLCDCIsTileData(renderer->lcdc)) {
		data += 0x1000;
	}
	int topY = ((sy >> 3) & 0x1F) * 0x20;
	int bottomY = sy & 7;
	if (startX < 0) {
		startX = 0;
	}
	int x;
	if ((startX + sx) & 7) {
		int startX2 = startX + 8 - ((startX + sx) & 7);
		for (x = startX; x < startX2; ++x) {
			uint8_t* localData = data;
			int localY = bottomY;
			int topX = ((x + sx) >> 3) & 0x1F;
			int bottomX = 7 - ((x + sx) & 7);
			int bgTile;
			if (GBRegisterLCDCIsTileData(renderer->lcdc)) {
				bgTile = maps[topX + topY];
			} else {
				bgTile = ((int8_t*) maps)[topX + topY];
			}
			int p = 0;
			if (renderer->model >= GB_MODEL_CGB) {
				GBObjAttributes attrs = attr[topX + topY];
				p = GBObjAttributesGetCGBPalette(attrs) * 4;
				if (GBObjAttributesIsPriority(attrs) && GBRegisterLCDCIsBgEnable(renderer->lcdc)) {
					p |= 0x80;
				}
				if (GBObjAttributesIsBank(attrs)) {
					localData += GB_SIZE_VRAM_BANK0;
				}
				if (GBObjAttributesIsYFlip(attrs)) {
					localY = 7 - bottomY;
				}
				if (GBObjAttributesIsXFlip(attrs)) {
					bottomX = 7 - bottomX;
				}
			}
			uint8_t tileDataLower = localData[(bgTile * 8 + localY) * 2];
			uint8_t tileDataUpper = localData[(bgTile * 8 + localY) * 2 + 1];
			tileDataUpper >>= bottomX;
			tileDataLower >>= bottomX;
			renderer->row[x] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		}
		startX = startX2;
	}
	for (x = startX; x < endX; x += 8) {
		uint8_t* localData = data;
		int localY = bottomY;
		int topX = ((x + sx) >> 3) & 0x1F;
		int bgTile;
		if (GBRegisterLCDCIsTileData(renderer->lcdc)) {
			bgTile = maps[topX + topY];
		} else {
			bgTile = ((int8_t*) maps)[topX + topY];
		}
		int p = 0;
		if (renderer->model >= GB_MODEL_CGB) {
			GBObjAttributes attrs = attr[topX + topY];
			p = GBObjAttributesGetCGBPalette(attrs) * 4;
			if (GBObjAttributesIsPriority(attrs) && GBRegisterLCDCIsBgEnable(renderer->lcdc)) {
				p |= 0x80;
			}
			if (GBObjAttributesIsBank(attrs)) {
				localData += GB_SIZE_VRAM_BANK0;
			}
			if (GBObjAttributesIsYFlip(attrs)) {
				localY = 7 - bottomY;
			}
			if (GBObjAttributesIsXFlip(attrs)) {
				uint8_t tileDataLower = localData[(bgTile * 8 + localY) * 2];
				uint8_t tileDataUpper = localData[(bgTile * 8 + localY) * 2 + 1];
				renderer->row[x + 0] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
				renderer->row[x + 1] = p | (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
				renderer->row[x + 2] = p | ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
				renderer->row[x + 3] = p | ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
				renderer->row[x + 4] = p | ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
				renderer->row[x + 5] = p | ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
				renderer->row[x + 6] = p | ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
				renderer->row[x + 7] = p | ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
				continue;
			}
		}
		uint8_t tileDataLower = localData[(bgTile * 8 + localY) * 2];
		uint8_t tileDataUpper = localData[(bgTile * 8 + localY) * 2 + 1];
		renderer->row[x + 7] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		renderer->row[x + 6] = p | (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
		renderer->row[x + 5] = p | ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
		renderer->row[x + 4] = p | ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
		renderer->row[x + 3] = p | ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
		renderer->row[x + 2] = p | ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
		renderer->row[x + 1] = p | ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
		renderer->row[x + 0] = p | ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
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
	if (startX < 0) {
		startX = 0;
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
	if (GBRegisterLCDCIsObjSize(renderer->lcdc) && obj->tile & 1) {
		--tileOffset;
	}
	uint8_t mask = GBObjAttributesIsPriority(obj->attr) ? 0x63 : 0x60;
	uint8_t mask2 = GBObjAttributesIsPriority(obj->attr) ? 0 : 0x83;
	int p;
	if (renderer->model >= GB_MODEL_CGB) {
		p = (GBObjAttributesGetCGBPalette(obj->attr) + 8) * 4;
		if (GBObjAttributesIsBank(obj->attr)) {
			data += GB_SIZE_VRAM_BANK0;
		}
		if (!GBRegisterLCDCIsBgEnable(renderer->lcdc)) {
			mask = 0x60;
			mask2 = 0x83;
		}
	} else {
		p = (GBObjAttributesGetPalette(obj->attr) + 8) * 4;
	}
	int bottomX;
	int x = startX;
	if ((x - obj->x) & 7) {
		for (; x < endX; ++x) {
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
			if (((tileDataUpper | tileDataLower) & 1) && !(current & mask) && (current & mask2) <= 0x80) {
				renderer->row[x] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
			}
		}
	} else if (GBObjAttributesIsXFlip(obj->attr)) {
		int objTile = obj->tile + tileOffset;
		uint8_t tileDataLower = data[(objTile * 8 + bottomY) * 2];
		uint8_t tileDataUpper = data[(objTile * 8 + bottomY) * 2 + 1];
		color_t current;
		current = renderer->row[x];
		if (((tileDataUpper | tileDataLower) & 1) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		}
		current = renderer->row[x + 1];
		if (((tileDataUpper | tileDataLower) & 2) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 1] = p | (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
		}
		current = renderer->row[x + 2];
		if (((tileDataUpper | tileDataLower) & 4) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 2] = p | ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
		}
		current = renderer->row[x + 3];
		if (((tileDataUpper | tileDataLower) & 8) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 3] = p | ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
		}
		current = renderer->row[x + 4];
		if (((tileDataUpper | tileDataLower) & 16) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 4] = p | ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
		}
		current = renderer->row[x + 5];
		if (((tileDataUpper | tileDataLower) & 32) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 5] = p | ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
		}
		current = renderer->row[x + 6];
		if (((tileDataUpper | tileDataLower) & 64) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 6] = p | ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
		}
		current = renderer->row[x + 7];
		if (((tileDataUpper | tileDataLower) & 128) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 7] = p | ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
		}
	} else {
		int objTile = obj->tile + tileOffset;
		uint8_t tileDataLower = data[(objTile * 8 + bottomY) * 2];
		uint8_t tileDataUpper = data[(objTile * 8 + bottomY) * 2 + 1];
		color_t current;
		current = renderer->row[x + 7];
		if (((tileDataUpper | tileDataLower) & 1) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 7] = p | ((tileDataUpper & 1) << 1) | (tileDataLower & 1);
		}
		current = renderer->row[x + 6];
		if (((tileDataUpper | tileDataLower) & 2) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 6] = p | (tileDataUpper & 2) | ((tileDataLower & 2) >> 1);
		}
		current = renderer->row[x + 5];
		if (((tileDataUpper | tileDataLower) & 4) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 5] = p | ((tileDataUpper & 4) >> 1) | ((tileDataLower & 4) >> 2);
		}
		current = renderer->row[x + 4];
		if (((tileDataUpper | tileDataLower) & 8) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 4] = p | ((tileDataUpper & 8) >> 2) | ((tileDataLower & 8) >> 3);
		}
		current = renderer->row[x + 3];
		if (((tileDataUpper | tileDataLower) & 16) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 3] = p | ((tileDataUpper & 16) >> 3) | ((tileDataLower & 16) >> 4);
		}
		current = renderer->row[x + 2];
		if (((tileDataUpper | tileDataLower) & 32) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 2] = p | ((tileDataUpper & 32) >> 4) | ((tileDataLower & 32) >> 5);
		}
		current = renderer->row[x + 1];
		if (((tileDataUpper | tileDataLower) & 64) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x + 1] = p | ((tileDataUpper & 64) >> 5) | ((tileDataLower & 64) >> 6);
		}
		current = renderer->row[x];
		if (((tileDataUpper | tileDataLower) & 128) && !(current & mask) && (current & mask2) <= 0x80) {
			renderer->row[x] = p | ((tileDataUpper & 128) >> 6) | ((tileDataLower & 128) >> 7);
		}
	}
}

static void GBVideoSoftwareRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	*stride = softwareRenderer->outputBufferStride;
	*pixels = softwareRenderer->outputBuffer;
}

static void GBVideoSoftwareRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels) {
	struct GBVideoSoftwareRenderer* softwareRenderer = (struct GBVideoSoftwareRenderer*) renderer;
	// TODO: Share with GBAVideoSoftwareRendererGetPixels

	const color_t* colorPixels = pixels;
	unsigned i;
	for (i = 0; i < GB_VIDEO_VERTICAL_PIXELS; ++i) {
		memmove(&softwareRenderer->outputBuffer[softwareRenderer->outputBufferStride * i], &colorPixels[stride * i], GB_VIDEO_HORIZONTAL_PIXELS * BYTES_PER_PIXEL);
	}
}
