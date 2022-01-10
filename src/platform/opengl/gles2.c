/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gles2.h"

#include <mgba/core/log.h>
#include <mgba-util/configuration.h>
#include <mgba-util/formatting.h>
#include <mgba-util/math.h>
#include <mgba-util/vector.h>
#include <mgba-util/vfs.h>

mLOG_DECLARE_CATEGORY(OPENGL);
mLOG_DEFINE_CATEGORY(OPENGL, "OpenGL", "video.ogl");

#define MAX_PASSES 8

static const GLchar* const _gles2Header =
	"#version 100\n"
	"precision mediump float;\n";

static const GLchar* const _gl2Header =
	"#version 120\n";

static const GLchar* const _gl32VHeader =
	"#version 150 core\n"
	"#define attribute in\n"
	"#define varying out\n";

static const GLchar* const _gl32FHeader =
	"#version 150 core\n"
	"#define varying in\n"
	"#define texture2D texture\n"
	"out vec4 compat_FragColor;\n"
	"#define gl_FragColor compat_FragColor\n";

static const char* const _vertexShader =
	"attribute vec4 position;\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	gl_Position = position;\n"
	"	texCoord = (position.st + vec2(1.0, -1.0)) * vec2(0.5, -0.5);\n"
	"}";

static const char* const _nullVertexShader =
	"attribute vec4 position;\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	gl_Position = position;\n"
	"	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);\n"
	"}";

static const char* const _fragmentShader =
	"varying vec2 texCoord;\n"
	"uniform sampler2D tex;\n"
	"uniform float gamma;\n"
	"uniform vec3 desaturation;\n"
	"uniform vec3 scale;\n"
	"uniform vec3 bias;\n"

	"void main() {\n"
	"	vec4 color = texture2D(tex, texCoord);\n"
	"	color.a = 1.;\n"
	"	float average = dot(color.rgb, vec3(1.)) / 3.;\n"
	"	color.rgb = mix(color.rgb, vec3(average), desaturation);\n"
	"	color.rgb = scale * pow(color.rgb, vec3(gamma, gamma, gamma)) + bias;\n"
	"	gl_FragColor = color;\n"
	"}";

static const char* const _nullFragmentShader =
	"varying vec2 texCoord;\n"
	"uniform sampler2D tex;\n"

	"void main() {\n"
	"	vec4 color = texture2D(tex, texCoord);\n"
	"	color.a = 1.;\n"
	"	gl_FragColor = color;\n"
	"}";

static const char* const _interframeFragmentShader =
	"varying vec2 texCoord;\n"
	"uniform sampler2D tex;\n"

	"void main() {\n"
	"	vec4 color = texture2D(tex, texCoord);\n"
	"	color.a = 0.5;\n"
	"	gl_FragColor = color;\n"
	"}";

static const GLfloat _vertices[] = {
	-1.f, -1.f,
	-1.f, 1.f,
	1.f, 1.f,
	1.f, -1.f,
};

#ifdef __APPLE__
#include <mgba/gba/core.h>
#include <dispatch/dispatch.h>

static void dispatch_main_reentrant(void (^block)()) {
	dispatch_queue_t queue = dispatch_get_main_queue();
	if (dispatch_queue_get_label(queue) == dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL)) {
		block();
	}
	else {
		if (__postEvent) {
			__postEvent(block);
		}
		else {
			block();
		}
	}
}
#endif

