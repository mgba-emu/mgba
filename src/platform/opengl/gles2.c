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

	glShaderSource(context->fragmentShader, 1, (const GLchar**) &_fragmentShader, 0);
	glShaderSource(context->vertexShader, 1, (const GLchar**) &_vertexShader, 0);
	glAttachShader(context->program, context->vertexShader);
	glAttachShader(context->program, context->fragmentShader);
	char log[1024];
	glCompileShader(context->fragmentShader);
	glCompileShader(context->vertexShader);
	glGetShaderInfoLog(context->fragmentShader, 1024, 0, log);
	glGetShaderInfoLog(context->vertexShader, 1024, 0, log);
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
	glUseProgram(context->program);
	glUniform1i(context->texLocation, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, context->tex);
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
}
