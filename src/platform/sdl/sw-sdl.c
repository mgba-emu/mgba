/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/core/version.h>
#include <mgba-util/arm-algo.h>

static bool mSDLSWInit(struct mSDLRenderer* renderer);
static void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user);
static void mSDLSWDeinit(struct mSDLRenderer* renderer);

void mSDLSWCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLSWInit;
	renderer->deinit = mSDLSWDeinit;
	renderer->runloop = mSDLSWRunloop;
}

bool mSDLSWInit(struct mSDLRenderer* renderer) {
#if !SDL_VERSION_ATLEAST(2, 0, 0)
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_DOUBLEBUF | SDL_HWSURFACE);
#else
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);
#endif
#endif

	unsigned width, height;
	renderer->core->desiredVideoDimensions(renderer->core, &width, &height);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer->window = SDL_CreateWindow(projectName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderer->viewportWidth, renderer->viewportHeight, SDL_WINDOW_OPENGL | (SDL_WINDOW_FULLSCREEN_DESKTOP * renderer->player.fullscreen));
	SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
	renderer->player.window = renderer->window;
	renderer->sdlRenderer = SDL_CreateRenderer(renderer->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	renderer->sdlTex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, width, height);
#else
	renderer->sdlTex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, width, height);
#endif
#else
	renderer->sdlTex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, width, height);
#endif

	int stride;
	SDL_LockTexture(renderer->sdlTex, 0, (void**) &renderer->outputBuffer, &stride);
	renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, stride / BYTES_PER_PIXEL);
#else
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_LockSurface(surface);

	if (renderer->ratio == 1) {
		renderer->core->setVideoBuffer(renderer->core, surface->pixels, surface->pitch / BYTES_PER_PIXEL);
	} else {
#ifdef USE_PIXMAN
		renderer->outputBuffer = malloc(width * height * BYTES_PER_PIXEL);
		renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, width);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
		pixman_format_code_t format = PIXMAN_r5g6b5;
#else
		pixman_format_code_t format = PIXMAN_x1b5g5r5;
#endif
#else
		pixman_format_code_t format = PIXMAN_x8b8g8r8;
#endif
		renderer->pix = pixman_image_create_bits(format, width, height,
		    renderer->outputBuffer, width * BYTES_PER_PIXEL);
		renderer->screenpix = pixman_image_create_bits(format, renderer->viewportWidth, renderer->viewportHeight, surface->pixels, surface->pitch);

		pixman_transform_t transform;
		pixman_transform_init_identity(&transform);
		pixman_transform_scale(0, &transform, pixman_int_to_fixed(renderer->ratio), pixman_int_to_fixed(renderer->ratio));
		pixman_image_set_transform(renderer->pix, &transform);
		pixman_image_set_filter(renderer->pix, PIXMAN_FILTER_NEAREST, 0, 0);
#else
		return false;
#endif
	}
#endif

	return true;
}

void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Surface* surface = SDL_GetVideoSurface();
#endif

	while (context->state < THREAD_EXITING) {
		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->sync)) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
			SDL_UnlockTexture(renderer->sdlTex);
			SDL_RenderCopy(renderer->sdlRenderer, renderer->sdlTex, 0, 0);
			SDL_RenderPresent(renderer->sdlRenderer);
			int stride;
			SDL_LockTexture(renderer->sdlTex, 0, (void**) &renderer->outputBuffer, &stride);
			renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, stride / BYTES_PER_PIXEL);
#else
#ifdef USE_PIXMAN
			if (renderer->ratio > 1) {
				pixman_image_composite32(PIXMAN_OP_SRC, renderer->pix, 0, renderer->screenpix,
				    0, 0, 0, 0, 0, 0,
				    renderer->viewportWidth, renderer->viewportHeight);
			}
#else
			switch (renderer->ratio) {
#if defined(__ARM_NEON) && COLOR_16_BIT
			case 2:
				_neon2x(surface->pixels, renderer->outputBuffer, width, height);
				break;
			case 4:
				_neon4x(surface->pixels, renderer->outputBuffer, width, height);
				break;
#endif
			case 1:
				break;
			default:
				abort();
			}
#endif
			SDL_UnlockSurface(surface);
			SDL_Flip(surface);
			SDL_LockSurface(surface);
#endif
		}
		mCoreSyncWaitFrameEnd(&context->sync);
	}
}

void mSDLSWDeinit(struct mSDLRenderer* renderer) {
	if (renderer->ratio > 1) {
		free(renderer->outputBuffer);
	}
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_UnlockSurface(surface);
#ifdef USE_PIXMAN
	pixman_image_unref(renderer->pix);
	pixman_image_unref(renderer->screenpix);
#endif
#endif
}
