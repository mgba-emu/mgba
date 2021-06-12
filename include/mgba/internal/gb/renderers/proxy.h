/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_VIDEO_PROXY_H
#define GB_VIDEO_PROXY_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gb/video.h>
#include <mgba/feature/video-logger.h>

struct GBVideoProxyRenderer {
	struct GBVideoRenderer d;
	struct GBVideoRenderer* backend;
	struct mVideoLogger* logger;
	enum GBModel model;
};

void GBVideoProxyRendererCreate(struct GBVideoProxyRenderer* renderer, struct GBVideoRenderer* backend);
void GBVideoProxyRendererShim(struct GBVideo* video, struct GBVideoProxyRenderer* renderer);
void GBVideoProxyRendererUnshim(struct GBVideo* video, struct GBVideoProxyRenderer* renderer);

CXX_GUARD_END

#endif
