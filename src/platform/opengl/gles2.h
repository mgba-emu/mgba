/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GLES2_H
#define GLES2_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#elif defined(BUILD_GL)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#elif defined(BUILD_GLES3)
#include <GLES3/gl3.h>
#else
#include <GLES2/gl2.h>
#endif

#include <mgba/feature/video-backend.h>

union mGLES2UniformValue {
	GLfloat f;
	GLint i;
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

struct mGLES2Uniform {
	const char* name;
	GLenum type;
	union mGLES2UniformValue value;
	GLuint location;
	union mGLES2UniformValue min;
	union mGLES2UniformValue max;
	const char* readableName;
};

struct mGLES2Shader {
	int width;
	int height;
	bool integerScaling;
	bool filter;
	bool blend;
	bool dirty;
	GLuint tex;
	GLuint fbo;
	GLuint vao;
	GLuint fragmentShader;
	GLuint vertexShader;
	GLuint program;
	GLuint texLocation;
	GLuint texSizeLocation;
	GLuint positionLocation;
	GLuint outputSizeLocation;

	struct mGLES2Uniform* uniforms;
	size_t nUniforms;
};

struct mGLES2Context {
	struct VideoBackend d;

	GLuint tex[VIDEO_LAYER_MAX];
	GLuint vbo;

	struct mRectangle layerDims[VIDEO_LAYER_MAX];
	struct mSize imageSizes[VIDEO_LAYER_MAX];
	int x;
	int y;
	unsigned width;
	unsigned height;

	struct mGLES2Shader initialShader;
	struct mGLES2Shader finalShader;
	struct mGLES2Shader interframeShader;

	struct mGLES2Shader* shaders;
	size_t nShaders;
};

void mGLES2ContextCreate(struct mGLES2Context*);
void mGLES2ContextUseFramebuffer(struct mGLES2Context*);

void mGLES2ShaderInit(struct mGLES2Shader*, const char* vs, const char* fs, int width, int height, bool integerScaling, struct mGLES2Uniform* uniforms, size_t nUniforms);
void mGLES2ShaderDeinit(struct mGLES2Shader*);
void mGLES2ShaderAttach(struct mGLES2Context*, struct mGLES2Shader*, size_t nShaders);
void mGLES2ShaderDetach(struct mGLES2Context*);

struct VDir;
bool mGLES2ShaderLoad(struct VideoShader*, struct VDir*);
void mGLES2ShaderFree(struct VideoShader*);

CXX_GUARD_END

#endif
