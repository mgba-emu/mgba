/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PROXY_BACKEND_H
#define PROXY_BACKEND_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/ring-fifo.h>
#include <mgba-util/threading.h>
#include <mgba/feature/video-backend.h>

enum mVideoBackendCommandType {
	mVB_CMD_DUMMY = 0,
	mVB_CMD_INIT,
	mVB_CMD_DEINIT,
	mVB_CMD_SET_LAYER_DIMENSIONS,
	mVB_CMD_LAYER_DIMENSIONS,
	mVB_CMD_SWAP,
	mVB_CMD_CLEAR,
	mVB_CMD_CONTEXT_RESIZED,
	mVB_CMD_SET_IMAGE_SIZE,
	mVB_CMD_IMAGE_SIZE,
	mVB_CMD_SET_IMAGE,
	mVB_CMD_DRAW_FRAME,
};

union mVideoBackendCommandData {
	struct mRectangle dims;
	struct {
		int width;
		int height;
	} s;
	struct {
		unsigned width;
		unsigned height;
	} u;
	const void* image;
};

struct mVideoBackendCommand {
	enum mVideoBackendCommandType cmd;

	union {
		WHandle handle;
		enum VideoLayer layer;
	};
	union mVideoBackendCommandData data;
};

struct mVideoProxyBackend {
	struct VideoBackend d;
	struct VideoBackend* backend;
	
	struct RingFIFO in;
	struct RingFIFO out;

	Mutex inLock;
	Mutex outLock;
	Condition inWait;
	Condition outWait;

	void (*wakeupCb)(struct mVideoProxyBackend*, void* context);
	void* context;
};

void mVideoProxyBackendInit(struct mVideoProxyBackend* proxy, struct VideoBackend* backend);
void mVideoProxyBackendDeinit(struct mVideoProxyBackend* proxy);

void mVideoProxyBackendSubmit(struct mVideoProxyBackend* proxy, const struct mVideoBackendCommand* cmd, union mVideoBackendCommandData* out);
bool mVideoProxyBackendRun(struct mVideoProxyBackend* proxy, bool block);

bool mVideoProxyBackendCommandIsBlocking(enum mVideoBackendCommandType);

CXX_GUARD_END

#endif
