/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/renderers/proxy.h>

#include <mgba/core/cache-set.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>

#define BUFFER_OAM 1
#define BUFFER_SGB 2

static void GBVideoProxyRendererInit(struct GBVideoRenderer* renderer, enum GBModel model, bool borders);
static void GBVideoProxyRendererDeinit(struct GBVideoRenderer* renderer);
static uint8_t GBVideoProxyRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoProxyRendererWriteSGBPacket(struct GBVideoRenderer* renderer, uint8_t* data);
static void GBVideoProxyRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address);
static void GBVideoProxyRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam);
static void GBVideoProxyRendererWritePalette(struct GBVideoRenderer* renderer, int address, uint16_t value);
static void GBVideoProxyRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y);
static void GBVideoProxyRendererFinishScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoProxyRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoProxyRendererEnableSGBBorder(struct GBVideoRenderer* renderer, bool enable);
static void GBVideoProxyRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBVideoProxyRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels);

static bool _parsePacket(struct mVideoLogger* logger, const struct mVideoLoggerDirtyInfo* packet);
static uint16_t* _vramBlock(struct mVideoLogger* logger, uint32_t address);

void GBVideoProxyRendererCreate(struct GBVideoProxyRenderer* renderer, struct GBVideoRenderer* backend) {
	renderer->d.init = GBVideoProxyRendererInit;
	renderer->d.deinit = GBVideoProxyRendererDeinit;
	renderer->d.writeVideoRegister = GBVideoProxyRendererWriteVideoRegister;
	renderer->d.writeSGBPacket = GBVideoProxyRendererWriteSGBPacket;
	renderer->d.writeVRAM = GBVideoProxyRendererWriteVRAM;
	renderer->d.writeOAM = GBVideoProxyRendererWriteOAM;
	renderer->d.writePalette = GBVideoProxyRendererWritePalette;
	renderer->d.drawRange = GBVideoProxyRendererDrawRange;
	renderer->d.finishScanline = GBVideoProxyRendererFinishScanline;
	renderer->d.finishFrame = GBVideoProxyRendererFinishFrame;
	renderer->d.enableSGBBorder = GBVideoProxyRendererEnableSGBBorder;
	renderer->d.getPixels = GBVideoProxyRendererGetPixels;
	renderer->d.putPixels = GBVideoProxyRendererPutPixels;

	renderer->d.disableBG = false;
	renderer->d.disableWIN = false;
	renderer->d.disableOBJ = false;

	renderer->d.highlightBG = false;
	renderer->d.highlightWIN = false;
	int i;
	for (i = 0; i < GB_VIDEO_MAX_OBJ; ++i) {
		renderer->d.highlightOBJ[i] = false;
	}
	renderer->d.highlightColor = M_COLOR_WHITE;
	renderer->d.highlightAmount = 0;

	renderer->logger->context = renderer;
	renderer->logger->parsePacket = _parsePacket;
	renderer->logger->vramBlock = _vramBlock;
	renderer->logger->paletteSize = 0;
	renderer->logger->vramSize = GB_SIZE_VRAM;
	renderer->logger->oamSize = GB_SIZE_OAM;

	renderer->backend = backend;
}

static void _init(struct GBVideoProxyRenderer* proxyRenderer) {
	mVideoLoggerRendererInit(proxyRenderer->logger);

	if (proxyRenderer->logger->block) {
		proxyRenderer->backend->vram = (uint8_t*) proxyRenderer->logger->vram;
		proxyRenderer->backend->oam = (union GBOAM*) proxyRenderer->logger->oam;
		proxyRenderer->backend->cache = NULL;
	}
}

static void _reset(struct GBVideoProxyRenderer* proxyRenderer) {
	memcpy(proxyRenderer->logger->oam, &proxyRenderer->d.oam->raw, GB_SIZE_OAM);
	memcpy(proxyRenderer->logger->vram, proxyRenderer->d.vram, GB_SIZE_VRAM);

	mVideoLoggerRendererReset(proxyRenderer->logger);
}

static void _copyExtraState(struct GBVideoProxyRenderer* proxyRenderer) {
	proxyRenderer->backend->disableBG = proxyRenderer->d.disableBG;
	proxyRenderer->backend->disableWIN = proxyRenderer->d.disableWIN;
	proxyRenderer->backend->disableOBJ = proxyRenderer->d.disableOBJ;
	proxyRenderer->backend->highlightBG = proxyRenderer->d.highlightBG;
	proxyRenderer->backend->highlightWIN = proxyRenderer->d.highlightWIN;
	memcpy(proxyRenderer->backend->highlightOBJ, proxyRenderer->d.highlightOBJ, sizeof(proxyRenderer->backend->highlightOBJ));
	proxyRenderer->backend->highlightAmount = proxyRenderer->d.highlightAmount;
	proxyRenderer->backend->highlightColor = proxyRenderer->d.highlightColor;
}

