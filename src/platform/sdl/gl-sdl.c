/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gba/supervisor/thread.h"
#include "platform/opengl/gl.h"

static void _sdlSwap(struct VideoBackend* context) {
	struct SDLSoftwareRenderer* renderer = (struct SDLSoftwareRenderer*) context->user;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_SwapWindow(renderer->window);
#else
	SDL_GL_SwapBuffers();
#endif
}

static void _doViewport(int w, int h, struct VideoBackend* v) {
	v->resized(v, w, h);
	v->clear(v);
	v->swap(v);
	v->clear(v);
}

static bool GBASDLGLInit(struct SDLSoftwareRenderer* renderer);
static void GBASDLGLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer);
static void GBASDLGLDeinit(struct SDLSoftwareRenderer* renderer);

void GBASDLGLCreate(struct SDLSoftwareRenderer* renderer) {
	renderer->init = GBASDLGLInit;
	renderer->deinit = GBASDLGLDeinit;
	renderer->runloop = GBASDLGLRunloop;
}

bool GBASDLGLInit(struct SDLSoftwareRenderer* renderer) {
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
#else
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
#ifdef COLOR_16_BIT
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_OPENGL);
#else
	SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_OPENGL);
#endif
#endif

	renderer->d.outputBuffer = malloc(256 * 256 * BYTES_PER_PIXEL);
	renderer->d.outputBufferStride = 256;

	GBAGLContextCreate(&renderer->gl);
	renderer->gl.d.user = renderer;
	renderer->gl.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl.d.filter = renderer->filter;
	renderer->gl.d.swap = _sdlSwap;
	renderer->gl.d.init(&renderer->gl.d, 0);

	_doViewport(renderer->viewportWidth, renderer->viewportHeight, &renderer->gl.d);
	return true;
}

void GBASDLGLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer) {
	SDL_Event event;
	struct VideoBackend* v = &renderer->gl.d;

	while (context->state < THREAD_EXITING) {
		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &renderer->player, &event);
#if SDL_VERSION_ATLEAST(2, 0, 0)
			// Event handling can change the size of the screen
			if (renderer->player.windowUpdated) {
				SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
				_doViewport(renderer->viewportWidth, renderer->viewportHeight, v);
				renderer->player.windowUpdated = 0;
			}
#endif
		}

		if (GBASyncWaitFrameStart(&context->sync, context->frameskip)) {
			v->postFrame(v, renderer->d.outputBuffer);
		}
		v->drawFrame(v);
		GBASyncWaitFrameEnd(&context->sync);
		v->swap(v);
	}
}

void GBASDLGLDeinit(struct SDLSoftwareRenderer* renderer) {
	if (renderer->gl.d.deinit) {
		renderer->gl.d.deinit(&renderer->gl.d);
	}
	free(renderer->d.outputBuffer);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_DeleteContext(renderer->glCtx);
#endif
}