static void mGLES2ContextInit(struct VideoBackend* v, WHandle handle) {
	UNUSED(handle);
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	struct mGLES2Context* context = (struct mGLES2Context*) v;
	v->width = 1;
	v->height = 1;
	glGenTextures(1, &context->tex);
	glBindTexture(GL_TEXTURE_2D, context->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenBuffers(1, &context->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, context->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_vertices), _vertices, GL_STATIC_DRAW);

	struct mGLES2Uniform* uniforms = malloc(sizeof(struct mGLES2Uniform) * 4);
	uniforms[0].name = "gamma";
	uniforms[0].readableName = "Gamma";
	uniforms[0].type = GL_FLOAT;
	uniforms[0].value.f = 1.0f;
	uniforms[0].min.f = 0.1f;
	uniforms[0].max.f = 3.0f;
	uniforms[1].name = "scale";
	uniforms[1].readableName = "Scale";
	uniforms[1].type = GL_FLOAT_VEC3;
	uniforms[1].value.fvec3[0] = 1.0f;
	uniforms[1].value.fvec3[1] = 1.0f;
	uniforms[1].value.fvec3[2] = 1.0f;
	uniforms[1].min.fvec3[0] = -1.0f;
	uniforms[1].min.fvec3[1] = -1.0f;
	uniforms[1].min.fvec3[2] = -1.0f;
	uniforms[1].max.fvec3[0] = 2.0f;
	uniforms[1].max.fvec3[1] = 2.0f;
	uniforms[1].max.fvec3[2] = 2.0f;
	uniforms[2].name = "bias";
	uniforms[2].readableName = "Bias";
	uniforms[2].type = GL_FLOAT_VEC3;
	uniforms[2].value.fvec3[0] = 0.0f;
	uniforms[2].value.fvec3[1] = 0.0f;
	uniforms[2].value.fvec3[2] = 0.0f;
	uniforms[2].min.fvec3[0] = -1.0f;
	uniforms[2].min.fvec3[1] = -1.0f;
	uniforms[2].min.fvec3[2] = -1.0f;
	uniforms[2].max.fvec3[0] = 1.0f;
	uniforms[2].max.fvec3[1] = 1.0f;
	uniforms[2].max.fvec3[2] = 1.0f;
	uniforms[3].name = "desaturation";
	uniforms[3].readableName = "Desaturation";
	uniforms[3].type = GL_FLOAT_VEC3;
	uniforms[3].value.fvec3[0] = 0.0f;
	uniforms[3].value.fvec3[1] = 0.0f;
	uniforms[3].value.fvec3[2] = 0.0f;
	uniforms[3].min.fvec3[0] = 0.0f;
	uniforms[3].min.fvec3[1] = 0.0f;
	uniforms[3].min.fvec3[2] = 0.0f;
	uniforms[3].max.fvec3[0] = 1.0f;
	uniforms[3].max.fvec3[1] = 1.0f;
	uniforms[3].max.fvec3[2] = 1.0f;
	mGLES2ShaderInit(&context->initialShader, _vertexShader, _fragmentShader, -1, -1, false, uniforms, 4);
	mGLES2ShaderInit(&context->finalShader, 0, 0, 0, 0, false, 0, 0);
	mGLES2ShaderInit(&context->interframeShader, 0, _interframeFragmentShader, -1, -1, false, 0, 0);

#ifdef BUILD_GLES3
	if (context->initialShader.vao != (GLuint) -1) {
		glBindVertexArray(context->initialShader.vao);
		glBindBuffer(GL_ARRAY_BUFFER, context->vbo);
		glBindVertexArray(context->finalShader.vao);
		glBindBuffer(GL_ARRAY_BUFFER, context->vbo);
		glBindVertexArray(context->interframeShader.vao);
		glBindBuffer(GL_ARRAY_BUFFER, context->vbo);
		glBindVertexArray(0);
	}
#endif

	glDeleteFramebuffers(1, &context->finalShader.fbo);
	glDeleteTextures(1, &context->finalShader.tex);
	context->finalShader.fbo = 0;
	context->finalShader.tex = 0;
#ifdef __APPLE__
	});
#endif
}

static void mGLES2ContextSetDimensions(struct VideoBackend* v, unsigned width, unsigned height) {
	struct mGLES2Context* context = (struct mGLES2Context*) v;
	if (width == v->width && height == v->height) {
		return;
	}
	v->width = width;
	v->height = height;

#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif

	glBindTexture(GL_TEXTURE_2D, context->tex);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 0);
#endif
#elif defined(__BIG_ENDIAN__)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif

#ifdef __APPLE__
	});
#endif

	size_t n;
	for (n = 0; n < context->nShaders; ++n) {
		if (context->shaders[n].width < 0 || context->shaders[n].height < 0) {
			context->shaders[n].dirty = true;
		}
	}
	context->initialShader.dirty = true;
	context->interframeShader.dirty = true;
}

static void mGLES2ContextDeinit(struct VideoBackend* v) {
	struct mGLES2Context* context = (struct mGLES2Context*) v;
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	glDeleteTextures(1, &context->tex);
	glDeleteBuffers(1, &context->vbo);
	mGLES2ShaderDeinit(&context->initialShader);
	mGLES2ShaderDeinit(&context->finalShader);
	mGLES2ShaderDeinit(&context->interframeShader);
#ifdef __APPLE__
	});
#endif
	free(context->initialShader.uniforms);
}

static void mGLES2ContextResized(struct VideoBackend* v, unsigned w, unsigned h) {
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	struct mGLES2Context* context = (struct mGLES2Context*) v;
	unsigned drawW = w;
	unsigned drawH = h;
	if (v->lockAspectRatio) {
		lockAspectRatioUInt(v->width, v->height, &drawW, &drawH);
	}
	if (v->lockIntegerScaling) {
		lockIntegerRatioUInt(v->width, &drawW);
		lockIntegerRatioUInt(v->height, &drawH);
	}
	size_t n;
	for (n = 0; n < context->nShaders; ++n) {
		if (context->shaders[n].width == 0 || context->shaders[n].height == 0) {
			context->shaders[n].dirty = true;
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport((w - drawW) / 2, (h - drawH) / 2, drawW, drawH);
#ifdef __APPLE__
	});
#endif
}

