/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GL_H
#define GL_H

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "platform/video-backend.h"

struct GBAGLContext {
	struct VideoBackend d;

	GLuint tex;
};

void GBAGLContextCreate(struct GBAGLContext*);

#endif
