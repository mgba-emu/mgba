/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gles2.h"

#include "gba/video.h"

static const char* const _vertexShader =
	"attribute vec4 position;\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	gl_Position = position;\n"
	"	texCoord = (position.st + vec2(1.0, -1.0)) * vec2(0.46875, -0.3125);\n"
	"}";

static const char* const _nullVertexShader =
	"attribute vec4 position;\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	gl_Position = position * vec4(1.0, 1.0, 1.0, 1.0);\n"
	"	texCoord = (position.st + vec2(1.0, 1.0)) * vec2(0.5, 0.5);\n"
	"}";

static const char* const _fragmentShader =
	"varying vec2 texCoord;\n"
	"uniform sampler2D tex;\n"

	"void main() {\n"
	"	vec4 color = texture2D(tex, texCoord);\n"
	"	color.a = 1.;\n"
	"	gl_FragColor = color;"
	"}";

static const GLfloat _vertices[] = {
	-1.f, -1.f,
	-1.f, 1.f,
	1.f, 1.f,
	1.f, -1.f,
};

static void GBAGLES2ContextInit(struct VideoBackend* v, WHandle handle) {
	UNUSED(handle);
	struct GBAGLES2Context* context = (struct GBAGLES2Context*) v;
	glGenTextures(1, &context->tex);
	glBindTexture(GL_TEXTURE_2D, context->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, 0);
#endif
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#endif

	context->program = glCreateProgram();
	context->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	context->vertexShader = glCreateShader(GL_VERTEX_SHADER);
	context->nullVertexShader = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(context->fragmentShader, 1, (const GLchar**) &_fragmentShader, 0);
	glShaderSource(context->vertexShader, 1, (const GLchar**) &_vertexShader, 0);
	glShaderSource(context->nullVertexShader, 1, (const GLchar**) &_nullVertexShader, 0);
	glAttachShader(context->program, context->vertexShader);
	glAttachShader(context->program, context->fragmentShader);
	char log[1024];
	glCompileShader(context->fragmentShader);
	glCompileShader(context->vertexShader);
	glCompileShader(context->nullVertexShader);
	glGetShaderInfoLog(context->fragmentShader, 1024, 0, log);
	printf("%s\n", log);
	glGetShaderInfoLog(context->vertexShader, 1024, 0, log);
	printf("%s\n", log);
	glGetShaderInfoLog(context->nullVertexShader, 1024, 0, log);
	printf("%s\n", log);
	glLinkProgram(context->program);
	glGetProgramInfoLog(context->program, 1024, 0, log);
	printf("%s\n", log);
	context->texLocation = glGetUniformLocation(context->program, "tex");
	context->positionLocation = glGetAttribLocation(context->program, "position");
	glClearColor(0.f, 0.f, 0.f, 1.f);
}

static void GBAGLES2ContextDeinit(struct VideoBackend* v) {
	struct GBAGLES2Context* context = (struct GBAGLES2Context*) v;
	glDeleteTextures(1, &context->tex);
	glDeleteShader(context->fragmentShader);
	glDeleteShader(context->vertexShader);
	glDeleteShader(context->nullVertexShader);
	glDeleteProgram(context->program);
}

static void GBAGLES2ContextResized(struct VideoBackend* v, int w, int h) {
	int drawW = w;
	int drawH = h;
	if (v->lockAspectRatio) {
		if (w * 2 > h * 3) {
			drawW = h * 3 / 2;
		} else if (w * 2 < h * 3) {
			drawH = w * 2 / 3;
		}
	}
	glViewport(0, 0, 240, 160);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport((w - drawW) / 2, (h - drawH) / 2, drawW, drawH);
}

static void GBAGLES2ContextClear(struct VideoBackend* v) {
	UNUSED(v);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void GBAGLES2ContextDrawFrame(struct VideoBackend* v) {
	struct GBAGLES2Context* context = (struct GBAGLES2Context*) v;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, context->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, v->filter ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, v->filter ? GL_LINEAR : GL_NEAREST);

	if (context->shader) {
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);
		glViewport(0, 0, context->shader->width, context->shader->height);
		glBindFramebuffer(GL_FRAMEBUFFER, context->shader->fbo);
		glUseProgram(context->shader->program);
		glUniform1i(context->shader->texLocation, 0);
		glVertexAttribPointer(context->shader->positionLocation, 2, GL_FLOAT, GL_FALSE, 0, _vertices);
		glEnableVertexAttribArray(context->shader->positionLocation);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, context->shader->tex);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
	}
	glUseProgram(context->program);
	glUniform1i(context->texLocation, 0);
	glVertexAttribPointer(context->positionLocation, 2, GL_FLOAT, GL_FALSE, 0, _vertices);
	glEnableVertexAttribArray(context->positionLocation);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glUseProgram(0);
}

void GBAGLES2ContextPostFrame(struct VideoBackend* v, const void* frame) {
	struct GBAGLES2Context* context = (struct GBAGLES2Context*) v;
	glBindTexture(GL_TEXTURE_2D, context->tex);
#ifdef COLOR_16_BIT
#ifdef COLOR_5_6_5
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame);
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, frame);
#endif
#else
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame);
#endif
}

void GBAGLES2ContextCreate(struct GBAGLES2Context* context) {
	context->d.init = GBAGLES2ContextInit;
	context->d.deinit = GBAGLES2ContextDeinit;
	context->d.resized = GBAGLES2ContextResized;
	context->d.swap = 0;
	context->d.clear = GBAGLES2ContextClear;
	context->d.postFrame = GBAGLES2ContextPostFrame;
	context->d.drawFrame = GBAGLES2ContextDrawFrame;
	context->d.setMessage = 0;
	context->d.clearMessage = 0;
	context->shader = 0;
}

void GBAGLES2ShaderInit(struct GBAGLES2Shader* shader, const char* src, int width, int height, bool filter) {
	shader->width = width > 0 ? width : 256;
	shader->height = height > 0 ? height : 256;
	glGenFramebuffers(1, &shader->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, shader->fbo);

	glGenTextures(1, &shader->tex);
	glBindTexture(GL_TEXTURE_2D, shader->tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, shader->width, shader->height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shader->tex, 0);
	shader->program = glCreateProgram();
	shader->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(shader->fragmentShader, 1, (const GLchar**) &src, 0);
	glAttachShader(shader->program, shader->fragmentShader);
	glCompileShader(shader->fragmentShader);
	char log[1024];
	glGetShaderInfoLog(shader->fragmentShader, 1024, 0, log);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAGLES2ShaderDeinit(struct GBAGLES2Shader* shader) {
	glDeleteTextures(1, &shader->tex);
	glDeleteShader(shader->fragmentShader);
	glDeleteProgram(shader->program);
}

void GBAGLES2ShaderAttach(struct GBAGLES2Context* context, struct GBAGLES2Shader* shader) {
	if (context->shader) {
		if (context->shader == shader) {
			return;
		}
		GBAGLES2ShaderDetach(context);
	}
	context->shader = shader;
	glAttachShader(shader->program, context->nullVertexShader);

	char log[1024];
	glLinkProgram(shader->program);
	glGetProgramInfoLog(shader->program, 1024, 0, log);
	printf("%s\n", log);
	shader->texLocation = glGetUniformLocation(shader->program, "tex");
	shader->positionLocation = glGetAttribLocation(shader->program, "position");
}

void GBAGLES2ShaderDetach(struct GBAGLES2Context* context) {
	if (!context->shader) {
		return;
	}
	glDetachShader(context->shader->program, context->nullVertexShader);
	context->shader = 0;
}
