/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_BACKEND_H
#define VIDEO_BACKEND_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/geometry.h>
#include <mgba/core/log.h>

#ifdef _WIN32
#include <windows.h>
typedef HWND WHandle;
#else
typedef void* WHandle;
#endif

mLOG_DECLARE_CATEGORY(VIDEO);

enum VideoLayer {
	VIDEO_LAYER_BACKGROUND = 0,
	VIDEO_LAYER_BEZEL,
	VIDEO_LAYER_IMAGE,
	VIDEO_LAYER_OVERLAY0,
	VIDEO_LAYER_OVERLAY1,
	VIDEO_LAYER_OVERLAY2,
	VIDEO_LAYER_OVERLAY3,
	VIDEO_LAYER_OVERLAY4,
	VIDEO_LAYER_OVERLAY5,
	VIDEO_LAYER_OVERLAY6,
	VIDEO_LAYER_OVERLAY7,
	VIDEO_LAYER_OVERLAY8,
	VIDEO_LAYER_OVERLAY9,
	VIDEO_LAYER_MAX
};

#define VIDEO_LAYER_OVERLAY_COUNT VIDEO_LAYER_MAX - VIDEO_LAYER_OVERLAY0

struct VideoBackend {
	void (*init)(struct VideoBackend*, WHandle handle);
	void (*deinit)(struct VideoBackend*);
	void (*setLayerDimensions)(struct VideoBackend*, enum VideoLayer, const struct mRectangle*);
	void (*layerDimensions)(const struct VideoBackend*, enum VideoLayer, struct mRectangle*);
	void (*swap)(struct VideoBackend*);
	void (*clear)(struct VideoBackend*);
	void (*contextResized)(struct VideoBackend*, unsigned w, unsigned h);
	void (*setImageSize)(struct VideoBackend*, enum VideoLayer, int w, int h);
	void (*imageSize)(struct VideoBackend*, enum VideoLayer, int* w, int* h);
	void (*setImage)(struct VideoBackend*, enum VideoLayer, const void* frame);
	void (*drawFrame)(struct VideoBackend*);

	void* user;

	bool filter;
	bool lockAspectRatio;
	bool lockIntegerScaling;
	bool interframeBlending;
	enum VideoLayer cropToLayer;
};

struct VideoShader {
	const char* name;
	const char* author;
	const char* description;
	void* preprocessShader;
	void* passes;
	size_t nPasses;
};

void VideoBackendGetFrame(const struct VideoBackend*, struct mRectangle* frame);
void VideoBackendGetFrameSize(const struct VideoBackend*, unsigned* width, unsigned* height);
void VideoBackendRecenter(struct VideoBackend* v, unsigned scale);

CXX_GUARD_END

#endif
