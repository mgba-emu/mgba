/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/renderers/thread-proxy.h>

#include <mgba/core/tile-cache.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#include <mgba-util/memory.h>

#ifndef DISABLE_THREADING

static void GBAVideoThreadProxyRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoThreadProxyRendererReset(struct GBAVideoRenderer* renderer);
static void GBAVideoThreadProxyRendererDeinit(struct GBAVideoRenderer* renderer);
static uint16_t GBAVideoThreadProxyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoThreadProxyRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address);
static void GBAVideoThreadProxyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoThreadProxyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoThreadProxyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoThreadProxyRendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoThreadProxyRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBAVideoThreadProxyRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels);

static THREAD_ENTRY _proxyThread(void* renderer);

static bool _writeData(struct mVideoProxy* proxy, const void* data, size_t length);
static bool _readData(struct mVideoProxy* proxy, void* data, size_t length, bool block);
static bool _parsePacket(struct mVideoProxy* proxy, const struct mVideoProxyDirtyInfo* packet);
static uint16_t* _vramBlock(struct mVideoProxy* proxy, uint32_t address);

void GBAVideoThreadProxyRendererCreate(struct GBAVideoThreadProxyRenderer* renderer, struct GBAVideoRenderer* backend) {
	renderer->d.init = GBAVideoThreadProxyRendererInit;
	renderer->d.reset = GBAVideoThreadProxyRendererReset;
	renderer->d.deinit = GBAVideoThreadProxyRendererDeinit;
	renderer->d.writeVideoRegister = GBAVideoThreadProxyRendererWriteVideoRegister;
	renderer->d.writeVRAM = GBAVideoThreadProxyRendererWriteVRAM;
	renderer->d.writeOAM = GBAVideoThreadProxyRendererWriteOAM;
	renderer->d.writePalette = GBAVideoThreadProxyRendererWritePalette;
	renderer->d.drawScanline = GBAVideoThreadProxyRendererDrawScanline;
	renderer->d.finishFrame = GBAVideoThreadProxyRendererFinishFrame;
	renderer->d.getPixels = GBAVideoThreadProxyRendererGetPixels;
	renderer->d.putPixels = GBAVideoThreadProxyRendererPutPixels;

	renderer->d.disableBG[0] = false;
	renderer->d.disableBG[1] = false;
	renderer->d.disableBG[2] = false;
	renderer->d.disableBG[3] = false;
	renderer->d.disableOBJ = false;

	renderer->proxy.context = renderer;
	renderer->proxy.writeData = _writeData;
	renderer->proxy.readData = _readData;
	renderer->proxy.parsePacket = _parsePacket;
	renderer->proxy.vramBlock = _vramBlock;
	renderer->proxy.paletteSize = SIZE_PALETTE_RAM;
	renderer->proxy.vramSize = SIZE_VRAM;
	renderer->proxy.oamSize = SIZE_OAM;

	renderer->backend = backend;
}

void GBAVideoThreadProxyRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	ConditionInit(&proxyRenderer->fromThreadCond);
	ConditionInit(&proxyRenderer->toThreadCond);
	MutexInit(&proxyRenderer->mutex);
	RingFIFOInit(&proxyRenderer->dirtyQueue, 0x40000);

	mVideoProxyRendererInit(&proxyRenderer->proxy);

	proxyRenderer->backend->palette = proxyRenderer->proxy.palette;
	proxyRenderer->backend->vram = proxyRenderer->proxy.vram;
	proxyRenderer->backend->oam = (union GBAOAM*) proxyRenderer->proxy.oam;
	proxyRenderer->backend->cache = NULL;

	proxyRenderer->backend->init(proxyRenderer->backend);

	proxyRenderer->threadState = PROXY_THREAD_IDLE;
	ThreadCreate(&proxyRenderer->thread, _proxyThread, proxyRenderer);
}

void GBAVideoThreadProxyRendererReset(struct GBAVideoRenderer* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	MutexLock(&proxyRenderer->mutex);
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	memcpy(proxyRenderer->proxy.oam, &renderer->oam->raw, SIZE_OAM);
	memcpy(proxyRenderer->proxy.palette, renderer->palette, SIZE_PALETTE_RAM);
	memcpy(proxyRenderer->proxy.vram, renderer->vram, SIZE_VRAM);

	mVideoProxyRendererReset(&proxyRenderer->proxy);

	proxyRenderer->backend->reset(proxyRenderer->backend);
	MutexUnlock(&proxyRenderer->mutex);
}

void GBAVideoThreadProxyRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	bool waiting = false;
	MutexLock(&proxyRenderer->mutex);
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	if (proxyRenderer->threadState == PROXY_THREAD_IDLE) {
		proxyRenderer->threadState = PROXY_THREAD_STOPPED;
		ConditionWake(&proxyRenderer->toThreadCond);
		waiting = true;
	}
	MutexUnlock(&proxyRenderer->mutex);
	if (waiting) {
		ThreadJoin(proxyRenderer->thread);
	}
	ConditionDeinit(&proxyRenderer->fromThreadCond);
	ConditionDeinit(&proxyRenderer->toThreadCond);
	MutexDeinit(&proxyRenderer->mutex);
	proxyRenderer->backend->deinit(proxyRenderer->backend);

	mVideoProxyRendererDeinit(&proxyRenderer->proxy);
}

void _proxyThreadRecover(struct GBAVideoThreadProxyRenderer* proxyRenderer) {
	MutexLock(&proxyRenderer->mutex);
	if (proxyRenderer->threadState != PROXY_THREAD_STOPPED) {
		MutexUnlock(&proxyRenderer->mutex);
		return;
	}
	RingFIFOClear(&proxyRenderer->dirtyQueue);
	MutexUnlock(&proxyRenderer->mutex);
	ThreadJoin(proxyRenderer->thread);
	proxyRenderer->threadState = PROXY_THREAD_IDLE;
	ThreadCreate(&proxyRenderer->thread, _proxyThread, proxyRenderer);
}

static bool _writeData(struct mVideoProxy* proxy, const void* data, size_t length) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = proxy->context;
	while (!RingFIFOWrite(&proxyRenderer->dirtyQueue, data, length)) {
		mLOG(GBA_VIDEO, DEBUG, "Can't write %"PRIz"u bytes. Proxy thread asleep?", length);
		MutexLock(&proxyRenderer->mutex);
		if (proxyRenderer->threadState == PROXY_THREAD_STOPPED) {
			mLOG(GBA_VIDEO, ERROR, "Proxy thread stopped prematurely!");
			MutexUnlock(&proxyRenderer->mutex);
			return false;
		}
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
		MutexUnlock(&proxyRenderer->mutex);
	}
	return true;
}

static bool _readData(struct mVideoProxy* proxy, void* data, size_t length, bool block) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = proxy->context;
	bool read = false;
	while (true) {
		read = RingFIFORead(&proxyRenderer->dirtyQueue, data, length);
		if (!block || read) {
			break;
		}
		mLOG(GBA_VIDEO, DEBUG, "Proxy thread can't read VRAM. CPU thread asleep?");
		MutexLock(&proxyRenderer->mutex);
		ConditionWake(&proxyRenderer->fromThreadCond);
		ConditionWait(&proxyRenderer->toThreadCond, &proxyRenderer->mutex);
		MutexUnlock(&proxyRenderer->mutex);
	}
	return read;
}

static bool _parsePacket(struct mVideoProxy* proxy, const struct mVideoProxyDirtyInfo* item) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = proxy->context;
	switch (item->type) {
	case DIRTY_REGISTER:
		proxyRenderer->backend->writeVideoRegister(proxyRenderer->backend, item->address, item->value);
		break;
	case DIRTY_PALETTE:
		proxy->palette[item->address >> 1] = item->value;
		proxyRenderer->backend->writePalette(proxyRenderer->backend, item->address, item->value);
		break;
	case DIRTY_OAM:
		proxy->oam[item->address] = item->value;
		proxyRenderer->backend->writeOAM(proxyRenderer->backend, item->address);
		break;
	case DIRTY_VRAM:
		proxy->readData(proxy, &proxy->vram[item->address >> 1], 0x1000, true);
		proxyRenderer->backend->writeVRAM(proxyRenderer->backend, item->address);
		break;
	case DIRTY_SCANLINE:
		proxyRenderer->backend->drawScanline(proxyRenderer->backend, item->address);
		break;
	case DIRTY_FLUSH:
		return false;
	default:
		return false;
	}
	return true;
}

static uint16_t* _vramBlock(struct mVideoProxy* proxy, uint32_t address) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = proxy->context;
	return &proxyRenderer->d.vram[address >> 1];
}

