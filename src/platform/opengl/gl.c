/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gl.h"

#include <mgba-util/math.h>

static const GLint _glVertices[] = {
	0, 0,
	1, 0,
	1, 1,
	0, 1
};

static const GLint _glTexCoords[] = {
	0, 0,
	1, 0,
	1, 1,
	0, 1
};

static inline void _initTex(void) {
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
#ifndef _WIN32
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif
}

static void mGLContextInit(struct VideoBackend* v, WHandle handle) {
	UNUSED(handle);
	struct mGLContext* context = (struct mGLContext*) v;
	memset(context->layerDims, 0, sizeof(context->layerDims));
	memset(context->imageSizes, -1, sizeof(context->imageSizes));
	glGenTextures(2, context->tex);
	glBindTexture(GL_TEXTURE_2D, context->tex[0]);
	_initTex();
	glBindTexture(GL_TEXTURE_2D, context->tex[1]);
	_initTex();
	context->activeTex = 0;

	glGenTextures(VIDEO_LAYER_MAX, context->tex);
	int i;
	for (i = 0; i < VIDEO_LAYER_MAX; ++i) {
		glBindTexture(GL_TEXTURE_2D, context->layers[i]);
		_initTex();
	}
}

static inline void _setTexDims(int width, int height) {
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

static void mGLContextSetLayerDimensions(struct VideoBackend* v, enum VideoLayer layer, const struct mRectangle* dims) {
	struct mGLContext* context = (struct mGLContext*) v;
	if (layer >= VIDEO_LAYER_MAX) {
		return;
	}
	context->layerDims[layer].x = dims->x;
	context->layerDims[layer].y = dims->y;
	if (dims->width == context->layerDims[layer].width && dims->height == context->layerDims[layer].height) {
		return;
	}
	context->layerDims[layer].width = dims->width;
	context->layerDims[layer].height = dims->height;

	if (context->imageSizes[layer].width <= 0 || context->imageSizes[layer].height <= 0) {
		if (layer == VIDEO_LAYER_IMAGE) {
			glBindTexture(GL_TEXTURE_2D, context->tex[0]);
			_setTexDims(dims->width, dims->height);
			glBindTexture(GL_TEXTURE_2D, context->tex[1]);
			_setTexDims(dims->width, dims->height);
		} else {
			glBindTexture(GL_TEXTURE_2D, context->layers[layer]);
			_setTexDims(dims->width, dims->height);
		}
	}
}

static void mGLContextLayerDimensions(const struct VideoBackend* v, enum VideoLayer layer, struct mRectangle* dims) {
	struct mGLContext* context = (struct mGLContext*) v;
	if (layer >= VIDEO_LAYER_MAX) {
		return;
	}
	memcpy(dims, &context->layerDims[layer], sizeof(*dims));
}

static void mGLContextDeinit(struct VideoBackend* v) {
	struct mGLContext* context = (struct mGLContext*) v;
	glDeleteTextures(2, context->tex);
	glDeleteTextures(VIDEO_LAYER_MAX, context->layers);
}

static void mGLContextResized(struct VideoBackend* v, unsigned w, unsigned h) {
	unsigned drawW = w;
	unsigned drawH = h;

	unsigned maxW;
	unsigned maxH;
	VideoBackendGetFrameSize(v, &maxW, &maxH);

	if (v->lockAspectRatio) {
		lockAspectRatioUInt(maxW, maxH, &drawW, &drawH);
	}
	if (v->lockIntegerScaling) {
		lockIntegerRatioUInt(maxW, &drawW);
		lockIntegerRatioUInt(maxH, &drawH);
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

static void _setFilter(struct VideoBackend* v) {
	if (v->filter) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
}

static void _setFrame(struct mRectangle* dims, struct mRectangle* frame) {
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	glScissor(viewport[0] + (dims->x - frame->x) * viewport[2] / frame->width,
	          viewport[1] + (dims->y - frame->y) * viewport[3] / frame->height,
	          dims->width * viewport[2] / frame->width,
	          dims->height * viewport[3] / frame->height);
	glTranslatef(dims->x, dims->y, 0);
	glScalef(toPow2(dims->width), toPow2(dims->height), 1);
}

void mGLContextDrawFrame(struct VideoBackend* v) {
	struct mGLContext* context = (struct mGLContext*) v;
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_SCISSOR_TEST);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	struct mRectangle frame;
	VideoBackendGetFrame(v, &frame);
	glOrtho(frame.x, frame.x + frame.width, frame.y + frame.height, frame.y, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	int layer;
	for (layer = 0; layer < VIDEO_LAYER_IMAGE; ++layer) {
		if (context->layerDims[layer].width < 1 || context->layerDims[layer].height < 1) {
			continue;
		}

		glDisable(GL_BLEND);
		glBindTexture(GL_TEXTURE_2D, context->layers[layer]);
		_setFilter(v);
		glPushMatrix();
		_setFrame(&context->layerDims[layer], &frame);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glPopMatrix();
	}

	_setFrame(&context->layerDims[VIDEO_LAYER_IMAGE], &frame);
	if (v->interframeBlending) {
		glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
		glBlendColor(1, 1, 1, 0.5);
		glBindTexture(GL_TEXTURE_2D, context->tex[context->activeTex ^ 1]);
		_setFilter(v);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glEnable(GL_BLEND);
	}
	glBindTexture(GL_TEXTURE_2D, context->tex[context->activeTex]);
	_setFilter(v);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisable(GL_BLEND);
}

static void mGLContextSetImageSize(struct VideoBackend* v, enum VideoLayer layer, int width, int height) {
	struct mGLContext* context = (struct mGLContext*) v;
	if (layer >= VIDEO_LAYER_MAX) {
		return;
	}
	if (width <= 0 || height <= 0) {
		context->imageSizes[layer].width = -1;
		context->imageSizes[layer].height = -1;
		width = context->layerDims[layer].width;
		height = context->layerDims[layer].height;
	} else {
		context->imageSizes[layer].width = width;
		context->imageSizes[layer].height = height;
	}
	if (layer == VIDEO_LAYER_IMAGE) {
		glBindTexture(GL_TEXTURE_2D, context->tex[0]);
		_setTexDims(width, height);
		glBindTexture(GL_TEXTURE_2D, context->tex[1]);
		_setTexDims(width, height);
	} else {
		glBindTexture(GL_TEXTURE_2D, context->layers[layer]);
		_setTexDims(width, height);
	}
}

static void mGLContextImageSize(struct VideoBackend* v, enum VideoLayer layer, int* width, int* height) {
	struct mGLContext* context = (struct mGLContext*) v;
	if (layer >= VIDEO_LAYER_MAX) {
		return;
	}

	if (context->imageSizes[layer].width <= 0 || context->imageSizes[layer].height <= 0) {
		*width = context->layerDims[layer].width;
		*height = context->layerDims[layer].height;
	} else {
		*width = context->imageSizes[layer].width;
		*height = context->imageSizes[layer].height;		
	}
}

void mGLContextPostFrame(struct VideoBackend* v, enum VideoLayer layer, const void* frame) {
	struct mGLContext* context = (struct mGLContext*) v;
	if (layer >= VIDEO_LAYER_MAX) {
		return;
	}
	if (layer == VIDEO_LAYER_IMAGE) {
		context->activeTex ^= 1;
		glBindTexture(GL_TEXTURE_2D, context->tex[context->activeTex]);
	} else {
		glBindTexture(GL_TEXTURE_2D, context->layers[layer]);		
	}

	int width = context->imageSizes[layer].width;
	int height = context->imageSizes[layer].height;

	if (width <= 0 || height <= 0) {
		width = context->layerDims[layer].width;
		height = context->layerDims[layer].height;
	}
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
#else
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);
#endif
#elif defined(__BIG_ENDIAN__)
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, frame);
#else
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, frame);
#endif
}

void mGLContextCreate(struct mGLContext* context) {
	context->d.init = mGLContextInit;
	context->d.deinit = mGLContextDeinit;
	context->d.setLayerDimensions = mGLContextSetLayerDimensions;
	context->d.layerDimensions = mGLContextLayerDimensions;
	context->d.contextResized = mGLContextResized;
	context->d.swap = NULL;
	context->d.clear = mGLContextClear;
	context->d.setImageSize = mGLContextSetImageSize;
	context->d.imageSize = mGLContextImageSize;
	context->d.setImage = mGLContextPostFrame;
	context->d.drawFrame = mGLContextDrawFrame;
}
