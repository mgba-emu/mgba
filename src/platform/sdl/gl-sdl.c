/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gl-common.h"

#include "core/core.h"
#include "core/thread.h"
#include "platform/opengl/gl.h"

#define GB_GBA_CENTER ((VIDEO_HORIZONTAL_PIXELS - GB_VIDEO_HORIZONTAL_PIXELS + VIDEO_HORIZONTAL_PIXELS * (VIDEO_VERTICAL_PIXELS - GB_VIDEO_VERTICAL_PIXELS)) / 2)

static void _doViewport(int w, int h, struct VideoBackend* v) {
	v->resized(v, w, h);
	v->clear(v);
	v->swap(v);
	v->clear(v);
}

#ifdef M_CORE_GBA
#include "gba/gba.h"

static bool mSDLGLInitGBA(struct mSDLRenderer* renderer);
#endif
#ifdef M_CORE_GB
#include "gb/gb.h"

static bool mSDLGLInitGB(struct mSDLRenderer* renderer);
#endif

static void mSDLGLRunloop(struct mSDLRenderer* renderer, void* user);
static void mSDLGLDeinit(struct mSDLRenderer* renderer);

#ifdef M_CORE_GBA
void mSDLGLCreateGBA(struct mSDLRenderer* renderer) {
	renderer->init = mSDLGLInitGBA;
	renderer->deinit = mSDLGLDeinit;
	renderer->runloop = mSDLGLRunloop;
}

bool mSDLGLInitGBA(struct mSDLRenderer* renderer) {
	mSDLGLCommonInit(renderer);

	renderer->outputBuffer = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	memset(renderer->outputBuffer, 0, VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, VIDEO_HORIZONTAL_PIXELS);

	GBAGLContextCreate(&renderer->gl);
	renderer->gl.d.user = renderer;
	renderer->gl.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl.d.filter = renderer->filter;
	renderer->gl.d.swap = mSDLGLCommonSwap;
	renderer->gl.d.init(&renderer->gl.d, 0);

	_doViewport(renderer->viewportWidth, renderer->viewportHeight, &renderer->gl.d);
	return true;
}
#endif

#ifdef M_CORE_GB
void mSDLGLCreateGB(struct mSDLRenderer* renderer) {
	renderer->init = mSDLGLInitGB;
	renderer->deinit = mSDLGLDeinit;
	renderer->runloop = mSDLGLRunloop;
}

bool mSDLGLInitGB(struct mSDLRenderer* renderer) {
	mSDLGLCommonInit(renderer);

	// TODO: Pass texture size along
	renderer->outputBuffer = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	memset(renderer->outputBuffer, 0, VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer + GB_GBA_CENTER, VIDEO_HORIZONTAL_PIXELS);

	GBAGLContextCreate(&renderer->gl);
	renderer->gl.d.user = renderer;
	renderer->gl.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl.d.filter = renderer->filter;
	renderer->gl.d.swap = mSDLGLCommonSwap;
	renderer->gl.d.init(&renderer->gl.d, 0);

	_doViewport(renderer->viewportWidth, renderer->viewportHeight, &renderer->gl.d);
	return true;
}
#endif

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
				_doViewport(renderer->viewportWidth, renderer->viewportHeight, v);
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
