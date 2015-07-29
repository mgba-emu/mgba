/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "thread-proxy.h"

#include "gba/io.h"

#include "util/memory.h"

DEFINE_VECTOR(GBAVideoDirtyQueue, struct GBAVideoDirtyInfo);

static void GBAVideoThreadProxyRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoThreadProxyRendererReset(struct GBAVideoRenderer* renderer);
static void GBAVideoThreadProxyRendererDeinit(struct GBAVideoRenderer* renderer);
static uint16_t GBAVideoThreadProxyRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoThreadProxyRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address);
static void GBAVideoThreadProxyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoThreadProxyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoThreadProxyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoThreadProxyRendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoThreadProxyRendererGetPixels(struct GBAVideoRenderer* renderer, unsigned* stride, void** pixels);
static void GBAVideoThreadProxyRendererPutPixels(struct GBAVideoRenderer* renderer, unsigned stride, void* pixels);

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
	GBAVideoDirtyQueueInit(&proxyRenderer->dirtyQueue, 1024);
	proxyRenderer->threadState = PROXY_THREAD_STOPPED;

	proxyRenderer->vramProxy = anonymousMemoryMap(SIZE_VRAM);
	proxyRenderer->backend->palette = proxyRenderer->paletteProxy;
	proxyRenderer->backend->vram = proxyRenderer->vramProxy;
	proxyRenderer->backend->oam = &proxyRenderer->oamProxy;

	proxyRenderer->backend->init(proxyRenderer->backend);

	proxyRenderer->vramDirtyBitmap = 0;
	memset(proxyRenderer->oamDirtyBitmap, 0, sizeof(proxyRenderer->oamDirtyBitmap));
	proxyRenderer->threadState = PROXY_THREAD_IDLE;
	ThreadCreate(&proxyRenderer->thread, _proxyThread, proxyRenderer);
}

void GBAVideoThreadProxyRendererReset(struct GBAVideoRenderer* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	MutexLock(&proxyRenderer->mutex);
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	int i;
	for (i = 0; i < 128; ++i) {
		proxyRenderer->oamProxy.raw[i * 4] = 0x0200;
		proxyRenderer->oamProxy.raw[i * 4 + 1] = 0x0000;
		proxyRenderer->oamProxy.raw[i * 4 + 2] = 0x0000;
		proxyRenderer->oamProxy.raw[i * 4 + 3] = 0x0000;
	}
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
		value
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
	ConditionWake(&proxyRenderer->toThreadCond);
	return value;
}

void GBAVideoThreadProxyRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	int bit = 1 << (address >> 12);
	if (proxyRenderer->vramDirtyBitmap & bit) {
		return;
	}
	proxyRenderer->vramDirtyBitmap |= bit;
}

void GBAVideoThreadProxyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_PALETTE,
		address,
		value
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
	ConditionWake(&proxyRenderer->toThreadCond);
}

void GBAVideoThreadProxyRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	proxyRenderer->oamProxy.raw[oam] = proxyRenderer->d.oam->raw[oam];
	int bit = 1 << (oam & 31);
	int base = oam >> 5;
	if (proxyRenderer->oamDirtyBitmap[base] & bit) {
		return;
	}
	proxyRenderer->oamDirtyBitmap[base] |= bit;
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_OAM,
		oam,
		0
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
	ConditionWake(&proxyRenderer->toThreadCond);
}

void GBAVideoThreadProxyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_SCANLINE,
		y,
		0
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
	ConditionWake(&proxyRenderer->toThreadCond);
}

void GBAVideoThreadProxyRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	MutexLock(&proxyRenderer->mutex);
	// Insert an extra item into the queue to make sure it gets flushed
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_FLUSH,
		0,
		0
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
	do {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	} while (proxyRenderer->threadState == PROXY_THREAD_BUSY);
	proxyRenderer->backend->finishFrame(proxyRenderer->backend);
	GBAVideoDirtyQueueClear(&proxyRenderer->dirtyQueue);
	proxyRenderer->queueCleared = true;
	proxyRenderer->vramDirtyBitmap = 0;
	memset(proxyRenderer->oamDirtyBitmap, 0, sizeof(proxyRenderer->oamDirtyBitmap));
	MutexUnlock(&proxyRenderer->mutex);
}

void GBAVideoThreadProxyRendererGetPixels(struct GBAVideoRenderer* renderer, unsigned* stride, void** pixels) {
	// TODO
}

void GBAVideoThreadProxyRendererPutPixels(struct GBAVideoRenderer* renderer, unsigned stride, void* pixels) {
	// TODO
}

static THREAD_ENTRY _proxyThread(void* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = renderer;
	ThreadSetName("Proxy Renderer Thread");

	MutexLock(&proxyRenderer->mutex);
	size_t i = 0;
	while (1) {
		ConditionWait(&proxyRenderer->toThreadCond, &proxyRenderer->mutex);
		if (proxyRenderer->threadState == PROXY_THREAD_STOPPED) {
			break;
		}
		proxyRenderer->threadState = PROXY_THREAD_BUSY;
		if (proxyRenderer->queueCleared) {
			proxyRenderer->queueCleared = false;
			i = 0;
		}
		if (!GBAVideoDirtyQueueSize(&proxyRenderer->dirtyQueue)) {
			continue;
		}
		MutexUnlock(&proxyRenderer->mutex);
		for (; i < GBAVideoDirtyQueueSize(&proxyRenderer->dirtyQueue) - 1; ++i) {
			struct GBAVideoDirtyInfo* item = GBAVideoDirtyQueueGetPointer(&proxyRenderer->dirtyQueue, i);
			switch (item->type) {
			case DIRTY_REGISTER:
				proxyRenderer->backend->writeVideoRegister(proxyRenderer->backend, item->address, item->value);
				break;
			case DIRTY_PALETTE:
				proxyRenderer->paletteProxy[item->address >> 1] = item->value;
				proxyRenderer->backend->writePalette(proxyRenderer->backend, item->address, item->value);
				break;
			case DIRTY_OAM:
				proxyRenderer->backend->writeOAM(proxyRenderer->backend, item->address);
				break;
			case DIRTY_SCANLINE:
				if (proxyRenderer->vramDirtyBitmap) {
					int bitmap = proxyRenderer->vramDirtyBitmap;
					proxyRenderer->vramDirtyBitmap = 0;
					int j;
					for (j = 0; j < 24; ++j) {
						if (!(bitmap & (1 << j))) {
							continue;
						}
						proxyRenderer->backend->writeVRAM(proxyRenderer->backend, j * 0x1000);
						memcpy(&proxyRenderer->vramProxy[j * 0x800], &proxyRenderer->d.vram[j * 0x800], 0x1000);
					}
				}
				proxyRenderer->backend->drawScanline(proxyRenderer->backend, item->address);
				break;
			case DIRTY_FLUSH:
				// This is only here to ensure the queue gets flushed
				break;
			}
		}
		MutexLock(&proxyRenderer->mutex);
		proxyRenderer->threadState = PROXY_THREAD_IDLE;
		ConditionWake(&proxyRenderer->fromThreadCond);
	}
	MutexUnlock(&proxyRenderer->mutex);

	return 0;
}
