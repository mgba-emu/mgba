/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gl.h"

#include <mgba-util/math.h>

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

static void mGLContextInit(struct VideoBackend* v, WHandle handle) {
	UNUSED(handle);
	struct mGLContext* context = (struct mGLContext*) v;
	v->width = 1;
	v->height = 1;
	glGenTextures(2, context->tex);
	glBindTexture(GL_TEXTURE_2D, context->tex[0]);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#ifndef _WIN32
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif
	glBindTexture(GL_TEXTURE_2D, context->tex[1]);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#ifndef _WIN32
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif
	context->activeTex = 0;
}

static void mGLContextSetDimensions(struct VideoBackend* v, unsigned width, unsigned height) {
	struct mGLContext* context = (struct mGLContext*) v;
	if (width == v->width && height == v->height) {
		return;
	}
	v->width = width;
	v->height = height;

	glBindTexture(GL_TEXTURE_2D, context->tex[0]);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toPow2(width), toPow2(height), 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toPow2(width), toPow2(height), 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 0);
#endif
#elif defined(__BIG_ENDIAN__)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toPow2(width), toPow2(height), 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toPow2(width), toPow2(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif

	glBindTexture(GL_TEXTURE_2D, context->tex[1]);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toPow2(width), toPow2(height), 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toPow2(width), toPow2(height), 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 0);
#endif
#elif defined(__BIG_ENDIAN__)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toPow2(width), toPow2(height), 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, toPow2(width), toPow2(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif
}

static void mGLContextDeinit(struct VideoBackend* v) {
	struct mGLContext* context = (struct mGLContext*) v;
	glDeleteTextures(2, context->tex);
}

static void mGLContextResized(struct VideoBackend* v, unsigned w, unsigned h) {
	unsigned drawW = w;
	unsigned drawH = h;
	if (v->lockAspectRatio) {
		if (w * v->height > h * v->width) {
			drawW = h * v->width / v->height;
		} else if (w * v->height < h * v->width) {
			drawH = w * v->height / v->width;
		}
	}
	if (v->lockIntegerScaling) {
		if (drawW >= v->width) {
			drawW -= drawW % v->width;
		}
		if (drawH >= v->height) {
			drawH -= drawH % v->height;
		}
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport((w - drawW) / 2, (h - drawH) / 2, drawW, drawH);
}

static void mGLContextClear(struct VideoBackend* v) {
	UNUSED(v);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
}

void mGLContextDrawFrame(struct VideoBackend* v) {
	struct mGLContext* context = (struct mGLContext*) v;
	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, v->width, v->height, 0, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if (v->interframeBlending) {
		glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
		glBlendColor(1, 1, 1, 0.5);
		glBindTexture(GL_TEXTURE_2D, context->tex[context->activeTex ^ 1]);
		if (v->filter) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		} else {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glEnable(GL_BLEND);
	}
	glBindTexture(GL_TEXTURE_2D, context->tex[context->activeTex]);
	if (v->filter) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisable(GL_BLEND);
}

void mGLContextPostFrame(struct VideoBackend* v, const void* frame) {
	struct mGLContext* context = (struct mGLContext*) v;
	context->activeTex ^= 1;
	glBindTexture(GL_TEXTURE_2D, context->tex[context->activeTex]);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, v->width, v->height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
#else
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, v->width, v->height, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);
#endif
#elif defined(__BIG_ENDIAN__)
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, v->width, v->height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, frame);
#else
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, v->width, v->height,  GL_RGBA, GL_UNSIGNED_BYTE, frame);
#endif
}

void mGLContextCreate(struct mGLContext* context) {
	context->d.init = mGLContextInit;
	context->d.deinit = mGLContextDeinit;
	context->d.setDimensions = mGLContextSetDimensions;
	context->d.resized = mGLContextResized;
	context->d.swap = 0;
	context->d.clear = mGLContextClear;
	context->d.postFrame = mGLContextPostFrame;
	context->d.drawFrame = mGLContextDrawFrame;
	context->d.setMessage = 0;
	context->d.clearMessage = 0;
}
