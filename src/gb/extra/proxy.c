/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/renderers/proxy.h>

#include <mgba/core/tile-cache.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>

#define BUFFER_OAM 1

static void GBVideoProxyRendererInit(struct GBVideoRenderer* renderer, enum GBModel model);
static void GBVideoProxyRendererDeinit(struct GBVideoRenderer* renderer);
static uint8_t GBVideoProxyRendererWriteVideoRegister(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
static void GBVideoProxyRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address);
static void GBVideoProxyRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam);
static void GBVideoProxyRendererWritePalette(struct GBVideoRenderer* renderer, int address, uint16_t value);
static void GBVideoProxyRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj* obj, size_t oamMax);
static void GBVideoProxyRendererFinishScanline(struct GBVideoRenderer* renderer, int y);
static void GBVideoProxyRendererFinishFrame(struct GBVideoRenderer* renderer);
static void GBVideoProxyRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBVideoProxyRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels);

static bool _parsePacket(struct mVideoLogger* logger, const struct mVideoLoggerDirtyInfo* packet);
static uint16_t* _vramBlock(struct mVideoLogger* logger, uint32_t address);

void GBVideoProxyRendererCreate(struct GBVideoProxyRenderer* renderer, struct GBVideoRenderer* backend) {
	renderer->d.init = GBVideoProxyRendererInit;
	renderer->d.deinit = GBVideoProxyRendererDeinit;
	renderer->d.writeVideoRegister = GBVideoProxyRendererWriteVideoRegister;
	renderer->d.writeVRAM = GBVideoProxyRendererWriteVRAM;
	renderer->d.writeOAM = GBVideoProxyRendererWriteOAM;
	renderer->d.writePalette = GBVideoProxyRendererWritePalette;
	renderer->d.drawRange = GBVideoProxyRendererDrawRange;
	renderer->d.finishScanline = GBVideoProxyRendererFinishScanline;
	renderer->d.finishFrame = GBVideoProxyRendererFinishFrame;
	renderer->d.getPixels = GBVideoProxyRendererGetPixels;
	renderer->d.putPixels = GBVideoProxyRendererPutPixels;

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

static void _reset(struct GBVideoProxyRenderer* proxyRenderer, enum GBModel model) {
	memcpy(proxyRenderer->logger->oam, &proxyRenderer->d.oam->raw, GB_SIZE_OAM);
	memcpy(proxyRenderer->logger->vram, proxyRenderer->d.vram, GB_SIZE_VRAM);

	proxyRenderer->oamMax = 0;

	mVideoLoggerRendererReset(proxyRenderer->logger);
}

void GBVideoProxyRendererShim(struct GBVideo* video, struct GBVideoProxyRenderer* renderer) {
	if ((renderer->backend && video->renderer != renderer->backend) || video->renderer == &renderer->d) {
		return;
	}
	renderer->backend = video->renderer;
	video->renderer = &renderer->d;
	renderer->d.cache = renderer->backend->cache;
	renderer->d.vram = video->vram;
	renderer->d.oam = &video->oam;
	_init(renderer);
	_reset(renderer, video->p->model);
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

void GBVideoProxyRendererInit(struct GBVideoRenderer* renderer, enum GBModel model) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;

	_init(proxyRenderer);

	proxyRenderer->backend->init(proxyRenderer->backend, model);
}

void GBVideoProxyRendererDeinit(struct GBVideoRenderer* renderer) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;

	proxyRenderer->backend->deinit(proxyRenderer->backend);

	mVideoLoggerRendererDeinit(proxyRenderer->logger);
}

