/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "main.h"

#include "gba/supervisor/thread.h"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#ifdef BUILD_GL
static const GLint _glVertices[] = {
	0, 0,
	256, 0,
	256, 256,
	0, 256
};

static const GLint _glTexCoords[] = {
	0, 0,
	1, 0,
	1, 1,
	0, 1
};
#endif

static void _doViewport(int w, int h, struct SDLSoftwareRenderer* renderer) {
	int drawW = w;
	int drawH = h;
	if (renderer->lockAspectRatio) {
		if (w * 2 > h * 3) {
			drawW = h * 3 / 2;
		} else if (w * 2 < h * 3) {
			drawH = w * 2 / 3;
		}
	}
	glViewport((w - drawW) / 2, (h - drawH) / 2, drawW, drawH);
	glClear(GL_COLOR_BUFFER_BIT);
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_SwapWindow(renderer->window);
#else
	SDL_GL_SwapBuffers();
#endif
	glClear(GL_COLOR_BUFFER_BIT);
}

bool GBASDLInit(struct SDLSoftwareRenderer* renderer) {
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
	SDL_GL_CreateContext(renderer->window);
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

	renderer->d.outputBuffer = malloc(VIDEO_HORIZONTAL_PIXELS * VIDEO_VERTICAL_PIXELS * BYTES_PER_PIXEL);
	renderer->d.outputBufferStride = VIDEO_HORIZONTAL_PIXELS;
	glGenTextures(1, &renderer->tex);
	glBindTexture(GL_TEXTURE_2D, renderer->tex);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	if (renderer->filter) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
#ifndef _WIN32
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif

#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 0);
#endif
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif

	_doViewport(renderer->viewportWidth, renderer->viewportHeight, renderer);
	return true;
}

void GBASDLRunloop(struct GBAThread* context, struct SDLSoftwareRenderer* renderer) {
	SDL_Event event;

	glEnable(GL_TEXTURE_2D);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_INT, 0, _glVertices);
	glTexCoordPointer(2, GL_INT, 0, _glTexCoords);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, 0, 0, 1);
	while (context->state < THREAD_EXITING) {
		while (SDL_PollEvent(&event)) {
			GBASDLHandleEvent(context, &renderer->player, &event);
#if SDL_VERSION_ATLEAST(2, 0, 0)
			// Event handling can change the size of the screen
			if (renderer->player.windowUpdated) {
				SDL_GetWindowSize(renderer->window, &renderer->viewportWidth, &renderer->viewportHeight);
				_doViewport(renderer->viewportWidth, renderer->viewportHeight, renderer);
				renderer->player.windowUpdated = 0;
			}
#endif
		}

		if (GBASyncWaitFrameStart(&context->sync, context->frameskip)) {
			glBindTexture(GL_TEXTURE_2D, renderer->tex);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, renderer->d.outputBuffer);
#else
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, renderer->d.outputBuffer);
#endif
#else
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VIDEO_HORIZONTAL_PIXELS, VIDEO_VERTICAL_PIXELS, GL_RGBA, GL_UNSIGNED_BYTE, renderer->d.outputBuffer);
#endif
			if (context->sync.videoFrameWait) {
				glFlush();
			}
		}
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		GBASyncWaitFrameEnd(&context->sync);
#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_GL_SwapWindow(renderer->window);
#else
		SDL_GL_SwapBuffers();
#endif
	}
}

void GBASDLDeinit(struct SDLSoftwareRenderer* renderer) {
	free(renderer->d.outputBuffer);
}
