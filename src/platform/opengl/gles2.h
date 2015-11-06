/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GLES2_H
#define GLES2_H

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#elif defined(BUILD_GL)
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#else
#include <GLES2/gl2.h>
#endif

#include "platform/video-backend.h"

union GBAGLES2UniformValue {
	GLfloat f;
	GLint i;
	GLuint ui;
	GLboolean b;
	GLfloat fvec2[2];
	GLfloat fvec3[3];
	GLfloat fvec4[4];
	GLint ivec2[2];
	GLint ivec3[3];
	GLint ivec4[4];
	GLboolean bvec2[2];
	GLboolean bvec3[3];
	GLboolean bvec4[4];
	GLfloat fmat2x2[4];
	GLfloat fmat3x3[9];
	GLfloat fmat4x4[16];
};

struct GBAGLES2Uniform {
	const char* name;
	GLenum type;
	union GBAGLES2UniformValue value;
	GLuint location;
};

struct GBAGLES2Shader {
	unsigned width;
	unsigned height;
	bool filter;
	bool blend;
	GLuint tex;
	GLuint fbo;
	GLuint fragmentShader;
	GLuint vertexShader;
	GLuint program;
	GLuint texLocation;
	GLuint positionLocation;

	struct GBAGLES2Uniform* uniforms;
	size_t nUniforms;
};

struct GBAGLES2Context {
	struct VideoBackend d;

	GLuint tex;
	GLuint texLocation;
	GLuint positionLocation;

	struct GBAGLES2Shader initialShader;
	struct GBAGLES2Shader finalShader;

	struct GBAGLES2Shader* shaders;
	size_t nShaders;
};

struct GBAGLES2ShaderMetadata {
	const char* name;
	const char* author;
	const char* description;
};

void GBAGLES2ContextCreate(struct GBAGLES2Context*);

void GBAGLES2ShaderInit(struct GBAGLES2Shader*, const char* vs, const char* fs, int width, int height, struct GBAGLES2Uniform* uniforms, size_t nUniforms);
void GBAGLES2ShaderDeinit(struct GBAGLES2Shader*);
void GBAGLES2ShaderAttach(struct GBAGLES2Context*, struct GBAGLES2Shader*, size_t nShaders);
void GBAGLES2ShaderDetach(struct GBAGLES2Context*);

struct VDir;
bool GBAGLES2ShaderLoad(struct GBAGLES2Shader**, size_t* nShaders, struct GBAGLES2ShaderMetadata*, struct VDir*);
void GBAGLES2ShaderFree(struct GBAGLES2Shader*, size_t nShaders);

#endif
