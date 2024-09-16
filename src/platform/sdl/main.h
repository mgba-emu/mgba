/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_MAIN_H
#define SDL_MAIN_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include "sdl-audio.h"
#include "sdl-events.h"

#ifdef BUILD_GL
#include "gl-common.h"
#include "platform/opengl/gl.h"
#endif

#if defined(BUILD_GLES2) || defined(BUILD_GLES3) || defined(USE_EPOXY)
#include "gl-common.h"
#include "platform/opengl/gles2.h"
#endif

#ifdef USE_PIXMAN
#include <pixman.h>
#endif

struct mCore;
struct mSDLRenderer {
	struct mCore* core;
	mColor* outputBuffer;

	struct mSDLAudio audio;
	struct mSDLEvents events;
	struct mSDLPlayer player;

	bool (*init)(struct mSDLRenderer* renderer);
	void (*runloop)(struct mSDLRenderer* renderer, void* user);
	void (*deinit)(struct mSDLRenderer* renderer);

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
	SDL_Texture* sdlTex;
	SDL_Renderer* sdlRenderer;
	SDL_GLContext* glCtx;
#endif

	unsigned width;
	unsigned height;
	int viewportWidth;
	int viewportHeight;
	int ratio;

	bool lockAspectRatio;
	bool lockIntegerScaling;
	bool interframeBlending;
	bool filter;

#ifdef BUILD_GL
	struct mGLContext gl;
#endif
#if defined(BUILD_GLES2) || defined(BUILD_GLES3) || defined(USE_EPOXY)
	struct mGLES2Context gl2;
#endif

	struct VideoBackend* backend;

#ifdef USE_PIXMAN
	pixman_image_t* pix;
	pixman_image_t* screenpix;
#endif
};

void mSDLSWCreate(struct mSDLRenderer* renderer);

#ifdef BUILD_GL
void mSDLGLCreate(struct mSDLRenderer* renderer);
#endif

#if defined(BUILD_GLES2) || defined(USE_EPOXY)
void mSDLGLES2Create(struct mSDLRenderer* renderer);
#endif

CXX_GUARD_END

#endif
