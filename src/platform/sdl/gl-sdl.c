/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gl-common.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba-util/math.h>

#include "platform/opengl/gl.h"

static bool mSDLGLInit(struct mSDLRenderer* renderer);
static void mSDLGLRunloop(struct mSDLRenderer* renderer, void* user);
static void mSDLGLDeinit(struct mSDLRenderer* renderer);

void mSDLGLCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLGLInit;
	renderer->deinit = mSDLGLDeinit;
	renderer->runloop = mSDLGLRunloop;
}

bool mSDLGLInit(struct mSDLRenderer* renderer) {
	mSDLGLCommonInit(renderer);

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
	renderer->gl.d.setDimensions(&renderer->gl.d, renderer->width, renderer->height);

	mSDLGLDoViewport(renderer->viewportWidth, renderer->viewportHeight, &renderer->gl.d);
	return true;
}

void mSDLGLRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;
	struct VideoBackend* v = &renderer->gl.d;

	while (mCoreThreadIsActive(context)) {
		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
			// Event handling can change the size of the screen
			if (renderer->player.windowUpdated) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
				SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
#else
				renderer->viewportWidth = renderer->player.newWidth;
				renderer->viewportHeight = renderer->player.newHeight;
				mSDLGLCommonInit(renderer);
#endif
				mSDLGLDoViewport(renderer->viewportWidth, renderer->viewportHeight, v);
				renderer->player.windowUpdated = 0;
			}
		}
		renderer->core->desiredVideoDimensions(renderer->core, &renderer->width, &renderer->height);
		if (renderer->width != v->width || renderer->height != v->height) {
			renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, renderer->width);
			v->setDimensions(v, renderer->width, renderer->height);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			v->postFrame(v, renderer->outputBuffer);
		}
		mCoreSyncWaitFrameEnd(&context->impl->sync);
		v->drawFrame(v);
		v->swap(v);
	}
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
