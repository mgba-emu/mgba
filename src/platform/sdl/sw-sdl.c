/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gba-thread.h"

#if defined(__ARM_NEON)
void _neon2x(void* dest, void* src, int width, int height);
void _neon4x(void* dest, void* src, int width, int height);
#endif

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

	SDL_LockTexture(renderer->tex, 0, &renderer->d.outputBuffer, &renderer->d.outputBufferStride);
	renderer->d.outputBufferStride /= BYTES_PER_PIXEL;
#else
	SDL_Surface* surface = SDL_GetVideoSurface();
	SDL_LockSurface(surface);

	if (renderer->ratio == 1) {
		renderer->d.outputBuffer = surface->pixels;
#ifdef COLOR_16_BIT
		renderer->d.outputBufferStride = surface->pitch / 2;
#else
		renderer->d.outputBufferStride = surface->pitch / 4;
#endif
	} else {
		renderer->d.outputBuffer = malloc(240 * 160 * BYTES_PER_PIXEL);
		renderer->d.outputBufferStride = 240;
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
			SDL_LockTexture(renderer->tex, 0, &renderer->d.outputBuffer, &renderer->d.outputBufferStride);
			renderer->d.outputBufferStride /= BYTES_PER_PIXEL;
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
			SDL_UnlockSurface(surface);
			SDL_Flip(surface);
			SDL_LockSurface(surface);
#endif
		}
		GBASyncWaitFrameEnd(&context->sync);
	}
}

void GBASDLDeinit(struct SDLSoftwareRenderer* renderer) {
	UNUSED(renderer);
}
