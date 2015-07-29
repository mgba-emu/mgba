/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_THREAD_PROXY_H
#define VIDEO_THREAD_PROXY_H

#include "gba/video.h"
#include "util/threading.h"
#include "util/vector.h"

enum GBAVideoDirtyType {
	DIRTY_REGISTER,
	DIRTY_VRAM,
	DIRTY_OAM,
	DIRTY_PALETTE
};

enum GBAVideoThreadProxyState {
	PROXY_THREAD_STOPPED = 0,
	PROXY_THREAD_IDLE,
	PROXY_THREAD_BUSY
};

struct GBAVideoDirtyInfo {
	enum GBAVideoDirtyType type;
	uint32_t address;
};

DECLARE_VECTOR(GBAVideoDirtyQueue, struct GBAVideoDirtyInfo);

struct GBAVideoThreadProxyRenderer {
	struct GBAVideoRenderer d;
	struct GBAVideoRenderer* backend;

	Thread thread;
	Condition fromThreadCond;
	Condition toThreadCond;
	Mutex mutex;
	enum GBAVideoThreadProxyState threadState;

	struct GBAVideoDirtyQueue dirtyQueue;
	uint32_t vramDirtyBitmap;
	uint32_t oamDirtyBitmap[16];
	uint32_t paletteDirtyBitmap[16];
	uint32_t regDirtyBitmap[2];

	uint16_t* vramProxy;
	union GBAOAM oamProxy;
	uint16_t paletteProxy[512];
	uint16_t regProxy[42];

	int y;
};

void GBAVideoThreadProxyRendererCreate(struct GBAVideoThreadProxyRenderer* renderer, struct GBAVideoRenderer* backend);

#endif
