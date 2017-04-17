/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/renderers/thread-proxy.h>

#include <mgba/core/tile-cache.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#ifndef DISABLE_THREADING

static void GBAVideoThreadProxyRendererInit(struct mVideoLogger* logger);
static void GBAVideoThreadProxyRendererReset(struct mVideoLogger* logger);
static void GBAVideoThreadProxyRendererDeinit(struct mVideoLogger* logger);

static THREAD_ENTRY _proxyThread(void* renderer);

static bool _writeData(struct mVideoLogger* logger, const void* data, size_t length);
static bool _readData(struct mVideoLogger* logger, void* data, size_t length, bool block);

static void _lock(struct mVideoLogger* logger);
static void _unlock(struct mVideoLogger* logger);
static void _wait(struct mVideoLogger* logger);
static void _wake(struct mVideoLogger* logger, int y);

void GBAVideoThreadProxyRendererCreate(struct GBAVideoThreadProxyRenderer* renderer, struct GBAVideoRenderer* backend) {
	renderer->d.logger.block = true;
	GBAVideoProxyRendererCreate(&renderer->d, backend, false);

	renderer->d.logger.init = GBAVideoThreadProxyRendererInit;
	renderer->d.logger.reset = GBAVideoThreadProxyRendererReset;
	renderer->d.logger.deinit = GBAVideoThreadProxyRendererDeinit;
	renderer->d.logger.lock = _lock;
	renderer->d.logger.unlock = _unlock;
	renderer->d.logger.wait = _wait;
	renderer->d.logger.wake = _wake;

	renderer->d.logger.writeData = _writeData;
	renderer->d.logger.readData = _readData;
	renderer->d.logger.vf = NULL;
}

void GBAVideoThreadProxyRendererInit(struct mVideoLogger* logger) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
	ConditionInit(&proxyRenderer->fromThreadCond);
	ConditionInit(&proxyRenderer->toThreadCond);
	MutexInit(&proxyRenderer->mutex);
	RingFIFOInit(&proxyRenderer->dirtyQueue, 0x40000);

	proxyRenderer->threadState = PROXY_THREAD_IDLE;
	ThreadCreate(&proxyRenderer->thread, _proxyThread, proxyRenderer);
}

void GBAVideoThreadProxyRendererReset(struct mVideoLogger* logger) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
	MutexLock(&proxyRenderer->mutex);
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
	MutexUnlock(&proxyRenderer->mutex);
}

void GBAVideoThreadProxyRendererDeinit(struct mVideoLogger* logger) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
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

static bool _writeData(struct mVideoLogger* logger, const void* data, size_t length) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
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

static bool _readData(struct mVideoLogger* logger, void* data, size_t length, bool block) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
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

static void _lock(struct mVideoLogger* logger) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
	MutexLock(&proxyRenderer->mutex);
}

static void _wait(struct mVideoLogger* logger) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
	if (proxyRenderer->threadState == PROXY_THREAD_STOPPED) {
		mLOG(GBA_VIDEO, ERROR, "Proxy thread stopped prematurely!");
		_proxyThreadRecover(proxyRenderer);
		return;
	}
	while (proxyRenderer->threadState == PROXY_THREAD_BUSY) {
		ConditionWake(&proxyRenderer->toThreadCond);
		ConditionWait(&proxyRenderer->fromThreadCond, &proxyRenderer->mutex);
	}
}

static void _unlock(struct mVideoLogger* logger) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
	MutexUnlock(&proxyRenderer->mutex);
}

static void _wake(struct mVideoLogger* logger, int y) {
	struct GBAVideoThreadProxyRenderer* proxyRenderer = logger->context;
	if ((y & 15) == 15) {
		ConditionWake(&proxyRenderer->toThreadCond);
	}
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
		if (!mVideoLoggerRendererRun(&proxyRenderer->d.logger, false)) {
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