uint16_t GBAVideoThreadProxyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	switch (address) {
	case REG_BG0CNT:
	case REG_BG1CNT:
	case REG_BG2CNT:
	case REG_BG3CNT:
		value &= 0xFFCF;
		break;
	case REG_BG0HOFS:
	case REG_BG0VOFS:
	case REG_BG1HOFS:
	case REG_BG1VOFS:
	case REG_BG2HOFS:
	case REG_BG2VOFS:
	case REG_BG3HOFS:
	case REG_BG3VOFS:
		value &= 0x01FF;
		break;
	}
	if (address > REG_BLDY) {
		return value;
	}

	mVideoProxyRendererWriteVideoRegister(&proxyRenderer->proxy, address, value);
	return value;
}

void GBAVideoThreadProxyRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	mVideoProxyRendererWriteVRAM(&proxyRenderer->proxy, address);
	if (renderer->cache) {
		mTileCacheWriteVRAM(renderer->cache, address);
	}
}

void GBAVideoThreadProxyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	mVideoProxyRendererWritePalette(&proxyRenderer->proxy, address, value);
	if (renderer->cache) {
		mTileCacheWritePalette(renderer->cache, address);
	}
}

void GBAVideoThreadProxyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	mVideoProxyRendererWriteOAM(&proxyRenderer->proxy, oam, proxyRenderer->d.oam->raw[oam]);
}

void GBAVideoThreadProxyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	mVideoProxyRendererDrawScanline(&proxyRenderer->proxy, y);
	if ((y & 15) == 15) {
		ConditionWake(&proxyRenderer->toThreadCond);
	}
}

void GBAVideoThreadProxyRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	if (proxyRenderer->threadState == PROXY_THREAD_STOPPED) {
		mLOG(GBA_VIDEO, ERROR, "Proxy thread stopped prematurely!");
		_proxyThreadRecover(proxyRenderer);
		return;
	}
	MutexLock(&proxyRenderer->mutex);
	// Insert an extra item into the queue to make sure it gets flushed
	mVideoProxyRendererFlush(&proxyRenderer->proxy);
	do {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	} while (proxyRenderer->threadState == PROXY_THREAD_BUSY);
	proxyRenderer->backend->finishFrame(proxyRenderer->backend);
	MutexUnlock(&proxyRenderer->mutex);
}

static void GBAVideoThreadProxyRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	MutexLock(&proxyRenderer->mutex);
	// Insert an extra item into the queue to make sure it gets flushed
	mVideoProxyRendererFlush(&proxyRenderer->proxy);
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	proxyRenderer->backend->getPixels(proxyRenderer->backend, stride, pixels);
	MutexUnlock(&proxyRenderer->mutex);
}

static void GBAVideoThreadProxyRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	MutexLock(&proxyRenderer->mutex);
	// Insert an extra item into the queue to make sure it gets flushed
	mVideoProxyRendererFlush(&proxyRenderer->proxy);
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	proxyRenderer->backend->putPixels(proxyRenderer->backend, stride, pixels);
	MutexUnlock(&proxyRenderer->mutex);
}

static THREAD_ENTRY _proxyThread(void* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = renderer;
	ThreadSetName("Proxy Renderer Thread");

	MutexLock(&proxyRenderer->mutex);
	while (proxyRenderer->threadState != PROXY_THREAD_STOPPED) {
		ConditionWait(&proxyRenderer->toThreadCond, &proxyRenderer->mutex);
		if (proxyRenderer->threadState == PROXY_THREAD_STOPPED) {
			break;
		}
		proxyRenderer->threadState = PROXY_THREAD_BUSY;
		MutexUnlock(&proxyRenderer->mutex);
		if (!mVideoProxyRendererRun(&proxyRenderer->proxy)) {
			// FIFO was corrupted
			proxyRenderer->threadState = PROXY_THREAD_STOPPED;
			mLOG(GBA_VIDEO, ERROR, "Proxy thread queue got corrupted!");
		}
		MutexLock(&proxyRenderer->mutex);
		ConditionWake(&proxyRenderer->fromThreadCond);
		if (proxyRenderer->threadState != PROXY_THREAD_STOPPED) {
			proxyRenderer->threadState = PROXY_THREAD_IDLE;
		}
	}
	MutexUnlock(&proxyRenderer->mutex);

#ifdef _3DS
	svcExitThread();
#endif
	return 0;
}

#endif
