/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/renderers/gl.h>

#include <mgba/core/cache-set.h>
#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/cache-set.h>

static void GBAVideoGLRendererInit(struct GBAVideoRenderer* renderer);
static void GBAVideoGLRendererDeinit(struct GBAVideoRenderer* renderer);
static void GBAVideoGLRendererReset(struct GBAVideoRenderer* renderer);
static void GBAVideoGLRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address);
static void GBAVideoGLRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam);
static void GBAVideoGLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static uint16_t GBAVideoGLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value);
static void GBAVideoGLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y);
static void GBAVideoGLRendererFinishFrame(struct GBAVideoRenderer* renderer);
static void GBAVideoGLRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels);
static void GBAVideoGLRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels);

static const GLchar* const _gl3Header =
	"#version 120\n";

static const char* const _vertexShader =
	"attribute vec4 position;\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	gl_Position = position;\n"
	"	texCoord = (position.st + vec2(1.0, -1.0)) * vec2(0.5, 0.5);\n"
	"}";

static const char* const _fragmentShader =
	"varying vec2 texCoord;\n"
	"uniform sampler2D tex;\n"

	"void main() {\n"
	"	vec4 color = texture2D(tex, texCoord);\n"
	"	color.a = 1.;\n"
	"	gl_FragColor = color;\n"
	"}";

static const GLfloat _vertices[] = {
	-1.f, -1.f,
	-1.f, 1.f,
	1.f, 1.f,
	1.f, -1.f,
};

void GBAVideoGLRendererCreate(struct GBAVideoGLRenderer* renderer) {
	renderer->d.init = GBAVideoGLRendererInit;
	renderer->d.reset = GBAVideoGLRendererReset;
	renderer->d.deinit = GBAVideoGLRendererDeinit;
	renderer->d.writeVideoRegister = GBAVideoGLRendererWriteVideoRegister;
	renderer->d.writeVRAM = GBAVideoGLRendererWriteVRAM;
	renderer->d.writeOAM = GBAVideoGLRendererWriteOAM;
	renderer->d.writePalette = GBAVideoGLRendererWritePalette;
	renderer->d.drawScanline = GBAVideoGLRendererDrawScanline;
	renderer->d.finishFrame = GBAVideoGLRendererFinishFrame;
	renderer->d.getPixels = GBAVideoGLRendererGetPixels;
	renderer->d.putPixels = GBAVideoGLRendererPutPixels;

	renderer->d.disableBG[0] = false;
	renderer->d.disableBG[1] = false;
	renderer->d.disableBG[2] = false;
	renderer->d.disableBG[3] = false;
	renderer->d.disableOBJ = false;
}

void GBAVideoGLRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glGenFramebuffers(6, glRenderer->fbo);
	glGenTextures(6, glRenderer->layers);

	glGenTextures(1, &glRenderer->paletteTex);

	glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	GBAVideoGLRendererReset(renderer);
}

void GBAVideoGLRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glDeleteFramebuffers(6, glRenderer->fbo);
	glDeleteTextures(6, glRenderer->layers);
	glDeleteTextures(1, &glRenderer->paletteTex);
}

void GBAVideoGLRendererReset(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[5]);
	glBindTexture(GL_TEXTURE_2D, glRenderer->layers[5]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glRenderer->layers[5], 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glRenderer->compositeProgram = glCreateProgram();
	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

	const GLchar* shaderBuffer[2];
	const GLubyte* version = glGetString(GL_VERSION);
	shaderBuffer[0] = _gl3Header;
	shaderBuffer[1] = _vertexShader;
	glShaderSource(vs, 2, shaderBuffer, 0);
	shaderBuffer[1] = _fragmentShader;
	glShaderSource(fs, 2, shaderBuffer, 0);

	glAttachShader(glRenderer->compositeProgram, vs);
	glAttachShader(glRenderer->compositeProgram, fs);
	char log[1024];

	glCompileShader(fs);
	glGetShaderInfoLog(fs, 1024, 0, log);

	glCompileShader(vs);
	glGetShaderInfoLog(vs, 1024, 0, log);

	glLinkProgram(glRenderer->compositeProgram);
	glGetProgramInfoLog(glRenderer->compositeProgram, 1024, 0, log);

	glRenderer->paletteDirty = false;
}

void GBAVideoGLRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {

}

void GBAVideoGLRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {

}

void GBAVideoGLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->paletteDirty = true;
}

uint16_t GBAVideoGLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {

}

void GBAVideoGLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (glRenderer->paletteDirty) {
		glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 256, 2, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, glRenderer->d.palette);
		glRenderer->paletteDirty = false;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[5]);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glUseProgram(glRenderer->compositeProgram);
	glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
	glUniform1i(0, 0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, _vertices);
	glEnableVertexAttribArray(0);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAVideoGLRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glFinish();
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[5]);
	glPixelStorei(GL_PACK_ROW_LENGTH, glRenderer->outputBufferStride);
	glReadPixels(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS, GL_RGBA, GL_UNSIGNED_BYTE, glRenderer->outputBuffer);
	glClearColor(1.f, 1.f, 0.f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAVideoGLRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {

}

void GBAVideoGLRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels) {

}

