/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_VIDEO_PROXY_H
#define GBA_VIDEO_PROXY_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gba/video.h>
#include "feature/video-logger.h"

struct GBAVideoProxyRenderer {
	struct GBAVideoRenderer d;
	struct GBAVideoRenderer* backend;
	struct mVideoLogger logger;

	bool block;

	void (*init)(struct GBAVideoProxyRenderer*);
	void (*deinit)(struct GBAVideoProxyRenderer*);
	void (*reset)(struct GBAVideoProxyRenderer*);

	void (*lock)(struct GBAVideoProxyRenderer*);
	void (*unlock)(struct GBAVideoProxyRenderer*);
	void (*wait)(struct GBAVideoProxyRenderer*);
	void (*wake)(struct GBAVideoProxyRenderer*, int y);
};

void GBAVideoProxyRendererCreate(struct GBAVideoProxyRenderer* renderer, struct GBAVideoRenderer* backend, bool readonly);
void GBAVideoProxyRendererShim(struct GBAVideo* video, struct GBAVideoProxyRenderer* renderer);
void GBAVideoProxyRendererUnshim(struct GBAVideo* video, struct GBAVideoProxyRenderer* renderer);

CXX_GUARD_END

#endif
