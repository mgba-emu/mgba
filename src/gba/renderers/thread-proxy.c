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
	memset(proxyRenderer->paletteDirtyBitmap, 0, sizeof(proxyRenderer->paletteDirtyBitmap));
	memset(proxyRenderer->regDirtyBitmap, 0, sizeof(proxyRenderer->regDirtyBitmap));
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
	proxyRenderer->regProxy[address >> 1] = value;
	int bit = 1 << ((address >> 1) & 31);
	int base = address >> 6;
	if (proxyRenderer->regDirtyBitmap[base] & bit) {
		return value;
	}
	proxyRenderer->regDirtyBitmap[base] |= bit;

	struct GBAVideoDirtyInfo dirty = {
		DIRTY_REGISTER,
		address
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
	return value;
}

void GBAVideoThreadProxyRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	int bit = 1 << (address >> 12);
	if (proxyRenderer->vramDirtyBitmap & bit) {
		return;
	}
	proxyRenderer->vramDirtyBitmap |= bit;
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_VRAM,
		address
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
}

void GBAVideoThreadProxyRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	proxyRenderer->paletteProxy[address >> 1] = value;
	int bit = 1 << ((address >> 1) & 31);
	int base = address >> 6;
	if (proxyRenderer->paletteDirtyBitmap[base] & bit) {
		return;
	}
	proxyRenderer->paletteDirtyBitmap[base] |= bit;
	struct GBAVideoDirtyInfo dirty = {
		DIRTY_PALETTE,
		address
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
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
		oam
	};
	*GBAVideoDirtyQueueAppend(&proxyRenderer->dirtyQueue) = dirty;
}

void GBAVideoThreadProxyRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	MutexLock(&proxyRenderer->mutex);
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	proxyRenderer->y = y;
	ConditionWake(&proxyRenderer->toThreadCond);
	while (proxyRenderer->threadState == PROXY_THREAD_IDLE) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	MutexUnlock(&proxyRenderer->mutex);
}

void GBAVideoThreadProxyRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = (struct GBAVideoThreadProxyRenderer*) renderer;
	MutexLock(&proxyRenderer->mutex);
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	proxyRenderer->backend->finishFrame(proxyRenderer->backend);
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
	while (1) {
		ConditionWait(&proxyRenderer->toThreadCond, &proxyRenderer->mutex);
		if (proxyRenderer->threadState == PROXY_THREAD_STOPPED) {
			break;
		}
		proxyRenderer->threadState = PROXY_THREAD_BUSY;
		proxyRenderer->vramDirtyBitmap = 0;
		memset(proxyRenderer->oamDirtyBitmap, 0, sizeof(proxyRenderer->oamDirtyBitmap));
		memset(proxyRenderer->paletteDirtyBitmap, 0, sizeof(proxyRenderer->paletteDirtyBitmap));
		memset(proxyRenderer->regDirtyBitmap, 0, sizeof(proxyRenderer->regDirtyBitmap));
		size_t queueSize = GBAVideoDirtyQueueSize(&proxyRenderer->dirtyQueue);
		struct GBAVideoDirtyInfo* queue = malloc(queueSize * sizeof(struct GBAVideoDirtyInfo));
		memcpy(queue, GBAVideoDirtyQueueGetPointer(&proxyRenderer->dirtyQueue, 0), queueSize * sizeof(struct GBAVideoDirtyInfo));
		GBAVideoDirtyQueueClear(&proxyRenderer->dirtyQueue);
		size_t i;
		for (i = 0; i < queueSize; ++i) {
			switch (queue[i].type) {
			case DIRTY_REGISTER:
				proxyRenderer->backend->writeVideoRegister(proxyRenderer->backend, queue[i].address, proxyRenderer->regProxy[queue[i].address >> 1]);
				break;
			case DIRTY_VRAM:
				proxyRenderer->backend->writeVRAM(proxyRenderer->backend, queue[i].address);
				memcpy(&proxyRenderer->vramProxy[(queue[i].address & ~0xFFF) >> 1], &proxyRenderer->d.vram[(queue[i].address & ~0xFFF) >> 1], 0x1000);
				break;
			case DIRTY_PALETTE:
				proxyRenderer->backend->writePalette(proxyRenderer->backend, queue[i].address, proxyRenderer->paletteProxy[queue[i].address >> 1]);
				break;
			case DIRTY_OAM:
				proxyRenderer->backend->writeOAM(proxyRenderer->backend, queue[i].address);
				break;
			}
		}
		free(queue);
		ConditionWake(&proxyRenderer->fromThreadCond);
		ConditionWait(&proxyRenderer->toThreadCond, &proxyRenderer->mutex);
		MutexUnlock(&proxyRenderer->mutex);

		proxyRenderer->backend->drawScanline(proxyRenderer->backend, proxyRenderer->y);

		MutexLock(&proxyRenderer->mutex);
		proxyRenderer->threadState = PROXY_THREAD_IDLE;
		ConditionWake(&proxyRenderer->fromThreadCond);
	}
	MutexUnlock(&proxyRenderer->mutex);

	return 0;
}
