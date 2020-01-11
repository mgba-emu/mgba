/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_THREAD_PROXY_H
#define VIDEO_THREAD_PROXY_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gba/video.h>
#include "video-logger.h"
#include <mgba-util/threading.h>
#include <mgba-util/ring-fifo.h>

enum mVideoThreadProxyState {
	PROXY_THREAD_STOPPED = 0,
	PROXY_THREAD_IDLE,
	PROXY_THREAD_BUSY
};

struct mVideoThreadProxy {
	struct mVideoLogger d;

	Thread thread;
	Condition fromThreadCond;
	Condition toThreadCond;
	Mutex mutex;
	enum mVideoThreadProxyState threadState;
	enum mVideoLoggerEvent event;

	struct RingFIFO dirtyQueue;
};

void mVideoThreadProxyCreate(struct mVideoThreadProxy* renderer);

CXX_GUARD_END

#endif
