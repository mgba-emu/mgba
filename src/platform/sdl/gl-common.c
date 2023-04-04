/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/core/version.h>

#ifdef USE_PNG
#include <mgba-util/image/png-io.h>
#include <mgba-util/vfs.h>
#endif

void mSDLGLDoViewport(int w, int h, struct VideoBackend* v) {
	v->contextResized(v, w, h);
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

bool mSDLGLCommonLoadBackground(struct VideoBackend* context) {
#ifdef USE_PNG
	struct mSDLRenderer* renderer = context->user;
	const char* bgImage = mCoreConfigGetValue(&renderer->core->config, "backgroundImage");
	if (!bgImage) {
		return false;
	}
	struct VFile* vf = VFileOpen(bgImage, O_RDONLY);
	if (!vf) {
		return false;
	}

	bool ok = false;
	png_structp png = PNGReadOpen(vf, 0);
	png_infop info = png_create_info_struct(png);
	png_infop end = png_create_info_struct(png);
	if (!png || !info || !end) {
		goto done;
	}

	if (!PNGReadHeader(png, info)) {
		goto done;
	}
	unsigned width = png_get_image_width(png, info);
	unsigned height = png_get_image_height(png, info);
	uint32_t* pixels = malloc(width * height * 4);
	if (!pixels) {
		goto done;
	}

	if (!PNGReadPixels(png, info, pixels, width, height, width) || !PNGReadFooter(png, end)) {
		free(pixels);
		goto done;
	}

	struct mRectangle dims = {
		.width = width,
		.height = height
	};
	context->setLayerDimensions(context, VIDEO_LAYER_BACKGROUND, &dims);
	context->setImage(context, VIDEO_LAYER_BACKGROUND, pixels);
	free(pixels);
	ok = true;

done:
	PNGReadClose(png, info, end);
	vf->close(vf);
	return ok;
#else
	UNUSED(context);
	return false;
#endif
}

bool mSDLGLCommonInit(struct mSDLRenderer* renderer) {
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
	if (!renderer->glCtx) {
		SDL_DestroyWindow(renderer->window);
		return false;
	}
	SDL_GL_SetSwapInterval(1);
	SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
	renderer->player.window = renderer->window;
	if (renderer->lockIntegerScaling) {
		SDL_SetWindowMinimumSize(renderer->window, renderer->width, renderer->height);
	}
#else
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
#ifdef COLOR_16_BIT
	SDL_Surface* surface = SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 16, SDL_OPENGL | SDL_RESIZABLE | (SDL_FULLSCREEN * renderer->player.fullscreen));
#else
	SDL_Surface* surface = SDL_SetVideoMode(renderer->viewportWidth, renderer->viewportHeight, 32, SDL_OPENGL | SDL_RESIZABLE | (SDL_FULLSCREEN * renderer->player.fullscreen));
#endif
	if (!surface) {
		return false;
	}
	SDL_WM_SetCaption(projectName, "");
#endif
	return true;
}

void mSDLGLCommonRunloop(struct mSDLRenderer* renderer, void* user) {
	struct mCoreThread* context = user;
	SDL_Event event;
	struct VideoBackend* v = renderer->backend;

	if (mSDLGLCommonLoadBackground(v)) {
		renderer->player.windowUpdated = true;

		struct mRectangle frame;
		v->layerDimensions(v, VIDEO_LAYER_IMAGE, &frame);
		int i;
		for (i = 0; i < VIDEO_LAYER_IMAGE; ++i) {
			struct mRectangle dims;
			v->layerDimensions(v, i, &dims);
			mRectangleCenter(&frame, &dims);
			v->setLayerDimensions(v, i, &dims);
		}

#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_SetWindowSize(renderer->window, frame.width * renderer->ratio, frame.height * renderer->ratio);
#endif
	}

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
		renderer->core->currentVideoSize(renderer->core, &renderer->width, &renderer->height);
		struct mRectangle dims;
		v->layerDimensions(v, VIDEO_LAYER_IMAGE, &dims);
		if (renderer->width != dims.width || renderer->height != dims.height) {
			renderer->core->setVideoBuffer(renderer->core, renderer->outputBuffer, renderer->width);
			dims.width = renderer->width;
			dims.height = renderer->height;
			v->setLayerDimensions(v, VIDEO_LAYER_IMAGE, &dims);
		}

		if (mCoreSyncWaitFrameStart(&context->impl->sync)) {
			v->setImage(v, VIDEO_LAYER_IMAGE, renderer->outputBuffer);
		}
		mCoreSyncWaitFrameEnd(&context->impl->sync);
		v->drawFrame(v);
		v->swap(v);
	}
}
