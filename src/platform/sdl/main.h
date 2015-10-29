/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_MAIN_H
#define SDL_MAIN_H

#include "gba/renderers/video-software.h"

#include "sdl-audio.h"
#include "sdl-events.h"

#ifdef BUILD_GL
#include "platform/opengl/gl.h"
#endif

#ifdef BUILD_RASPI
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include <SDL/SDL.h>
#include <EGL/egl.h>

#include <bcm_host.h>
#pragma GCC diagnostic pop
#endif

#ifdef BUILD_GLES2
#include "platform/opengl/gles2.h"
#endif

#ifdef USE_PIXMAN
#include <pixman.h>
#endif

struct SDLSoftwareRenderer {
	struct GBAVideoSoftwareRenderer d;
	struct GBASDLAudio audio;
	struct GBASDLEvents events;
	struct GBASDLPlayer player;

	bool (*init)(struct SDLSoftwareRenderer* renderer);
	void (*runloop)(struct GBAThread* context, struct SDLSoftwareRenderer* renderer);
	void (*deinit)(struct SDLSoftwareRenderer* renderer);

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
	SDL_Texture* sdlTex;
	SDL_Renderer* sdlRenderer;
	SDL_GLContext* glCtx;
#else
	bool fullscreen;
#endif

	int viewportWidth;
	int viewportHeight;
	int ratio;

	bool lockAspectRatio;
	bool filter;

#ifdef BUILD_GL
	struct GBAGLContext gl;
#endif
#ifdef BUILD_GLES2
	struct GBAGLES2Context gl2;
#endif

#ifdef USE_PIXMAN
	pixman_image_t* pix;
	pixman_image_t* screenpix;
#endif

#ifdef BUILD_RASPI
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGL_DISPMANX_WINDOW_T window;
#endif

#ifdef BUILD_PANDORA
	int fb;
	int odd;
	void* base[2];
#endif
};

void GBASDLSWCreate(struct SDLSoftwareRenderer* renderer);

#ifdef BUILD_GL
void GBASDLGLCreate(struct SDLSoftwareRenderer* renderer);
#endif

#ifdef BUILD_GLES2
void GBASDLGLES2Create(struct SDLSoftwareRenderer* renderer);
#endif
#endif