void GBVideoProxyRendererShim(struct GBVideo* video, struct GBVideoProxyRenderer* renderer) {
	if ((renderer->backend && video->renderer != renderer->backend) || video->renderer == &renderer->d) {
		return;
	}
	renderer->backend = video->renderer;
	video->renderer = &renderer->d;
	renderer->d.cache = renderer->backend->cache;
	renderer->d.sgbRenderMode = renderer->backend->sgbRenderMode;
	renderer->d.sgbCharRam = renderer->backend->sgbCharRam;
	renderer->d.sgbMapRam = renderer->backend->sgbMapRam;
	renderer->d.sgbPalRam = renderer->backend->sgbPalRam;
	renderer->d.sgbAttributeFiles = renderer->backend->sgbAttributeFiles;
	renderer->d.sgbAttributes = renderer->backend->sgbAttributes;
	renderer->d.vram = video->vram;
	renderer->d.oam = &video->oam;
	_init(renderer);
	_reset(renderer);
}

void GBVideoProxyRendererUnshim(struct GBVideo* video, struct GBVideoProxyRenderer* renderer) {
	if (video->renderer != &renderer->d) {
		return;
	}
	renderer->backend->cache = video->renderer->cache;
	video->renderer = renderer->backend;
	renderer->backend->vram = video->vram;
	renderer->backend->oam = &video->oam;

	mVideoLoggerRendererDeinit(renderer->logger);
}

void GBVideoProxyRendererInit(struct GBVideoRenderer* renderer, enum GBModel model, bool borders) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;

	_init(proxyRenderer);

	proxyRenderer->model = model;
	proxyRenderer->backend->init(proxyRenderer->backend, model, borders);
}

void GBVideoProxyRendererDeinit(struct GBVideoRenderer* renderer) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;

	proxyRenderer->backend->deinit(proxyRenderer->backend);

	mVideoLoggerRendererDeinit(proxyRenderer->logger);
}

static bool _parsePacket(struct mVideoLogger* logger, const struct mVideoLoggerDirtyInfo* item) {
	struct GBVideoProxyRenderer* proxyRenderer = logger->context;
	uint8_t sgbPacket[16];
	struct GBObj legacyBuffer[GB_VIDEO_MAX_OBJ];
	switch (item->type) {
	case DIRTY_REGISTER:
		proxyRenderer->backend->writeVideoRegister(proxyRenderer->backend, item->address, item->value);
		break;
	case DIRTY_PALETTE:
		if (item->address < 64) {
			proxyRenderer->backend->writePalette(proxyRenderer->backend, item->address, item->value);
		}
		break;
	case DIRTY_OAM:
		if (item->address < GB_SIZE_OAM) {
			((uint8_t*) logger->oam)[item->address] = item->value;
			proxyRenderer->backend->writeOAM(proxyRenderer->backend, item->address);
		}
		break;
	case DIRTY_VRAM:
		if (item->address <= GB_SIZE_VRAM - 0x1000) {
			logger->readData(logger, &logger->vram[item->address >> 1], 0x1000, true);
			proxyRenderer->backend->writeVRAM(proxyRenderer->backend, item->address);
		}
		break;
	case DIRTY_SCANLINE:
		_copyExtraState(proxyRenderer);
		if (item->address < GB_VIDEO_VERTICAL_PIXELS) {
			proxyRenderer->backend->finishScanline(proxyRenderer->backend, item->address);
		}
		break;
	case DIRTY_RANGE:
		if (item->value < item->value2 && item->value2 <= GB_VIDEO_HORIZONTAL_PIXELS && item->address < GB_VIDEO_VERTICAL_PIXELS) {
			proxyRenderer->backend->drawRange(proxyRenderer->backend, item->value, item->value2, item->address);
		}
		break;
	case DIRTY_FRAME:
		proxyRenderer->backend->finishFrame(proxyRenderer->backend);
		break;
	case DIRTY_BUFFER:
		switch (item->address) {
		case BUFFER_OAM:
			if (item->value2 / sizeof(struct GBObj) > GB_VIDEO_MAX_OBJ) {
				return false;
			}
			logger->readData(logger, legacyBuffer, item->value2, true);
			break;
		case BUFFER_SGB:
			logger->readData(logger, sgbPacket, 16, true);
			if (proxyRenderer->model & GB_MODEL_SGB) {
				proxyRenderer->backend->writeSGBPacket(proxyRenderer->backend, sgbPacket);
			}
			break;
		}
		break;
	case DIRTY_FLUSH:
		return false;
	default:
		return false;
	}
	return true;
}