static void mGLES2ContextClear(struct VideoBackend* v) {
	UNUSED(v);
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
#ifdef __APPLE__
	});
#endif
}

void _drawShader(struct mGLES2Context* context, struct mGLES2Shader* shader) {
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	int drawW = shader->width;
	int drawH = shader->height;
	int padW = 0;
	int padH = 0;
	if (!drawW) {
		drawW = viewport[2];
		padW = viewport[0];
	} else if (shader->width < 0) {
		drawW = context->d.width * -shader->width;
	}
	if (!drawH) {
		drawH = viewport[3];
		padH = viewport[1];
	} else if (shader->height < 0) {
		drawH = context->d.height * -shader->height;
	}
	if (shader->integerScaling) {
		padW = 0;
		padH = 0;
		drawW -= drawW % context->d.width;
		drawH -= drawH % context->d.height;
	}

	if (shader->dirty) {
		if (shader->tex && (shader->width <= 0 || shader->height <= 0)) {
			GLint oldTex;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTex);
			glBindTexture(GL_TEXTURE_2D, shader->tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, drawW, drawH, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
			glBindTexture(GL_TEXTURE_2D, oldTex);
		}
		shader->dirty = false;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, shader->fbo);
	glViewport(padW, padH, drawW, drawH);
	if (shader->blend) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glDisable(GL_BLEND);
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, shader->filter ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, shader->filter ? GL_LINEAR : GL_NEAREST);
	glUseProgram(shader->program);
	glUniform1i(shader->texLocation, 0);
	glUniform2f(shader->texSizeLocation, context->d.width - padW, context->d.height - padH);
#ifdef BUILD_GLES3
	if (shader->vao != (GLuint) -1) {
		glBindVertexArray(shader->vao);
	} else
#endif
	{
		glBindBuffer(GL_ARRAY_BUFFER, context->vbo);
		glEnableVertexAttribArray(shader->positionLocation);
		glVertexAttribPointer(shader->positionLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}
	size_t u;
	for (u = 0; u < shader->nUniforms; ++u) {
		struct mGLES2Uniform* uniform = &shader->uniforms[u];
		switch (uniform->type) {
		case GL_FLOAT:
			glUniform1f(uniform->location, uniform->value.f);
			break;
		case GL_INT:
			glUniform1i(uniform->location, uniform->value.i);
			break;
		case GL_BOOL:
			glUniform1i(uniform->location, uniform->value.b);
			break;
		case GL_FLOAT_VEC2:
			glUniform2fv(uniform->location, 1, uniform->value.fvec2);
			break;
		case GL_FLOAT_VEC3:
			glUniform3fv(uniform->location, 1, uniform->value.fvec3);
			break;
		case GL_FLOAT_VEC4:
			glUniform4fv(uniform->location, 1, uniform->value.fvec4);
			break;
		case GL_INT_VEC2:
			glUniform2iv(uniform->location, 1, uniform->value.ivec2);
			break;
		case GL_INT_VEC3:
			glUniform3iv(uniform->location, 1, uniform->value.ivec3);
			break;
		case GL_INT_VEC4:
			glUniform4iv(uniform->location, 1, uniform->value.ivec4);
			break;
		case GL_BOOL_VEC2:
			glUniform2i(uniform->location, uniform->value.bvec2[0], uniform->value.bvec2[1]);
			break;
		case GL_BOOL_VEC3:
			glUniform3i(uniform->location, uniform->value.bvec3[0], uniform->value.bvec3[1], uniform->value.bvec3[2]);
			break;
		case GL_BOOL_VEC4:
			glUniform4i(uniform->location, uniform->value.bvec4[0], uniform->value.bvec4[1], uniform->value.bvec4[2], uniform->value.bvec4[3]);
			break;
		case GL_FLOAT_MAT2:
			glUniformMatrix2fv(uniform->location, 1, GL_FALSE, uniform->value.fmat2x2);
			break;
		case GL_FLOAT_MAT3:
			glUniformMatrix3fv(uniform->location, 1, GL_FALSE, uniform->value.fmat3x3);
			break;
		case GL_FLOAT_MAT4:
			glUniformMatrix4fv(uniform->location, 1, GL_FALSE, uniform->value.fmat4x4);
			break;
		}
	}
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindTexture(GL_TEXTURE_2D, shader->tex);
#ifdef __APPLE__
	});
#endif
}

