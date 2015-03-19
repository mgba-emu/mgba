/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gba/supervisor/thread.h"
#include "util/arm-algo.h"

bool GBASDLInit(struct SDLSoftwareRenderer* renderer) {
#if !SDL_VERSION_ATLEAST(2, 0, 0)
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_DOUBLEBUF | SDL_HWSURFACE);
#else
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_DOUBLEBUF | SDL_HWSURFACE);
#endif
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer->window = SDL_CreateWindow(PROJECT_NAME, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderer->viewportWidth, renderer->viewportHeight, SDL_WINDOW_OPENGL | (SDL_WINDOW_FULLSCREEN_DESKTOP * renderer->events.fullscreen));
	SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
	renderer->events.window = renderer->window;
	renderer->sdlRenderer = SDL_CreateRenderer(renderer->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	renderer->tex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#else
	renderer->tex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_ABGR1555, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#endif
#else
	renderer->tex = SDL_CreateTexture(renderer->sdlRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS);
#endif

	SDL_LockTexture(renderer->tex, 0, (void**) &renderer->d.outputBuffer, &renderer->d.outputBufferStride);
	renderer->d.outputBufferStride /= BYTES_PER_PIXEL;
#else
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_LockSurface(surface);

	if (renderer->ratio == 1) {
		renderer->d.outputBuffer = surface->pixels;
		renderer->d.outputBufferStride = surface->pitch / BYTES_PER_PIXEL;
	} else {
#ifdef USE_PIXMAN
		renderer->d.outputBuffer = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
		renderer->d.outputBufferStride = VIDEO_HORIZONTAL_PIXELS;
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
		pixman_format_code_t format = PIXMAN_r5g6b5;
#else
		pixman_format_code_t format = PIXMAN_x1b5g5r5;
#endif
#else
		pixman_format_code_t format = PIXMAN_x8b8g8r8;
#endif
		renderer->pix = pixman_image_create_bits(format, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS,
			renderer->d.outputBuffer, renderer->d.outputBufferStride * BYTES_PER_PIXEL);
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

void GBASDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer) {
	SDL_Event event;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Surface* surface = SDL_GetVideoSurface();
#endif

	while (context->state < THREAD_EXITING) {
		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &renderer->events, &event);
		}

		if (GBASyncWaitFrameStart(&context->sync, context->frameskip)) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
			SDL_UnlockTexture(renderer->tex);
			SDL_RenderCopy(renderer->sdlRenderer, renderer->tex, 0, 0);
			SDL_RenderPresent(renderer->sdlRenderer);
			SDL_LockTexture(renderer->tex, 0, (void**) &renderer->d.outputBuffer, &renderer->d.outputBufferStride);
			renderer->d.outputBufferStride /= BYTES_PER_PIXEL;
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
				_neon2x(surface->pixels, renderer->d.outputBuffer, 240, 160);
				break;
			case 4:
				_neon4x(surface->pixels, renderer->d.outputBuffer, 240, 160);
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
		GBASyncWaitFrameEnd(&context->sync);
	}
}

void GBASDLDeinit(struct SDLSoftwareRenderer* renderer) {
	if (renderer->ratio > 1) {
		free(renderer->d.outputBuffer);
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
