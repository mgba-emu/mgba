/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SDL_MAIN_H
#define SDL_MAIN_H

#include "renderers/video-software.h"

#include "sdl-audio.h"
#include "sdl-events.h"

#ifdef BUILD_GL
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

#ifdef BUILD_RASPI
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#include <SDL/SDL.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>

#include <bcm_host.h>
#pragma GCC diagnostic pop
#endif

struct SDLSoftwareRenderer {
	struct GBAVideoSoftwareRenderer d;
	struct GBASDLAudio audio;
	struct GBASDLEvents events;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_Window* window;
#ifndef BUILD_GL
	SDL_Texture* tex;
	SDL_Renderer* sdlRenderer;
#endif
#endif

	int viewportWidth;
	int viewportHeight;
	int ratio;

#ifdef BUILD_GL
	GLuint tex;
#endif

#ifdef BUILD_RASPI
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;
	EGL_DISPMANX_WINDOW_T window;
	GLuint tex;
	GLuint fragmentShader;
	GLuint vertexShader;
	GLuint program;
	GLuint bufferObject;
	GLuint texLocation;
	GLuint positionLocation;
#endif
};

bool GBASDLInit(struct SDLSoftwareRenderer* renderer);
void GBASDLDeinit(struct SDLSoftwareRenderer* renderer);
void GBASDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer);

#endif

