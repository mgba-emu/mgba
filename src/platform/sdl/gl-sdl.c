/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gl-common.h"

#include "gba/supervisor/thread.h"
#include "platform/opengl/gl.h"

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
	GBASDLGLCommonInit(renderer);

	renderer->d.outputBuffer = malloc(256 * 256 * BYTES_PER_PIXEL);
	renderer->d.outputBufferStride = 256;

	GBAGLContextCreate(&renderer->gl);
	renderer->gl.d.user = renderer;
	renderer->gl.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl.d.filter = renderer->filter;
	renderer->gl.d.swap = GBASDLGLCommonSwap;
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
		GBASyncWaitFrameEnd(&context->sync);
		v->drawFrame(v);
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
