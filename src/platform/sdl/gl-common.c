/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include <mgba/core/version.h>

void mSDLGLDoViewport(int w, int h, struct VideoBackend* v) {
	v->resized(v, w, h);
	v->clear(v);
	v->swap(v);
	v->clear(v);
}

void mSDLGLCommonSwap(struct VideoBackend* context) {
	struct mSDLRenderer* renderer = (struct mSDLRenderer*) context->user;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_SwapWindow(renderer->window);
#else
	UNUSED(renderer);
	SDL_GL_SwapBuffers();
#endif
}

void mSDLGLCommonInit(struct mSDLRenderer* renderer) {
#ifndef COLOR_16_BIT
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
#else
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
#ifdef COLOR_5_6_5
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
#else
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
#endif
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
	renderer->window = SDL_CreateWindow(projectName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, renderer->viewportWidth, renderer->viewportHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | (SDL_WINDOW_FULLSCREEN_DESKTOP * renderer->player.fullscreen));
	renderer->glCtx = SDL_GL_CreateContext(renderer->window);
	SDL_GL_SetSwapInterval(1);
	SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
	renderer->player.window = renderer->window;
	if (renderer->lockIntegerScaling) {
		SDL_SetWindowMinimumSize(renderer->window, renderer->width, renderer->height);
	}
#else
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_OPENGL | (SDL_FULLSCREEN * renderer->fullscreen));
#else
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_OPENGL | (SDL_FULLSCREEN * renderer->fullscreen));
#endif
#endif
}