static bool _parsePacket(struct mVideoLogger* logger, const struct mVideoLoggerDirtyInfo* item) {
	struct GBVideoProxyRenderer* proxyRenderer = logger->context;
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
			logger->oam[item->address] = item->value;
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
		if (item->address < GB_VIDEO_VERTICAL_PIXELS) {
			proxyRenderer->backend->finishScanline(proxyRenderer->backend, item->address);
		}
		break;
	case DIRTY_RANGE:
		if (item->value < item->value2 && item->value2 <= GB_VIDEO_HORIZONTAL_PIXELS && item->address < GB_VIDEO_VERTICAL_PIXELS) {
			proxyRenderer->backend->drawRange(proxyRenderer->backend, item->value, item->value2, item->address, proxyRenderer->objThisLine, proxyRenderer->oamMax);
		}
		break;
	case DIRTY_FRAME:
		proxyRenderer->backend->finishFrame(proxyRenderer->backend);
		break;
	case DIRTY_BUFFER:
		switch (item->address) {
		case BUFFER_OAM:
			proxyRenderer->oamMax = item->value2 / sizeof(struct GBObj);
			if (proxyRenderer->oamMax > 40) {
				proxyRenderer->oamMax = 0;
				return false;
			}
			logger->readData(logger, &proxyRenderer->objThisLine, item->value2, true);
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

void GBVideoProxyRendererWriteVRAM(struct GBVideoRenderer* renderer, uint16_t address) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	mVideoLoggerRendererWriteVRAM(proxyRenderer->logger, address);
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->writeVRAM(proxyRenderer->backend, address);
	}
	if (renderer->cache) {
		mTileCacheWriteVRAM(renderer->cache, address);
	}
}

void GBVideoProxyRendererWritePalette(struct GBVideoRenderer* renderer, int address, uint16_t value) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	mVideoLoggerRendererWritePalette(proxyRenderer->logger, address, value);
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->writePalette(proxyRenderer->backend, address, value);
	}
	if (renderer->cache) {
		mTileCacheWritePalette(renderer->cache, address);
	}
}

void GBVideoProxyRendererWriteOAM(struct GBVideoRenderer* renderer, uint16_t oam) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->writeOAM(proxyRenderer->backend, oam);
	}
	mVideoLoggerRendererWriteOAM(proxyRenderer->logger, oam, ((uint8_t*) proxyRenderer->d.oam->raw)[oam]);
}

void GBVideoProxyRendererDrawRange(struct GBVideoRenderer* renderer, int startX, int endX, int y, struct GBObj* obj, size_t oamMax) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (!proxyRenderer->logger->block) {
		proxyRenderer->backend->drawRange(proxyRenderer->backend, startX, endX, y, obj, oamMax);
	}
	mVideoLoggerWriteBuffer(proxyRenderer->logger, BUFFER_OAM, 0, oamMax * sizeof(*obj), obj);	
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
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->lock(proxyRenderer->logger);
		proxyRenderer->logger->wait(proxyRenderer->logger);
	}
	proxyRenderer->backend->finishFrame(proxyRenderer->backend);
	mVideoLoggerRendererFinishFrame(proxyRenderer->logger);
	mVideoLoggerRendererFlush(proxyRenderer->logger);
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->unlock(proxyRenderer->logger);
	}
}

static void GBVideoProxyRendererGetPixels(struct GBVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->lock(proxyRenderer->logger);
		// Insert an extra item into the queue to make sure it gets flushed
		mVideoLoggerRendererFlush(proxyRenderer->logger);
		proxyRenderer->logger->wait(proxyRenderer->logger);
	}
	proxyRenderer->backend->getPixels(proxyRenderer->backend, stride, pixels);
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->unlock(proxyRenderer->logger);
	}
}

static void GBVideoProxyRendererPutPixels(struct GBVideoRenderer* renderer, size_t stride, const void* pixels) {
	struct GBVideoProxyRenderer* proxyRenderer = (struct GBVideoProxyRenderer*) renderer;
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->lock(proxyRenderer->logger);
		// Insert an extra item into the queue to make sure it gets flushed
		mVideoLoggerRendererFlush(proxyRenderer->logger);
		proxyRenderer->logger->wait(proxyRenderer->logger);
	}
	proxyRenderer->backend->putPixels(proxyRenderer->backend, stride, pixels);
	if (proxyRenderer->logger->block && proxyRenderer->logger->wait) {
		proxyRenderer->logger->unlock(proxyRenderer->logger);
	}
}