static uint16_t* _vramBlock(struct mVideoLogger* logger, uint32_t address) {
	struct GBVideoProxyRenderer* proxyRenderer = logger->context;
	return (uint16_t*) &proxyRenderer->d.vram[address];
}

uint8_t GBVideoProxyRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;

	mVideoLoggerRendererWriteVideoRegister(proxyRenderer->logger, address, value);
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->writeVideoRegister(proxyRenderer->backend, address, value);
	}
	return value;
}

void GBVideoProxyRendererWriteSGBPacket(struct GBVideoRenderer* renderer, uint8_t* data) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->writeSGBPacket(proxyRenderer->backend, data);
	}
	mVideoLoggerWriteBuffer(proxyRenderer->logger, BUFFER_SGB, 0, 16, data);
}

void GBVideoProxyRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	mVideoLoggerRendererWriteVRAM(proxyRenderer->logger, address);
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->writeVRAM(proxyRenderer->backend, address);
	}
	if (renderer->cache) {
		mCacheSetWriteVRAM(renderer->cache, address);
	}
}

void GBVideoProxyRendererWritePalette(struct GBVideoRenderer* renderer, int address, uint16_t value) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	mVideoLoggerRendererWritePalette(proxyRenderer->logger, address, value);
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->writePalette(proxyRenderer->backend, address, value);
	}
	if (renderer->cache) {
		mCacheSetWritePalette(renderer->cache, address, mColorFrom555(value));
	}
}

void GBVideoProxyRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->writeOAM(proxyRenderer->backend, oam);
	}
	mVideoLoggerRendererWriteOAM(proxyRenderer->logger, oam, ((uint8_t*) proxyRenderer->d.oam->raw)[oam]);
}

void GBVideoProxyRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (!proxyRenderer->logger->block) {
		_copyExtraState(proxyRenderer);
		proxyRenderer->backend->drawRange(proxyRenderer->backend, startX, endX, y);
	}
	mVideoLoggerRendererDrawRange(proxyRenderer->logger, startX, endX, y);	
}

void GBVideoProxyRendererFinishScanline(struct GBVideoRenderer* renderer, int y) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->finishScanline(proxyRenderer->backend, y);
	}
	mVideoLoggerRendererDrawScanline(proxyRenderer->logger, y);
	if (proxyRenderer->logger->block && proxyRenderer->logger->wake) {
		proxyRenderer->logger->wake(proxyRenderer->logger, y);
	}
}

void GBVideoProxyRendererFinishFrame(struct GBVideoRenderer* renderer) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->finishFrame(proxyRenderer->backend);
	}
	mVideoLoggerRendererFinishFrame(proxyRenderer->logger);
	mVideoLoggerRendererFlush(proxyRenderer->logger);
}

static void GBVideoProxyRendererEnableSGBBorder(struct GBVideoRenderer* renderer, bool enable) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		// Insert an extra item into the queue to make sure it gets flushed
		mVideoLoggerRendererFlush(proxyRenderer->logger);
		proxyRenderer->logger->wait(proxyRenderer->logger);
	}
	proxyRenderer->backend->enableSGBBorder(proxyRenderer->backend, enable);
}

static void GBVideoProxyRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->lock(proxyRenderer->logger);
		// Insert an extra item into the queue to make sure it gets flushed
		mVideoLoggerRendererFlush(proxyRenderer->logger);
		proxyRenderer->logger->postEvent(proxyRenderer->logger, LOGGER_EVENT_GET_PIXELS);
		mVideoLoggerRendererFlush(proxyRenderer->logger);
		proxyRenderer->logger->unlock(proxyRenderer->logger);
		*pixels = proxyRenderer->logger->pixelBuffer;
		*stride = proxyRenderer->logger->pixelStride;
	} else {
		proxyRenderer->backend->getPixels(proxyRenderer->backend, stride, pixels);
	}
}

static void GBVideoProxyRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->lock(proxyRenderer->logger);
	}
	proxyRenderer->backend->putPixels(proxyRenderer->backend, stride, pixels);
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->unlock(proxyRenderer->logger);
	}
}
