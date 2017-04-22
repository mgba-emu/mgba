/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_BACKEND_H
#define VIDEO_BACKEND_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef _WIN32
#include <windows.h>
typedef HWND WHandle;
#else
typedef void* WHandle;
#endif

struct VideoBackend {
	void (*init)(struct VideoBackend*, WHandle handle);
	void (*deinit)(struct VideoBackend*);
	void (*setDimensions)(struct VideoBackend*, unsigned width, unsigned height);
	void (*swap)(struct VideoBackend*);
	void (*clear)(struct VideoBackend*);
	void (*resized)(struct VideoBackend*, unsigned w, unsigned h);
	void (*postFrame)(struct VideoBackend*, const void* frame);
	void (*drawFrame)(struct VideoBackend*);
	void (*setMessage)(struct VideoBackend*, const char* message);
	void (*clearMessage)(struct VideoBackend*);

	void* user;
	unsigned width;
	unsigned height;

	bool filter;
	bool lockAspectRatio;
	bool lockIntegerScaling;
};

struct VideoShader {
	const char* name;
	const char* author;
	const char* description;
	void* preprocessShader;
	void* passes;
	size_t nPasses;
};

CXX_GUARD_END

#endif
