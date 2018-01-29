/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gl-common.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
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

	renderer->outputBuffer = malloc(renderer->width * renderer->height * BYTES_PER_PIXEL);
	memset(renderer->outputBuffer, 0, renderer->width * renderer->height * BYTES_PER_PIXEL);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, renderer->width);

	mGLContextCreate(&renderer->gl);
	renderer->gl.d.user = renderer;
	renderer->gl.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl.d.lockIntegerScaling = renderer->lockIntegerScaling;
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

	while (context->state < THREAD_EXITING) {
		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
#if SDL_VERSION_ATLEAST(2, 0, 0)
			// Event handling can change the size of the screen
			if (renderer->player.windowUpdated) {
				SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
				mSDLGLDoViewport(renderer->viewportWidth, renderer->viewportHeight, v);
				renderer->player.windowUpdated = 0;
			}
#endif
		}

		if (mCoreSyncWaitFrameStart(&context->sync)) {
			v->postFrame(v, renderer->outputBuffer);
		}
		mCoreSyncWaitFrameEnd(&context->sync);
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
