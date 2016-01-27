/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gl-common.h"

#include "gba/supervisor/thread.h"
#ifdef M_CORE_GB
#include "gb/gb.h"
#endif
#include "platform/opengl/gl.h"

#define GB_GBA_CENTER ((VIDEO_HORIZONTAL_PIXELS - GB_VIDEO_HORIZONTAL_PIXELS + VIDEO_HORIZONTAL_PIXELS * (VIDEO_VERTICAL_PIXELS - GB_VIDEO_VERTICAL_PIXELS)) / 2)

static void _doViewport(int w, int h, struct VideoBackend* v) {
	v->resized(v, w, h);
	v->clear(v);
	v->swap(v);
	v->clear(v);
}

#ifdef M_CORE_GBA
static bool mSDLGLInitGBA(struct mSDLRenderer* renderer);
static void mSDLGLRunloopGBA(struct mSDLRenderer* renderer, void* user);
static void mSDLGLDeinitGBA(struct mSDLRenderer* renderer);
#endif
#ifdef M_CORE_GB
static bool mSDLGLInitGB(struct mSDLRenderer* renderer);
static void mSDLGLRunloopGB(struct mSDLRenderer* renderer, void* user);
static void mSDLGLDeinitGB(struct mSDLRenderer* renderer);
#endif

#ifdef M_CORE_GBA
void mSDLGLCreate(struct mSDLRenderer* renderer) {
	renderer->init = mSDLGLInitGBA;
	renderer->deinit = mSDLGLDeinitGBA;
	renderer->runloop = mSDLGLRunloopGBA;
}

bool mSDLGLInitGBA(struct mSDLRenderer* renderer) {
	mSDLGLCommonInit(renderer);

	renderer->d.outputBuffer = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	renderer->d.outputBufferStride = VIDEO_HORIZONTAL_PIXELS;

	GBAGLContextCreate(&renderer->gl);
	renderer->gl.d.user = renderer;
	renderer->gl.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl.d.filter = renderer->filter;
	renderer->gl.d.swap = mSDLGLCommonSwap;
	renderer->gl.d.init(&renderer->gl.d, 0);

	_doViewport(renderer->viewportWidth, renderer->viewportHeight, &renderer->gl.d);
	return true;
}

void mSDLGLRunloopGBA(struct mSDLRenderer* renderer, void* user) {
	struct GBAThread* context = user;
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

		if (GBASyncWaitFrameStart(&context->sync)) {
			v->postFrame(v, renderer->d.outputBuffer);
		}
		GBASyncWaitFrameEnd(&context->sync);
		v->drawFrame(v);
		v->swap(v);
	}
}

void mSDLGLDeinitGBA(struct mSDLRenderer* renderer) {
	if (renderer->gl.d.deinit) {
		renderer->gl.d.deinit(&renderer->gl.d);
	}
	free(renderer->d.outputBuffer);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_DeleteContext(renderer->glCtx);
#endif
}
#endif

#ifdef M_CORE_GB
void mSDLGLCreateGB(struct mSDLRenderer* renderer) {
	renderer->init = mSDLGLInitGB;
	renderer->deinit = mSDLGLDeinitGB;
	renderer->runloop = mSDLGLRunloopGB;
}

bool mSDLGLInitGB(struct mSDLRenderer* renderer) {
	mSDLGLCommonInit(renderer);

	// TODO: Pass texture size along
	color_t* buf = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	memset(buf, 0, VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	renderer->gb.outputBuffer = buf + GB_GBA_CENTER;
	renderer->gb.outputBufferStride = VIDEO_HORIZONTAL_PIXELS;

	GBAGLContextCreate(&renderer->gl);
	renderer->gl.d.user = renderer;
	renderer->gl.d.lockAspectRatio = renderer->lockAspectRatio;
	renderer->gl.d.filter = renderer->filter;
	renderer->gl.d.swap = mSDLGLCommonSwap;
	renderer->gl.d.init(&renderer->gl.d, 0);

	_doViewport(renderer->viewportWidth, renderer->viewportHeight, &renderer->gl.d);
	return true;
}

void mSDLGLRunloopGB(struct mSDLRenderer* renderer, void* user) {
	struct GB* gb = user;
	SDL_Event event;
	struct VideoBackend* v = &renderer->gl.d;
	int activeKeys = 0;
	gb->keySource = &activeKeys;

	while (true) {
		int64_t frameCounter = gb->video.frameCounter;
		while (gb->video.frameCounter == frameCounter) {
			LR35902Tick(gb->cpu);
		}
		while (SDL_PollEvent(&event)) {
			// TODO: Refactor out
			if (event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) {
				int key;
#if !defined(BUILD_PANDORA) && SDL_VERSION_ATLEAST(2, 0, 0)
				key = GBAInputMapKey(renderer->player.bindings, SDL_BINDING_KEY, event.key.keysym.scancode);
#else
				key = GBAInputMapKey(renderer->player.bindings, SDL_BINDING_KEY, event.key.keysym.sym);
#endif
				if (key != GBA_KEY_NONE) {
					if (event.type == SDL_KEYDOWN) {
						activeKeys |= 1 << key;
					} else {
						activeKeys &= ~(1 << key);
					}
				}
			}
			if (event.type == SDL_QUIT) {
				return;
			}

#if SDL_VERSION_ATLEAST(2, 0, 0)
			// Event handling can change the size of the screen
			if (renderer->player.windowUpdated) {
				SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
				_doViewport(renderer->viewportWidth, renderer->viewportHeight, v);
				renderer->player.windowUpdated = 0;
			}
#endif
		}

		v->postFrame(v, renderer->gb.outputBuffer - GB_GBA_CENTER);
		v->drawFrame(v);
		v->swap(v);
	}
}

void mSDLGLDeinitGB(struct mSDLRenderer* renderer) {
	if (renderer->gl.d.deinit) {
		renderer->gl.d.deinit(&renderer->gl.d);
	}
	free(renderer->gb.outputBuffer - GB_GBA_CENTER);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_DeleteContext(renderer->glCtx);
#endif
}
#endif
