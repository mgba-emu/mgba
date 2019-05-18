/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gl-common.h"
#ifdef BUILD_RASPI
#include "rpi-common.h"
#endif

#include <mgba/core/core.h>
#include <mgba/core/thread.h>

#ifdef __linux__
#include <malloc.h>
#endif

static bool mSDLGLES2Init(struct mSDLRenderer* renderer);
static void mSDLGLES2Runloop(struct mSDLRenderer* renderer, void* user);
static void mSDLGLES2Deinit(struct mSDLRenderer* renderer);

void mSDLGLES2Create(struct mSDLRenderer* renderer) {
	renderer->init = mSDLGLES2Init;
	renderer->deinit = mSDLGLES2Deinit;
	renderer->runloop = mSDLGLES2Runloop;
}

bool mSDLGLES2Init(struct mSDLRenderer* renderer) {
#ifdef BUILD_RASPI
	mRPIGLCommonInit(renderer);
#else
	mSDLGLCommonInit(renderer);
#endif

	size_t size = renderer->width * renderer->height * BYTES_PER_PIXEL;
#ifdef _WIN32
	renderer->outputBuffer = _aligned_malloc(size, 16);
#elif defined(__linux__)
	renderer->outputBuffer = memalign(16, size);
#else
	posix_memalign((void**) &renderer->outputBuffer, 16, size);
#endif
	memset(renderer->outputBuffer, 0, size);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, renderer->width);

	mGLES2ContextCreate(&renderer->gl2);
	renderer->gl2.d.user = renderer;
	renderer->gl2.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl2.d.lockIntegerScaling = renderer->lockIntegerScaling;
	renderer->gl2.d.filter = renderer->filter;
#ifdef BUILD_RASPI
	renderer->gl2.d.swap = mRPIGLCommonSwap;
#else
	renderer->gl2.d.swap = mSDLGLCommonSwap;
#endif
	renderer->gl2.d.init(&renderer->gl2.d, 0);
	renderer->gl2.d.setDimensions(&renderer->gl2.d, renderer->width, renderer->height);

	mSDLGLDoViewport(renderer->viewportWidth, renderer->viewportHeight, &renderer->gl2.d);
	return true;
}

void mSDLGLES2Runloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;
	struct VideoBackend* v = &renderer->gl2.d;

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

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			v->postFrame(v, renderer->outputBuffer);
		}
		mCoreSyncWaitFrameEnd(&context->impl->sync);
		v->drawFrame(v);
		v->swap(v);
	}
}

void mSDLGLES2Deinit(struct mSDLRenderer* renderer) {
	if (renderer->gl2.d.deinit) {
		renderer->gl2.d.deinit(&renderer->gl2.d);
	}
#ifdef BUILD_RASPI
	eglMakeCurrent(renderer->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(renderer->eglDisplay, renderer->eglSurface);
	eglDestroyContext(renderer->eglDisplay, renderer->eglContext);
	eglTerminate(renderer->eglDisplay);
	bcm_host_deinit();
#elif SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_DeleteContext(renderer->glCtx);
#endif
	free(renderer->outputBuffer);
}
