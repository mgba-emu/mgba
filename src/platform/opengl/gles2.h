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

struct GBAGLES2Shader {
	unsigned width;
	unsigned height;
	bool filter;
	bool blend;
	GLuint tex;
	GLuint fbo;
	GLuint fragmentShader;
	GLuint program;
	GLuint texLocation;
	GLuint positionLocation;
};

struct GBAGLES2Context {
	struct VideoBackend d;

	GLuint tex;
	GLuint fragmentShader;
	GLuint vertexShader;
	GLuint nullVertexShader;
	GLuint program;
	GLuint bufferObject;
	GLuint texLocation;
	GLuint positionLocation;
	GLuint gammaLocation;
	GLuint biasLocation;
	GLuint scaleLocation;

	GLfloat gamma;
	GLfloat bias[3];
	GLfloat scale[3];

	struct GBAGLES2Shader* shader;
};

void GBAGLES2ContextCreate(struct GBAGLES2Context*);

void GBAGLES2ShaderInit(struct GBAGLES2Shader*, const char*, int width, int height);
void GBAGLES2ShaderDeinit(struct GBAGLES2Shader*);
void GBAGLES2ShaderAttach(struct GBAGLES2Context*, struct GBAGLES2Shader*);
void GBAGLES2ShaderDetach(struct GBAGLES2Context*);

#endif
