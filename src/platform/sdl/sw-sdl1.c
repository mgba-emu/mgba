/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/core/version.h>

static bool mSDLSWInit(struct mSDLRenderer* renderer);
static void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user);
static void mSDLSWDeinit(struct mSDLRenderer* renderer);

void mSDLSWCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLSWInit;
	renderer->deinit = mSDLSWDeinit;
	renderer->runloop = mSDLSWRunloop;
}

bool mSDLSWInit(struct mSDLRenderer* renderer) {
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_DOUBLEBUF | SDL_HWSURFACE | (SDL_FULLSCREEN * renderer->player.fullscreen));
#else
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_DOUBLEBUF | SDL_HWSURFACE | (SDL_FULLSCREEN * renderer->player.fullscreen));
#endif
	SDL_WM_SetCaption(projectName, "");

	unsigned width, height;
	renderer->core->baseVideoSize(renderer->core, &width, &height);
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

	return true;
}

void mSDLSWRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;
	SDL_Surface* surface = SDL_GetVideoSurface();

	while (mCoreThreadIsActive(context)) {
		while (SDL_PollEvent(&event)) {
			mSDLHandleEvent(context, &renderer->player, &event);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
#ifdef USE_PIXMAN
			if (renderer->ratio > 1) {
				pixman_image_composite32(PIXMAN_OP_SRC, renderer->pix, 0, renderer->screenpix,
				    0, 0, 0, 0, 0, 0,
				    renderer->viewportWidth, renderer->viewportHeight);
			}
#else
			if (renderer->ratio != 1) {
				abort();
			}
#endif
			SDL_UnlockSurface(surface);
			SDL_Flip(surface);
			SDL_LockSurface(surface);
		}
		mCoreSyncWaitFrameEnd(&context->impl->sync);
	}
}

void mSDLSWDeinit(struct mSDLRenderer* renderer) {
	if (renderer->ratio > 1) {
		free(renderer->outputBuffer);
#ifdef USE_PIXMAN
		pixman_image_unref(renderer->pix);
		pixman_image_unref(renderer->screenpix);
#endif
	}
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_UnlockSurface(surface);
}