void mGLES2ContextDrawFrame(struct VideoBackend* v) {
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	struct mGLES2Context* context = (struct mGLES2Context*) v;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, context->tex);

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	context->finalShader.filter = v->filter;
	context->finalShader.dirty = true;
	_drawShader(context, &context->initialShader);
	if (v->interframeBlending) {
		context->interframeShader.blend = true;
		glViewport(0, 0, viewport[2], viewport[3]);
		_drawShader(context, &context->interframeShader);
	}
	size_t n;
	for (n = 0; n < context->nShaders; ++n) {
		glViewport(0, 0, viewport[2], viewport[3]);
		_drawShader(context, &context->shaders[n]);
	}
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	_drawShader(context, &context->finalShader);
	if (v->interframeBlending) {
		context->interframeShader.blend = false;
		glBindTexture(GL_TEXTURE_2D, context->tex);
		_drawShader(context, &context->initialShader);
		glViewport(0, 0, viewport[2], viewport[3]);
		_drawShader(context, &context->interframeShader);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glUseProgram(0);
#ifdef BUILD_GLES3
	if (context->finalShader.vao != (GLuint) -1) {
		glBindVertexArray(0);
	}
#endif
#ifdef __APPLE__
	});
#endif
}

void mGLES2ContextPostFrame(struct VideoBackend* v, const void* frame) {
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	struct mGLES2Context* context = (struct mGLES2Context*) v;
	glBindTexture(GL_TEXTURE_2D, context->tex);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, v->width, v->height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, v->width, v->height, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);
#endif
#elif defined(__BIG_ENDIAN__)
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, v->width, v->height, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, frame);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, v->width, v->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame);
#endif
#ifdef __APPLE__
	});
#endif
}

void mGLES2ContextCreate(struct mGLES2Context* context) {
	context->d.init = mGLES2ContextInit;
	context->d.deinit = mGLES2ContextDeinit;
	context->d.setDimensions = mGLES2ContextSetDimensions;
	context->d.resized = mGLES2ContextResized;
	context->d.swap = 0;
	context->d.clear = mGLES2ContextClear;
	context->d.postFrame = mGLES2ContextPostFrame;
	context->d.drawFrame = mGLES2ContextDrawFrame;
	context->d.setMessage = 0;
	context->d.clearMessage = 0;
	context->shaders = 0;
	context->nShaders = 0;
}

void mGLES2ShaderInit(struct mGLES2Shader* shader, const char* vs, const char* fs, int width, int height, bool integerScaling, struct mGLES2Uniform* uniforms, size_t nUniforms) {
	shader->width = width;
	shader->height = height;
	shader->integerScaling = integerScaling;
	shader->filter = false;
	shader->blend = false;
	shader->dirty = true;
	shader->uniforms = uniforms;
	shader->nUniforms = nUniforms;
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	glGenFramebuffers(1, &shader->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, shader->fbo);

	glGenTextures(1, &shader->tex);
	glBindTexture(GL_TEXTURE_2D, shader->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (shader->width > 0 && shader->height > 0) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, shader->width, shader->height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	} else {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);		
	}

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shader->tex, 0);
	shader->program = glCreateProgram();
	shader->vertexShader = glCreateShader(GL_VERTEX_SHADER);
	shader->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	const GLchar* shaderBuffer[2];
	const GLubyte* version = glGetString(GL_VERSION);
	if (strncmp((const char*) version, "OpenGL ES ", strlen("OpenGL ES ")) == 0) {
		shaderBuffer[0] = _gles2Header;
	} else if (version[0] == '2') {
		shaderBuffer[0] = _gl2Header;
	} else {
		shaderBuffer[0] = _gl32VHeader;
	}
	if (vs) {
		shaderBuffer[1] = vs;
	} else {
		shaderBuffer[1] = _nullVertexShader;
	}
	glShaderSource(shader->vertexShader, 2, shaderBuffer, 0);

	if (shaderBuffer[0] == _gl32VHeader) {
		shaderBuffer[0] = _gl32FHeader;
	}
	if (fs) {
		shaderBuffer[1] = fs;
	} else {
		shaderBuffer[1] = _nullFragmentShader;
	}
	glShaderSource(shader->fragmentShader, 2, shaderBuffer, 0);

	glAttachShader(shader->program, shader->vertexShader);
	glAttachShader(shader->program, shader->fragmentShader);
	char log[1024];
	glCompileShader(shader->fragmentShader);
	glGetShaderInfoLog(shader->fragmentShader, 1024, 0, log);
	if (log[0]) {
		mLOG(OPENGL, ERROR, "%s\n", log);
	}
	glCompileShader(shader->vertexShader);
	glGetShaderInfoLog(shader->vertexShader, 1024, 0, log);
	if (log[0]) {
		mLOG(OPENGL, ERROR, "%s\n", log);
	}
	glLinkProgram(shader->program);
	glGetProgramInfoLog(shader->program, 1024, 0, log);
	if (log[0]) {
		mLOG(OPENGL, ERROR, "%s\n", log);
	}

	shader->texLocation = glGetUniformLocation(shader->program, "tex");
	shader->texSizeLocation = glGetUniformLocation(shader->program, "texSize");
	shader->positionLocation = glGetAttribLocation(shader->program, "position");
	size_t i;
	for (i = 0; i < shader->nUniforms; ++i) {
		shader->uniforms[i].location = glGetUniformLocation(shader->program, shader->uniforms[i].name);
	}

