/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GLES2_H
#define GLES2_H

#ifdef BUILD_GL
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#else
#include <GLES2/gl2.h>
#endif

#include "platform/video-backend.h"

struct GBAGLES2Context {
	struct VideoBackend d;

	GLuint tex;
	GLuint fragmentShader;
	GLuint vertexShader;
	GLuint program;
	GLuint bufferObject;
	GLuint texLocation;
	GLuint positionLocation;
};

void GBAGLES2ContextCreate(struct GBAGLES2Context*);

#endif
