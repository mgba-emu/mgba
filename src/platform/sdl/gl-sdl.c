/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gl-common.h"

#include <mgba/core/core.h>

#include "platform/opengl/gl.h"

static bool mSDLGLInit(struct mSDLRenderer* renderer);
static void mSDLGLDeinit(struct mSDLRenderer* renderer);

void mSDLGLCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLGLInit;
	renderer->deinit = mSDLGLDeinit;
	renderer->runloop = mSDLGLCommonRunloop;
	renderer->backend = &renderer->gl.d;
}

bool mSDLGLInit(struct mSDLRenderer* renderer) {
	size_t size = renderer->width * renderer->height * BYTES_PER_PIXEL;
	renderer->outputBuffer = malloc(size);
	memset(renderer->outputBuffer, 0, size);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, renderer->width);

	mGLContextCreate(&renderer->gl);
	renderer->gl.d.user = renderer;
	renderer->gl.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl.d.lockIntegerScaling = renderer->lockIntegerScaling;
	renderer->gl.d.interframeBlending = renderer->interframeBlending;
	renderer->gl.d.filter = renderer->filter;
	renderer->gl.d.swap = mSDLGLCommonSwap;
	renderer->gl.d.init(&renderer->gl.d, 0);
	struct mRectangle dims = {
		.x = 0,
		.y = 0,
		.width = renderer->width,
		.height = renderer->height
	};
	renderer->gl.d.setLayerDimensions(&renderer->gl.d, VIDEO_LAYER_IMAGE, &dims);

	mSDLGLDoViewport(renderer->viewportWidth, renderer->viewportHeight, &renderer->gl.d);
	return true;
}

void mSDLGLDeinit(struct mSDLRenderer* renderer) {
	if (renderer->gl.d.deinit) {
		renderer->gl.d.deinit(&renderer->gl.d);
	}
	free(renderer->outputBuffer);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_DeleteContext(renderer->glCtx);
#endif
}