#ifdef BUILD_GLES3
	const GLubyte* extensions = glGetString(GL_EXTENSIONS);
	if (shaderBuffer[0] == _gles2Header || version[0] >= '3' || (extensions && strstr((const char*) extensions, "_vertex_array_object") != NULL)) {
		glGenVertexArrays(1, &shader->vao);
		glBindVertexArray(shader->vao);
		glEnableVertexAttribArray(shader->positionLocation);
		glVertexAttribPointer(shader->positionLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		glBindVertexArray(0);
	} else
#endif
	{
		shader->vao = -1;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
#ifdef __APPLE__
	});
#endif
}

void mGLES2ShaderDeinit(struct mGLES2Shader* shader) {
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	glDeleteTextures(1, &shader->tex);
	glDeleteShader(shader->fragmentShader);
	glDeleteProgram(shader->program);
	glDeleteFramebuffers(1, &shader->fbo);
#ifdef BUILD_GLES3
	if (shader->vao != (GLuint) -1) {
		glDeleteVertexArrays(1, &shader->vao);
	}
#endif
#ifdef __APPLE__
	});
#endif
}

void mGLES2ShaderAttach(struct mGLES2Context* context, struct mGLES2Shader* shaders, size_t nShaders) {
#ifdef __APPLE__
	dispatch_main_reentrant(^(){
#endif
	if (context->shaders) {
		if (context->shaders == shaders && context->nShaders == nShaders) {
			return;
		}
		mGLES2ShaderDetach(context);
	}
	context->shaders = shaders;
	context->nShaders = nShaders;
	size_t i;
	for (i = 0; i < nShaders; ++i) {
		glBindFramebuffer(GL_FRAMEBUFFER, context->shaders[i].fbo);
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);

#ifdef BUILD_GLES3
		if (context->shaders[i].vao != (GLuint) -1) {
			glBindVertexArray(context->shaders[i].vao);
			glBindBuffer(GL_ARRAY_BUFFER, context->vbo);
			glEnableVertexAttribArray(context->shaders[i].positionLocation);
			glVertexAttribPointer(context->shaders[i].positionLocation, 2, GL_FLOAT, GL_FALSE, 0, NULL);
		}
#endif
	}
#ifdef BUILD_GLES3
	if (context->initialShader.vao != (GLuint) -1) {
		glBindVertexArray(0);
	}
#endif
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
#ifdef __APPLE__
	});
#endif
}

void mGLES2ShaderDetach(struct mGLES2Context* context) {
	if (!context->shaders) {
		return;
	}
	context->shaders = 0;
	context->nShaders = 0;
}

static bool _lookupIntValue(const struct Configuration* config, const char* section, const char* key, int* out) {
	const char* charValue = ConfigurationGetValue(config, section, key);
	if (!charValue) {
		return false;
	}
	char* end;
	unsigned long value = strtol(charValue, &end, 10);
	if (*end) {
		return false;
	}
	*out = value;
	return true;
}

static bool _lookupFloatValue(const struct Configuration* config, const char* section, const char* key, float* out) {
	const char* charValue = ConfigurationGetValue(config, section, key);
	if (!charValue) {
		return false;
	}
	char* end;
	float value = strtof_u(charValue, &end);
	if (*end) {
		return false;
	}
	*out = value;
	return true;
}

static bool _lookupBoolValue(const struct Configuration* config, const char* section, const char* key, GLboolean* out) {
	const char* charValue = ConfigurationGetValue(config, section, key);
	if (!charValue) {
		return false;
	}
	if (!strcmp(charValue, "true")) {
		*out = GL_TRUE;
		return true;
	}
	if (!strcmp(charValue, "false")) {
		*out = GL_FALSE;
		return true;
	}
	char* end;
	unsigned long value = strtol(charValue, &end, 10);
	if (*end) {
		return false;
	}
	*out = value;
	return true;
}

