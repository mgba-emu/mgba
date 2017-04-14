/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_THREAD_PROXY_H
#define VIDEO_THREAD_PROXY_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/video-proxy.h>
#include <mgba/internal/gba/video.h>
#include <mgba-util/threading.h>
#include <mgba-util/ring-fifo.h>

enum GBAVideoThreadProxyState {
	PROXY_THREAD_STOPPED = 0,
	PROXY_THREAD_IDLE,
	PROXY_THREAD_BUSY
};

struct GBAVideoThreadProxyRenderer {
	struct GBAVideoRenderer d;
	struct GBAVideoRenderer* backend;
	struct mVideoProxy proxy;

	Thread thread;
	Condition fromThreadCond;
	Condition toThreadCond;
	Mutex mutex;
	enum GBAVideoThreadProxyState threadState;

	struct RingFIFO dirtyQueue;
};

void GBAVideoThreadProxyRendererCreate(struct GBAVideoThreadProxyRenderer* renderer, struct GBAVideoRenderer* backend);

CXX_GUARD_END

#endif
