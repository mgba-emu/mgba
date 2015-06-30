/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gl.h"

#include "gba/video.h"

static const GLint _glVertices[] = {
	0, 0,
	256, 0,
	256, 256,
	0, 256
};

static const GLint _glTexCoords[] = {
	0, 0,
	1, 0,
	1, 1,
	0, 1
};

static void GBAGLContextInit(struct VideoBackend* v, WHandle handle) {
	UNUSED(handle);
	struct GBAGLContext* context = (struct GBAGLContext*) v;
	glGenTextures(1, &context->tex);
	glBindTexture(GL_TEXTURE_2D, context->tex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#ifndef _WIN32
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif

#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 0);
#endif
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif
}

static void GBAGLContextDeinit(struct VideoBackend* v) {
	struct GBAGLContext* context = (struct GBAGLContext*) v;
	glDeleteTextures(1, &context->tex);
}

static void GBAGLContextResized(struct VideoBackend* v, int w, int h) {
	int drawW = w;
	int drawH = h;
	if (v->lockAspectRatio) {
		if (w * 2 > h * 3) {
			drawW = h * 3 / 2;
		} else if (w * 2 < h * 3) {
			drawH = w * 2 / 3;
		}
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport((w - drawW) / 2, (h - drawH) / 2, drawW, drawH);
}

static void GBAGLContextClear(struct VideoBackend* v) {
	UNUSED(v);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
}

void GBAGLContextDrawFrame(struct VideoBackend* v) {
	struct GBAGLContext* context = (struct GBAGLContext*) v;
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glBindTexture(GL_TEXTURE_2D, context->tex);
	if (v->filter) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void GBAGLContextPostFrame(struct VideoBackend* v, const void* frame) {
	struct GBAGLContext* context = (struct GBAGLContext*) v;
	glBindTexture(GL_TEXTURE_2D, context->tex);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);
#endif
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame);
#endif
}

void GBAGLContextCreate(struct GBAGLContext* context) {
	context->d.init = GBAGLContextInit;
	context->d.deinit = GBAGLContextDeinit;
	context->d.resized = GBAGLContextResized;
	context->d.swap = 0;
	context->d.clear = GBAGLContextClear;
	context->d.postFrame = GBAGLContextPostFrame;
	context->d.drawFrame = GBAGLContextDrawFrame;
	context->d.setMessage = 0;
	context->d.clearMessage = 0;
}
