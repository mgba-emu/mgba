/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GL_H
#define GL_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#ifdef _MSC_VER
#include <windows.h>
#endif
#include <GL/gl.h>
#endif

#include <mgba/feature/video-backend.h>

struct mGLContext {
	struct VideoBackend d;

	int activeTex;
	GLuint tex[2];
	GLuint layers[VIDEO_LAYER_MAX];
	struct mRectangle layerDims[VIDEO_LAYER_MAX];
	struct mSize imageSizes[VIDEO_LAYER_MAX];
};

void mGLContextCreate(struct mGLContext*);

CXX_GUARD_END

#endif
