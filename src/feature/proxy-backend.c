/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/feature/proxy-backend.h>

static void _mVideoProxyBackendInit(struct VideoBackend* v, WHandle handle) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_INIT,
		.handle = handle
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static void _mVideoProxyBackendDeinit(struct VideoBackend* v) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_DEINIT,
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static void _mVideoProxyBackendSetLayerDimensions(struct VideoBackend* v, enum VideoLayer layer, const struct mRectangle* dims) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_SET_LAYER_DIMENSIONS,
		.layer = layer,
		.data = {
			.dims = *dims
		}
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static void _mVideoProxyBackendLayerDimensions(const struct VideoBackend* v, enum VideoLayer layer, struct mRectangle* dims) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_LAYER_DIMENSIONS,
		.layer = layer,
	};
	union mVideoBackendCommandData out;
	mVideoProxyBackendSubmit(proxy, &cmd, &out);
	memcpy(dims, &out.dims, sizeof(*dims));
}

static void _mVideoProxyBackendSwap(struct VideoBackend* v) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_SWAP,
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static void _mVideoProxyBackendClear(struct VideoBackend* v) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_CLEAR,
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static void _mVideoProxyBackendContextResized(struct VideoBackend* v, unsigned w, unsigned h) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_CONTEXT_RESIZED,
		.data = {
			.u = {
				.width = w,
				.height = h,
			}
		}
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static void _mVideoProxyBackendSetImageSize(struct VideoBackend* v, enum VideoLayer layer, int w, int h) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_SET_IMAGE_SIZE,
		.layer = layer,
		.data = {
			.s = {
				.width = w,
				.height = h,
			}
		}
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static void _mVideoProxyBackendImageSize(struct VideoBackend* v, enum VideoLayer layer, int* w, int* h) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_IMAGE_SIZE,
		.layer = layer,
	};
	union mVideoBackendCommandData out;
	mVideoProxyBackendSubmit(proxy, &cmd, &out);
	*w = out.s.width;
	*h = out.s.height;
}

static void _mVideoProxyBackendSetImage(struct VideoBackend* v, enum VideoLayer layer, const void* frame) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_SET_IMAGE,
		.layer = layer,
		.data = {
			.image = frame
		}
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static void _mVideoProxyBackendDrawFrame(struct VideoBackend* v) {
	struct mVideoProxyBackend* proxy = (struct mVideoProxyBackend*) v;
	struct mVideoBackendCommand cmd = {
		.cmd = mVB_CMD_DRAW_FRAME,
	};
	mVideoProxyBackendSubmit(proxy, &cmd, NULL);
}

static bool mVideoProxyBackendReadIn(struct mVideoProxyBackend* proxy, struct mVideoBackendCommand* cmd, bool block);
static void mVideoProxyBackendWriteOut(struct mVideoProxyBackend* proxy, const union mVideoBackendCommandData* out);

void mVideoProxyBackendInit(struct mVideoProxyBackend* proxy, struct VideoBackend* backend) {
	proxy->d.init = _mVideoProxyBackendInit;
	proxy->d.deinit = _mVideoProxyBackendDeinit;
	proxy->d.setLayerDimensions = _mVideoProxyBackendSetLayerDimensions;
	proxy->d.layerDimensions = _mVideoProxyBackendLayerDimensions;
	proxy->d.swap = _mVideoProxyBackendSwap;
	proxy->d.clear = _mVideoProxyBackendClear;
	proxy->d.contextResized = _mVideoProxyBackendContextResized;
	proxy->d.setImageSize = _mVideoProxyBackendSetImageSize;
	proxy->d.imageSize = _mVideoProxyBackendImageSize;
	proxy->d.setImage = _mVideoProxyBackendSetImage;
	proxy->d.drawFrame = _mVideoProxyBackendDrawFrame;
	proxy->backend = backend;

	RingFIFOInit(&proxy->in, 0x400);
	RingFIFOInit(&proxy->out, 0x400);
	MutexInit(&proxy->inLock);
	MutexInit(&proxy->outLock);
	ConditionInit(&proxy->inWait);
	ConditionInit(&proxy->outWait);

	proxy->wakeupCb = NULL;
	proxy->context = NULL;
}

void mVideoProxyBackendDeinit(struct mVideoProxyBackend* proxy) {
	ConditionDeinit(&proxy->inWait);
	ConditionDeinit(&proxy->outWait);
	MutexDeinit(&proxy->inLock);
	MutexDeinit(&proxy->outLock);
	RingFIFODeinit(&proxy->in);
	RingFIFODeinit(&proxy->out);
}

