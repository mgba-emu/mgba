/* Copyright (c) 2013-2015 Jeffrey Pfau
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

enum GBAVideoDirtyType {
	DIRTY_DUMMY = 0,
	DIRTY_REGISTER,
	DIRTY_OAM,
	DIRTY_PALETTE,
	DIRTY_VRAM,
	DIRTY_SCANLINE,
	DIRTY_FLUSH
};

struct GBAVideoDirtyInfo {
	enum GBAVideoDirtyType type;
	uint32_t address;
	uint16_t value;
	uint32_t padding;
};

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

	renderer->backend = backend;
}

void GBAVideoThreadProxyRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	ConditionInit(&proxyRenderer->fromThreadCond);
	ConditionInit(&proxyRenderer->toThreadCond);
	MutexInit(&proxyRenderer->mutex);
	RingFIFOInit(&proxyRenderer->dirtyQueue, 0x40000);

	proxyRenderer->vramProxy = anonymousMemoryMap(SIZE_VRAM);
	proxyRenderer->backend->palette = proxyRenderer->paletteProxy;
	proxyRenderer->backend->vram = proxyRenderer->vramProxy;
	proxyRenderer->backend->oam = &proxyRenderer->oamProxy;
	proxyRenderer->backend->cache = NULL;

	proxyRenderer->backend->init(proxyRenderer->backend);

	proxyRenderer->vramDirtyBitmap = 0;
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
	memcpy(&proxyRenderer->oamProxy.raw, &renderer->oam->raw, SIZE_OAM);
	memcpy(proxyRenderer->paletteProxy, renderer->palette, SIZE_PALETTE_RAM);
	memcpy(proxyRenderer->vramProxy, renderer->vram, SIZE_VRAM);
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

	mappedMemoryFree(proxyRenderer->vramProxy, SIZE_VRAM);
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

static bool _writeData(struct GBAVideoThreadProxyRenderer* proxyRenderer, void* data, size_t length) {
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

	struct GBAVideoDirtyInfo dirty = {
		DIRTY_REGISTER,
		address,
		value,
		0xDEADBEEF,
	};
	_writeData(proxyRenderer, &dirty, sizeof(dirty));
	return value;
}

void GBAVideoThreadProxyRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	int bit = 1 << (address >> 12);
	if (proxyRenderer->vramDirtyBitmap & bit) {
		return;
	}
	proxyRenderer->vramDirtyBitmap |= bit;
	if (renderer->cache) {
		mTileCacheWriteVRAM(renderer->cache, address);
	}
}

void GBAVideoThreadProxyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_PALETTE,
		address,
		value,
		0xDEADBEEF,
	};
	_writeData(proxyRenderer, &dirty, sizeof(dirty));
	if (renderer->cache) {
		mTileCacheWritePalette(renderer->cache, address);
	}
}

void GBAVideoThreadProxyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_OAM,
		oam,
		proxyRenderer->d.oam->raw[oam],
		0xDEADBEEF,
	};
	_writeData(proxyRenderer, &dirty, sizeof(dirty));
}

void GBAVideoThreadProxyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	if (proxyRenderer->vramDirtyBitmap) {
		int bitmap = proxyRenderer->vramDirtyBitmap;
		proxyRenderer->vramDirtyBitmap = 0;
		int j;
		for (j = 0; j < 24; ++j) {
			if (!(bitmap & (1 << j))) {
				continue;
			}
			struct GBAVideoDirtyInfo dirty = {
				DIRTY_VRAM,
				j * 0x1000,
				0xABCD,
				0xDEADBEEF,
			};
			_writeData(proxyRenderer, &dirty, sizeof(dirty));
			_writeData(proxyRenderer, &proxyRenderer->d.vram[j * 0x800], 0x1000);
		}
	}
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_SCANLINE,
		y,
		0,
		0xDEADBEEF,
	};
	_writeData(proxyRenderer, &dirty, sizeof(dirty));
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
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_FLUSH,
		0,
		0,
		0xDEADBEEF,
	};
	_writeData(proxyRenderer, &dirty, sizeof(dirty));
	do {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	} while (proxyRenderer->threadState == PROXY_THREAD_BUSY);
	proxyRenderer->backend->finishFrame(proxyRenderer->backend);
	proxyRenderer->vramDirtyBitmap = 0;
	MutexUnlock(&proxyRenderer->mutex);
}

static void GBAVideoThreadProxyRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	MutexLock(&proxyRenderer->mutex);
	// Insert an extra item into the queue to make sure it gets flushed
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_FLUSH,
		0,
		0,
		0xDEADBEEF,
	};
	_writeData(proxyRenderer, &dirty, sizeof(dirty));
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
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_FLUSH,
		0,
		0,
		0xDEADBEEF,
	};
	_writeData(proxyRenderer, &dirty, sizeof(dirty));
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
	struct GBAVideoDirtyInfo item = {0};
	while (proxyRenderer->threadState != PROXY_THREAD_STOPPED) {
		ConditionWait(&proxyRenderer->toThreadCond, &proxyRenderer->mutex);
		if (proxyRenderer->threadState == PROXY_THREAD_STOPPED) {
			break;
		}
		if (RingFIFORead(&proxyRenderer->dirtyQueue, &item, sizeof(item))) {
			proxyRenderer->threadState = PROXY_THREAD_BUSY;
			MutexUnlock(&proxyRenderer->mutex);
			do {
				switch (item.type) {
				case DIRTY_REGISTER:
					proxyRenderer->backend->writeVideoRegister(proxyRenderer->backend, item.address, item.value);
					break;
				case DIRTY_PALETTE:
					proxyRenderer->paletteProxy[item.address >> 1] = item.value;
					proxyRenderer->backend->writePalette(proxyRenderer->backend, item.address, item.value);
					break;
				case DIRTY_OAM:
					proxyRenderer->oamProxy.raw[item.address] = item.value;
					proxyRenderer->backend->writeOAM(proxyRenderer->backend, item.address);
					break;
				case DIRTY_VRAM:
					while (!RingFIFORead(&proxyRenderer->dirtyQueue, &proxyRenderer->vramProxy[item.address >> 1], 0x1000)) {
						mLOG(GBA_VIDEO, DEBUG, "Proxy thread can't read VRAM. CPU thread asleep?");
						MutexLock(&proxyRenderer->mutex);
						ConditionWake(&proxyRenderer->fromThreadCond);
						ConditionWait(&proxyRenderer->toThreadCond, &proxyRenderer->mutex);
						MutexUnlock(&proxyRenderer->mutex);
					}
					proxyRenderer->backend->writeVRAM(proxyRenderer->backend, item.address);
					break;
				case DIRTY_SCANLINE:
					proxyRenderer->backend->drawScanline(proxyRenderer->backend, item.address);
					break;
				case DIRTY_FLUSH:
					MutexLock(&proxyRenderer->mutex);
					goto out;
				default:
					// FIFO was corrupted
					MutexLock(&proxyRenderer->mutex);
					proxyRenderer->threadState = PROXY_THREAD_STOPPED;
					mLOG(GBA_VIDEO, ERROR, "Proxy thread queue got corrupted!");
					goto out;
				}
			} while (proxyRenderer->threadState == PROXY_THREAD_BUSY && RingFIFORead(&proxyRenderer->dirtyQueue, &item, sizeof(item)));
			MutexLock(&proxyRenderer->mutex);
		}
		out:
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