DECLARE_VECTOR(mGLES2UniformList, struct mGLES2Uniform);
DEFINE_VECTOR(mGLES2UniformList, struct mGLES2Uniform);

static void _uniformHandler(const char* sectionName, void* user) {
	struct mGLES2UniformList* uniforms = user;
	unsigned passId;
	int sentinel;
	if (sscanf(sectionName, "pass.%u.uniform.%n", &passId, &sentinel) < 1) {
		return;
	}
	struct mGLES2Uniform* u = mGLES2UniformListAppend(uniforms);
	u->name = sectionName;
}


static void _loadValue(struct Configuration* description, const char* name, GLenum type, const char* field, union mGLES2UniformValue* value) {
	char fieldName[16];
	switch (type) {
	case GL_FLOAT:
		value->f = 0;
		_lookupFloatValue(description, name, field, &value->f);
		break;
	case GL_FLOAT_VEC2:
		value->fvec2[0] = 0;
		value->fvec2[1] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec2[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec2[1]);
		break;
	case GL_FLOAT_VEC3:
		value->fvec3[0] = 0;
		value->fvec3[1] = 0;
		value->fvec3[2] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec3[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec3[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec3[2]);
		break;
	case GL_FLOAT_VEC4:
		value->fvec4[0] = 0;
		value->fvec4[1] = 0;
		value->fvec4[2] = 0;
		value->fvec4[3] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec4[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec4[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec4[2]);
		snprintf(fieldName, sizeof(fieldName), "%s[3]", field);
		_lookupFloatValue(description, name, fieldName, &value->fvec4[3]);
		break;
	case GL_FLOAT_MAT2:
		value->fmat2x2[0] = 0;
		value->fmat2x2[1] = 0;
		value->fmat2x2[2] = 0;
		value->fmat2x2[3] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat2x2[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[0,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat2x2[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat2x2[2]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat2x2[3]);
		break;
	case GL_FLOAT_MAT3:
		value->fmat3x3[0] = 0;
		value->fmat3x3[1] = 0;
		value->fmat3x3[2] = 0;
		value->fmat3x3[3] = 0;
		value->fmat3x3[4] = 0;
		value->fmat3x3[5] = 0;
		value->fmat3x3[6] = 0;
		value->fmat3x3[7] = 0;
		value->fmat3x3[8] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[0,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[0,2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[2]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[3]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[4]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[5]);
		snprintf(fieldName, sizeof(fieldName), "%s[2,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[6]);
		snprintf(fieldName, sizeof(fieldName), "%s[2,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[7]);
		snprintf(fieldName, sizeof(fieldName), "%s[2,2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat3x3[8]);
		break;
	case GL_FLOAT_MAT4:
		value->fmat4x4[0] = 0;
		value->fmat4x4[1] = 0;
		value->fmat4x4[2] = 0;
		value->fmat4x4[3] = 0;
		value->fmat4x4[4] = 0;
		value->fmat4x4[5] = 0;
		value->fmat4x4[6] = 0;
		value->fmat4x4[7] = 0;
		value->fmat4x4[8] = 0;
		value->fmat4x4[9] = 0;
		value->fmat4x4[10] = 0;
		value->fmat4x4[11] = 0;
		value->fmat4x4[12] = 0;
		value->fmat4x4[13] = 0;
		value->fmat4x4[14] = 0;
		value->fmat4x4[15] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[0,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[0,2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[2]);
		snprintf(fieldName, sizeof(fieldName), "%s[0,3]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[3]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[4]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[5]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[6]);
		snprintf(fieldName, sizeof(fieldName), "%s[1,3]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[7]);
		snprintf(fieldName, sizeof(fieldName), "%s[2,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[8]);
		snprintf(fieldName, sizeof(fieldName), "%s[2,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[9]);
		snprintf(fieldName, sizeof(fieldName), "%s[2,2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[10]);
		snprintf(fieldName, sizeof(fieldName), "%s[2,3]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[11]);
		snprintf(fieldName, sizeof(fieldName), "%s[3,0]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[12]);
		snprintf(fieldName, sizeof(fieldName), "%s[3,1]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[13]);
		snprintf(fieldName, sizeof(fieldName), "%s[3,2]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[14]);
		snprintf(fieldName, sizeof(fieldName), "%s[3,3]", field);
		_lookupFloatValue(description, name, fieldName, &value->fmat4x4[15]);
		break;
	case GL_INT:
		value->i = 0;
		_lookupIntValue(description, name, field, &value->i);
		break;
	case GL_INT_VEC2:
		value->ivec2[0] = 0;
		value->ivec2[1] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec2[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec2[1]);
		break;
	case GL_INT_VEC3:
		value->ivec3[0] = 0;
		value->ivec3[1] = 0;
		value->ivec3[2] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec3[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec3[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[2]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec3[2]);
		break;
	case GL_INT_VEC4:
		value->ivec4[0] = 0;
		value->ivec4[1] = 0;
		value->ivec4[2] = 0;
		value->ivec4[3] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec4[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec4[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[2]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec4[2]);
		snprintf(fieldName, sizeof(fieldName), "%s[3]", field);
		_lookupIntValue(description, name, fieldName, &value->ivec4[3]);
		break;
	case GL_BOOL:
		value->b = 0;
		_lookupBoolValue(description, name, field, &value->b);
		break;
	case GL_BOOL_VEC2:
		value->bvec2[0] = 0;
		value->bvec2[1] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec2[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec2[1]);
		break;
	case GL_BOOL_VEC3:
		value->bvec3[0] = 0;
		value->bvec3[1] = 0;
		value->bvec3[2] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec3[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec3[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[2]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec3[2]);
		break;
	case GL_BOOL_VEC4:
		value->bvec4[0] = 0;
		value->bvec4[1] = 0;
		value->bvec4[2] = 0;
		value->bvec4[3] = 0;
		snprintf(fieldName, sizeof(fieldName), "%s[0]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec4[0]);
		snprintf(fieldName, sizeof(fieldName), "%s[1]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec4[1]);
		snprintf(fieldName, sizeof(fieldName), "%s[2]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec4[2]);
		snprintf(fieldName, sizeof(fieldName), "%s[3]", field);
		_lookupBoolValue(description, name, fieldName, &value->bvec4[3]);
		break;
	}
}

static bool _loadUniform(struct Configuration* description, size_t pass, struct mGLES2Uniform* uniform) {
	unsigned passId;
	if (sscanf(uniform->name, "pass.%u.uniform.", &passId) < 1 || passId != pass) {
		return false;
	}
	const char* type = ConfigurationGetValue(description, uniform->name, "type");
	if (!type) {
		return false;
	}
	if (!strcmp(type, "float")) {
		uniform->type = GL_FLOAT;
	} else if (!strcmp(type, "float2")) {
		uniform->type = GL_FLOAT_VEC2;
	} else if (!strcmp(type, "float3")) {
		uniform->type = GL_FLOAT_VEC3;
	} else if (!strcmp(type, "float4")) {
		uniform->type = GL_FLOAT_VEC4;
	} else if (!strcmp(type, "float2x2")) {
		uniform->type = GL_FLOAT_MAT2;
	} else if (!strcmp(type, "float3x3")) {
		uniform->type = GL_FLOAT_MAT3;
	} else if (!strcmp(type, "float4x4")) {
		uniform->type = GL_FLOAT_MAT4;
	} else if (!strcmp(type, "int")) {
		uniform->type = GL_INT;
	} else if (!strcmp(type, "int2")) {
		uniform->type = GL_INT_VEC2;
	} else if (!strcmp(type, "int3")) {
		uniform->type = GL_INT_VEC3;
	} else if (!strcmp(type, "int4")) {
		uniform->type = GL_INT_VEC4;
	} else if (!strcmp(type, "bool")) {
		uniform->type = GL_BOOL;
	} else if (!strcmp(type, "bool2")) {
		uniform->type = GL_BOOL_VEC2;
	} else if (!strcmp(type, "bool3")) {
		uniform->type = GL_BOOL_VEC3;
	} else if (!strcmp(type, "bool4")) {
		uniform->type = GL_BOOL_VEC4;
	} else {
		return false;
	}
	_loadValue(description, uniform->name, uniform->type, "default", &uniform->value);
	_loadValue(description, uniform->name, uniform->type, "min", &uniform->min);
	_loadValue(description, uniform->name, uniform->type, "max", &uniform->max);
	const char* readable = ConfigurationGetValue(description, uniform->name, "readableName");
	if (readable) {
		uniform->readableName = strdup(readable);
	} else {
		uniform->readableName = 0;
	}
	uniform->name = strdup(strstr(uniform->name, "uniform.") + strlen("uniform."));
	return true;
}

bool mGLES2ShaderLoad(struct VideoShader* shader, struct VDir* dir) {
	struct VFile* manifest = dir->openFile(dir, "manifest.ini", O_RDONLY);
	if (!manifest) {
		return false;
	}
	bool success = false;
	struct Configuration description;
	ConfigurationInit(&description);
	if (ConfigurationReadVFile(&description, manifest)) {
		int inShaders;
		success = _lookupIntValue(&description, "shader", "passes", &inShaders);
		if (inShaders > MAX_PASSES || inShaders < 1) {
			success = false;
		}
		if (success) {
			struct mGLES2Shader* shaderBlock = calloc(inShaders, sizeof(struct mGLES2Shader));
			int n;
			for (n = 0; n < inShaders; ++n) {
				char passName[12];
				snprintf(passName, sizeof(passName), "pass.%u", n);
				const char* fs = ConfigurationGetValue(&description, passName, "fragmentShader");
				const char* vs = ConfigurationGetValue(&description, passName, "vertexShader");
				if (fs && (fs[0] == '.' || strstr(fs, PATH_SEP))) {
					success = false;
					break;
				}
				if (vs && (vs[0] == '.' || strstr(vs, PATH_SEP))) {
					success = false;
					break;
				}
				char* fssrc = 0;
				char* vssrc = 0;
				if (fs) {
					struct VFile* fsf = dir->openFile(dir, fs, O_RDONLY);
					if (!fsf) {
						success = false;
						break;
					}
					fssrc = malloc(fsf->size(fsf) + 1);
					fssrc[fsf->size(fsf)] = '\0';
					fsf->read(fsf, fssrc, fsf->size(fsf));
					fsf->close(fsf);
				}
				if (vs) {
					struct VFile* vsf = dir->openFile(dir, vs, O_RDONLY);
					if (!vsf) {
						success = false;
						free(fssrc);
						break;
					}
					vssrc = malloc(vsf->size(vsf) + 1);
					vssrc[vsf->size(vsf)] = '\0';
					vsf->read(vsf, vssrc, vsf->size(vsf));
					vsf->close(vsf);
				}
				int width = 0;
				int height = 0;
				int scaling = 0;
				_lookupIntValue(&description, passName, "width", &width);
				_lookupIntValue(&description, passName, "height", &height);
				_lookupIntValue(&description, passName, "integerScaling", &scaling);

				struct mGLES2UniformList uniformVector;
				mGLES2UniformListInit(&uniformVector, 0);
				ConfigurationEnumerateSections(&description, _uniformHandler, &uniformVector);
				size_t u;
				for (u = 0; u < mGLES2UniformListSize(&uniformVector); ++u) {
					struct mGLES2Uniform* uniform = mGLES2UniformListGetPointer(&uniformVector, u);
					if (!_loadUniform(&description, n, uniform)) {
						mGLES2UniformListShift(&uniformVector, u, 1);
						--u;
					}
				}
				u = mGLES2UniformListSize(&uniformVector);
				struct mGLES2Uniform* uniformBlock = calloc(u, sizeof(*uniformBlock));
				memcpy(uniformBlock, mGLES2UniformListGetPointer(&uniformVector, 0), sizeof(*uniformBlock) * u);
				mGLES2UniformListDeinit(&uniformVector);

				mGLES2ShaderInit(&shaderBlock[n], vssrc, fssrc, width, height, scaling, uniformBlock, u);
				int b = 0;
				_lookupIntValue(&description, passName, "blend", &b);
				if (b) {
					shaderBlock[n].blend = b;
				}
				b = 0;
				_lookupIntValue(&description, passName, "filter", &b);
				if (b) {
					shaderBlock[n].filter = b;
				}
				free(fssrc);
				free(vssrc);
			}
			if (success) {
				shader->nPasses = inShaders;
				shader->passes = shaderBlock;
				shader->name = ConfigurationGetValue(&description, "shader", "name");
				if (shader->name) {
					shader->name = strdup(shader->name);
				}
				shader->author = ConfigurationGetValue(&description, "shader", "author");
				if (shader->author) {
					shader->author = strdup(shader->author);
				}
				shader->description = ConfigurationGetValue(&description, "shader", "description");
				if (shader->description) {
					shader->description = strdup(shader->description);
				}
			} else {
				inShaders = n;
				for (n = 0; n < inShaders; ++n) {
					mGLES2ShaderDeinit(&shaderBlock[n]);
				}
			}
		}
	}
	manifest->close(manifest);
	ConfigurationDeinit(&description);
	return success;
}

void mGLES2ShaderFree(struct VideoShader* shader) {
	free((void*) shader->name);
	free((void*) shader->author);
	free((void*) shader->description);
	shader->name = 0;
	shader->author = 0;
	shader->description = 0;
	struct mGLES2Shader* shaders = shader->passes;
	size_t n;
	for (n = 0; n < shader->nPasses; ++n) {
		mGLES2ShaderDeinit(&shaders[n]);
		size_t u;
		for (u = 0; u < shaders[n].nUniforms; ++u) {
			free((void*) shaders[n].uniforms[u].name);
			free((void*) shaders[n].uniforms[u].readableName);
		}
	}
	free(shaders);
	shader->passes = 0;
	shader->nPasses = 0;
}