void mVideoProxyBackendSubmit(struct mVideoProxyBackend* proxy, const struct mVideoBackendCommand* cmd, union mVideoBackendCommandData* out) {
	MutexLock(&proxy->inLock);
	while (!RingFIFOWrite(&proxy->in, cmd, sizeof(*cmd))) {
		mLOG(VIDEO, DEBUG, "Can't write command. Proxy thread asleep?");
		ConditionWait(&proxy->inWait, &proxy->inLock);
	}
	MutexUnlock(&proxy->inLock);
	if (proxy->wakeupCb) {
		proxy->wakeupCb(proxy, proxy->context);
	}

	if (!mVideoProxyBackendCommandIsBlocking(cmd->cmd)) {
		return;
	}

	MutexLock(&proxy->outLock);
	while (!RingFIFORead(&proxy->out, out, sizeof(*out))) {
		ConditionWait(&proxy->outWait, &proxy->outLock);
	}
	MutexUnlock(&proxy->outLock);
}

bool mVideoProxyBackendRun(struct mVideoProxyBackend* proxy, bool block) {
	bool ok = false;
	do {
		struct mVideoBackendCommand cmd;
		union mVideoBackendCommandData out;
		if (mVideoProxyBackendReadIn(proxy, &cmd, block)) {
			switch (cmd.cmd) {
			case mVB_CMD_DUMMY:
				break;
			case mVB_CMD_INIT:
				proxy->backend->init(proxy->backend, cmd.handle);
				break;
			case mVB_CMD_DEINIT:
				proxy->backend->deinit(proxy->backend);
				break;
			case mVB_CMD_SET_LAYER_DIMENSIONS:
				proxy->backend->setLayerDimensions(proxy->backend, cmd.layer, &cmd.data.dims);
				break;
			case mVB_CMD_LAYER_DIMENSIONS:
				proxy->backend->layerDimensions(proxy->backend, cmd.layer, &out.dims);
				break;
			case mVB_CMD_SWAP:
				proxy->backend->swap(proxy->backend);
				break;
			case mVB_CMD_CLEAR:
				proxy->backend->clear(proxy->backend);
				break;
			case mVB_CMD_CONTEXT_RESIZED:
				proxy->backend->contextResized(proxy->backend, cmd.data.u.width, cmd.data.u.height);
				break;
			case mVB_CMD_SET_IMAGE_SIZE:
				proxy->backend->setImageSize(proxy->backend, cmd.layer, cmd.data.s.width, cmd.data.s.height);
				break;
			case mVB_CMD_IMAGE_SIZE:
				proxy->backend->imageSize(proxy->backend, cmd.layer, &out.s.width, &out.s.height);
				break;
			case mVB_CMD_SET_IMAGE:
				proxy->backend->setImage(proxy->backend, cmd.layer, cmd.data.image);
				break;
			case mVB_CMD_DRAW_FRAME:
				proxy->backend->drawFrame(proxy->backend);
				break;
			}
			if (mVideoProxyBackendCommandIsBlocking(cmd.cmd)) {
				mVideoProxyBackendWriteOut(proxy, &out);
			}
			ok = true;
		}
	} while (block);
	return ok;
}

bool mVideoProxyBackendReadIn(struct mVideoProxyBackend* proxy, struct mVideoBackendCommand* cmd, bool block) {
	bool gotCmd = false;
	MutexLock(&proxy->inLock);
	do {
		gotCmd = RingFIFORead(&proxy->in, cmd, sizeof(*cmd));
		ConditionWake(&proxy->inWait);
		// TODO: interlock?
		if (block && !gotCmd) {
			mLOG(VIDEO, DEBUG, "Can't read command. Runner thread asleep?");
		}
	} while (block && !gotCmd);
	MutexUnlock(&proxy->inLock);
	return gotCmd;
}

void mVideoProxyBackendWriteOut(struct mVideoProxyBackend* proxy, const union mVideoBackendCommandData* out) {
	bool gotReply = false;
	MutexLock(&proxy->outLock);
	while (!gotReply) {
		gotReply = RingFIFOWrite(&proxy->out, out, sizeof(*out));
		ConditionWake(&proxy->outWait);
		// TOOD: interlock?
		if (!gotReply) {
			mLOG(VIDEO, DEBUG, "Can't write reply. Runner thread asleep?");
		}
	}
	MutexUnlock(&proxy->outLock);
}

bool mVideoProxyBackendCommandIsBlocking(enum mVideoBackendCommandType cmd) {
	switch (cmd) {
	case mVB_CMD_DUMMY:
	case mVB_CMD_CONTEXT_RESIZED:
	case mVB_CMD_SET_LAYER_DIMENSIONS:
	case mVB_CMD_CLEAR:
	case mVB_CMD_SET_IMAGE_SIZE:
	case mVB_CMD_DRAW_FRAME:
		return false;
	case mVB_CMD_INIT:
	case mVB_CMD_DEINIT:
	case mVB_CMD_LAYER_DIMENSIONS:
	case mVB_CMD_SWAP:
	case mVB_CMD_IMAGE_SIZE:
	case mVB_CMD_SET_IMAGE:
		return true;
	}

	return true;
}
