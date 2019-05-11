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

static void GBAVideoGLRendererUpdateDISPCNT(struct GBAVideoGLRenderer* renderer);
static void GBAVideoGLRendererWriteBGCNT(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBGX_LO(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBGX_HI(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBGY_LO(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBGY_HI(struct GBAVideoGLBackground* bg, uint16_t value);
static void GBAVideoGLRendererWriteBLDCNT(struct GBAVideoGLRenderer* renderer, uint16_t value);

static void GBAVideoGLRendererDrawBackgroundMode0(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawBackgroundMode2(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawBackgroundMode3(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawBackgroundMode4(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawBackgroundMode5(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);

#define TEST_LAYER_ENABLED(X) !renderer->disableBG[X] && glRenderer->bg[X].enabled == 4 && glRenderer->bg[X].priority == priority

static const GLchar* const _gl3Header =
	"#version 130\n";

static const char* const _vertexShader =
	"attribute vec2 position;\n"
	"uniform ivec2 loc;\n"
	"const ivec2 maxPos = ivec2(240, 160);\n"
	"varying vec2 texCoord;\n"

	"void main() {\n"
	"	vec2 local = (position * loc.x + vec2(0, loc.y)) / vec2(1., maxPos.y);\n"
	"	gl_Position = vec4(local * 2. - 1., 0., 1.);\n"
	"	texCoord = local * maxPos.xy;\n"
	"}";

static const char* const _renderTile16 =
	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord) {\n"
	"	int address = charBase + tile * 16 + (localCoord.x >> 2) + (localCoord.y << 1);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - (localCoord.x & 3)] * 15.9);\n"
	"	vec4 color = texelFetch(palette, ivec2(entry, paletteId), 0);\n"
	"	if (entry == 0) {\n"
	"		color.a = 0;\n"
	"	} else {\n"
	"		color.a = 1;\n"
	"	}\n"
	"	return color;\n"
	"}";

static const char* const _renderTile256 =
	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord) {\n"
	"	int address = charBase + tile * 32 + (localCoord.x >> 1) + (localCoord.y << 2);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - 2 * (localCoord.x & 1)] * 15.9);\n"
	"	int pal2 = int(halfrow[2 - 2 * (localCoord.x & 1)] * 15.9);\n"
	"	vec4 color = texelFetch(palette, ivec2(entry, pal2 + (paletteId & 16)), 0);\n"
	"	if (pal2 > 0 || entry > 0) {\n"
	"		color.a = 1.;\n"
	"	} else {\n"
	"		color.a = 0.;\n"
	"	}\n"
	"	return color;\n"
	"}";

static const char* const _renderMode0 =
	"varying vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int screenBase;\n"
	"uniform int charBase;\n"
	"uniform int size;\n"
	"uniform ivec2 offset;\n"

	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord);\n"

	"void main() {\n"
	"	ivec2 coord = ivec2(texCoord) + offset;\n"
	"	if ((size & 1) == 1) {\n"
	"		coord.y += coord.x & 256;\n"
	"	}\n"
	"	coord.x &= 255;\n"
	"	int mapAddress = screenBase + (coord.x >> 3) + (coord.y >> 3) * 32;\n"
	"	vec4 map = texelFetch(vram, ivec2(mapAddress & 255, mapAddress >> 8), 0);\n"
	"	int flags = int(map.g * 15.9);\n"
	"	if ((flags & 4) == 4) {\n"
	"		coord.x ^= 7;\n"
	"	}\n"
	"	if ((flags & 8) == 8) {\n"
	"		coord.y ^= 7;\n"
	"	}\n"
	"	int tile = int(map.a * 15.9) + int(map.b * 15.9) * 16 + (flags & 0x3) * 256;\n"
	"	gl_FragColor = renderTile(tile, int(map.r * 15.9), coord & 7);\n"
	"}";

static const char* const _fetchTileOverflow =
	"vec4 fetchTile(ivec2 coord) {\n"
	"	int sizeAdjusted = (0x8000 << size) - 1;\n"
	"	coord &= sizeAdjusted;\n"
	"	return renderTile(coord);\n"
	"}";

static const char* const _fetchTileNoOverflow =
	"vec4 fetchTile(ivec2 coord) {\n"
	"	int sizeAdjusted = (0x8000 << size) - 1;\n"
	"	ivec2 outerCoord = coord & ~sizeAdjusted;\n"
	"	if ((outerCoord.x | outerCoord.y) != 0) {\n"
	"		vec4 color = texelFetch(palette, ivec2(0, 0), 0);\n"
	"		color.a = 0;\n"
	"		return color;\n"
	"	}\n"
	"	return renderTile(coord);\n"
	"}";

static const char* const _renderMode2 =
	"varying vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int screenBase;\n"
	"uniform int charBase;\n"
	"uniform int size;\n"
	"uniform ivec2[4] offset;\n"
	"uniform ivec2[4] transform;\n"
	"precision highp float;\n"
	"precision highp int;\n"

	"vec4 fetchTile(ivec2 coord);\n"

	"vec4 renderTile(ivec2 coord) {\n"
	"	int map = (coord.x >> 11) + (((coord.y >> 7) & 0x7F0) << size);\n"
	"	int mapAddress = screenBase + (map >> 1);\n"
	"	vec4 twomaps = texelFetch(vram, ivec2(mapAddress & 255, mapAddress >> 8), 0);\n"
	"	int tile = int(twomaps[3 - 2 * (map & 1)] * 15.9) + int(twomaps[2 - 2 * (map & 1)] * 15.9) * 16;\n"
	"	int address = charBase + tile * 32 + ((coord.x >> 9) & 3) + ((coord.y >> 6) & 0x1C);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - ((coord.x >> 7) & 2)] * 15.9);\n"
	"	int pal2 = int(halfrow[2 - ((coord.x >> 7) & 2)] * 15.9);\n"
	"	vec4 color = texelFetch(palette, ivec2(entry, pal2), 0);\n"
	"	if (pal2 > 0 || entry > 0) {\n"
	"		color.a = 1.;\n"
	"	} else {\n"
	"		color.a = 0.;\n"
	"	}\n"
	"	return color;\n"
	"}\n"

	"vec2 interpolate(ivec2 arr[4], float x) {\n"
	"	float x1m = 1. - x;\n"
	"	return x1m * x1m * x1m * arr[0] +"
		"  3 * x1m * x1m * x   * arr[1] +"
		"  3 * x1m * x   * x   * arr[2] +"
		"      x   * x   * x   * arr[3];\n"
	"}\n"

	"void main() {\n"
	"	float y = fract(texCoord.y);\n"
	"	float lin = 0.5 - y / ceil(y) * 0.25;\n"
	"	vec2 mixedTransform = interpolate(transform, lin);\n"
	"	vec2 mixedOffset = interpolate(offset, lin);\n"
	"	gl_FragColor = fetchTile(ivec2(mixedTransform * texCoord.x + mixedOffset));\n"
	"}";

static const char* const _composite =
	"varying vec2 texCoord;\n"
	"uniform ivec3 inflags;\n"
	"uniform int scale;\n"
	"uniform vec3 blend;\n"
	"uniform sampler2D layer;\n"
	"uniform sampler2D oldLayer;\n"
	"uniform sampler2D buffer;\n"
	"out vec4 color;\n"
	"out vec3 flags;\n"

	"void main() {\n"
	"	vec4 pix = texelFetch(layer, ivec2(texCoord * scale), 0);\n"
	"	if (pix.a == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	ivec3 oldFlags = ivec3(texelFetch(buffer, ivec2(texCoord * scale), 0).xyz * vec3(32., 4., 1.));\n"
	"	ivec3 outflags = ivec3(0, 0, 0);\n"
	"	if (inflags.x < oldFlags.x) {\n"
	"		outflags = inflags;\n"
	"		if (inflags.z == 1 && (inflags.y & 1) == 1 && (oldFlags.y & 2) == 2) {\n"
	"			vec4 oldpix = texelFetch(oldLayer, ivec2(texCoord * scale), 0);\n"
	"			pix *= blend.x;\n"
	"			pix += oldpix * blend.y;\n"
	"		}\n"
	"	} else {\n"
	"		pix = texelFetch(oldLayer, ivec2(texCoord * scale), 0);\n"
	"	}\n"
	"	color = pix;\n"
	"	flags = outflags / vec3(32., 4., 1.);\n"
	"}";

static const GLint _vertices[] = {
	0, 0,
	0, 1,
	1, 1,
	1, 0,
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

	renderer->scale = 1;
}

void _compileBackground(struct GBAVideoGLRenderer* glRenderer, GLuint program, const char** shaderBuffer, int shaderBufferLines, GLuint vs, char* log) {
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glShaderSource(fs, shaderBufferLines, shaderBuffer, 0);
	glCompileShader(fs);
	glGetShaderInfoLog(fs, 1024, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Fragment shader compilation failure: %s", log);
	}
	glLinkProgram(program);
	glGetProgramInfoLog(program, 1024, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Program link failure: %s", log);
	}
	glDeleteShader(fs);
}

void GBAVideoGLRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glGenFramebuffers(2, glRenderer->fbo);
	glGenTextures(3, glRenderer->layers);

	glGenTextures(1, &glRenderer->paletteTex);
	glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenTextures(1, &glRenderer->vramTex);
	glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0);

	glGenTextures(1, &glRenderer->oamTex);
	glBindTexture(GL_TEXTURE_2D, glRenderer->oamTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[1]);
	glBindTexture(GL_TEXTURE_2D, glRenderer->outputTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GBA_VIDEO_HORIZONTAL_PIXELS * glRenderer->scale, GBA_VIDEO_VERTICAL_PIXELS * glRenderer->scale, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glRenderer->outputTex, 0);

	glBindTexture(GL_TEXTURE_2D, glRenderer->layers[2]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GBA_VIDEO_HORIZONTAL_PIXELS * glRenderer->scale, GBA_VIDEO_VERTICAL_PIXELS * glRenderer->scale, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, glRenderer->layers[2], 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	int i;
	for (i = 0; i < 4; ++i) {
		glRenderer->bg[i].index = i;
		glGenFramebuffers(1, &glRenderer->bg[i].fbo);
		glGenTextures(1, &glRenderer->bg[i].tex);
		glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->bg[i].fbo);
		glBindTexture(GL_TEXTURE_2D, glRenderer->bg[i].tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GBA_VIDEO_HORIZONTAL_PIXELS * glRenderer->scale, GBA_VIDEO_VERTICAL_PIXELS * glRenderer->scale, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glRenderer->bg[i].tex, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	glRenderer->compositeProgram = glCreateProgram();
	glRenderer->objProgram = glCreateProgram();
	glRenderer->bgProgram[0] = glCreateProgram();
	glRenderer->bgProgram[1] = glCreateProgram();
	glRenderer->bgProgram[2] = glCreateProgram();
	glRenderer->bgProgram[3] = glCreateProgram();
	glRenderer->bgProgram[4] = glCreateProgram();
	glRenderer->bgProgram[5] = glCreateProgram();

	char log[1024];
	const GLchar* shaderBuffer[8];
	shaderBuffer[0] = _gl3Header;

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	shaderBuffer[1] = _vertexShader;
	glShaderSource(vs, 2, shaderBuffer, 0);
	glCompileShader(vs);
	glGetShaderInfoLog(vs, 1024, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Vertex shader compilation failure: %s", log);
	}

	shaderBuffer[1] = _renderMode0;

	shaderBuffer[2] = _renderTile16;
	_compileBackground(glRenderer, glRenderer->bgProgram[0], shaderBuffer, 3, vs, log);

	shaderBuffer[2] = _renderTile256;
	_compileBackground(glRenderer, glRenderer->bgProgram[1], shaderBuffer, 3, vs, log);

	shaderBuffer[1] = _renderMode2;

	shaderBuffer[2] = _fetchTileOverflow;
	_compileBackground(glRenderer, glRenderer->bgProgram[2], shaderBuffer, 3, vs, log);

	shaderBuffer[2] = _fetchTileNoOverflow;
	_compileBackground(glRenderer, glRenderer->bgProgram[3], shaderBuffer, 3, vs, log);

	shaderBuffer[1] = _composite;
	_compileBackground(glRenderer, glRenderer->compositeProgram, shaderBuffer, 2, vs, log);
	glBindFragDataLocation(glRenderer->compositeProgram, 0, "color");
	glBindFragDataLocation(glRenderer->compositeProgram, 1, "flags");

	glDeleteShader(vs);

	GBAVideoGLRendererReset(renderer);
}

void GBAVideoGLRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glDeleteFramebuffers(2, glRenderer->fbo);
	glDeleteTextures(3, glRenderer->layers);
	glDeleteTextures(1, &glRenderer->paletteTex);
	glDeleteTextures(1, &glRenderer->vramTex);
	glDeleteTextures(1, &glRenderer->oamTex);
}

void GBAVideoGLRendererReset(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;

	glRenderer->paletteDirty = true;
	glRenderer->vramDirty = 0xFFFFFF;
	glRenderer->firstAffine = -1;
}

void GBAVideoGLRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->vramDirty |= 1 << (address >> 12);
}

void GBAVideoGLRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	UNUSED(oam);
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->oamDirty = true;
}

void GBAVideoGLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	UNUSED(address);
	UNUSED(value);
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->paletteDirty = true;
}

uint16_t GBAVideoGLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (renderer->cache) {
		GBAVideoCacheWriteVideoRegister(renderer->cache, address, value);
	}

	switch (address) {
	case REG_DISPCNT:
		value &= 0xFFF7;
		glRenderer->dispcnt = value;
		GBAVideoGLRendererUpdateDISPCNT(glRenderer);
		break;
	case REG_BG0CNT:
		value &= 0xDFFF;
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[0], value);
		break;
	case REG_BG1CNT:
		value &= 0xDFFF;
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[1], value);
		break;
	case REG_BG2CNT:
		value &= 0xFFFF;
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[2], value);
		break;
	case REG_BG3CNT:
		value &= 0xFFFF;
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[3], value);
		break;
	case REG_BG0HOFS:
		value &= 0x01FF;
		glRenderer->bg[0].x = value;
		break;
	case REG_BG0VOFS:
		value &= 0x01FF;
		glRenderer->bg[0].y = value;
		break;
	case REG_BG1HOFS:
		value &= 0x01FF;
		glRenderer->bg[1].x = value;
		break;
	case REG_BG1VOFS:
		value &= 0x01FF;
		glRenderer->bg[1].y = value;
		break;
	case REG_BG2HOFS:
		value &= 0x01FF;
		glRenderer->bg[2].x = value;
		break;
	case REG_BG2VOFS:
		value &= 0x01FF;
		glRenderer->bg[2].y = value;
		break;
	case REG_BG3HOFS:
		value &= 0x01FF;
		glRenderer->bg[3].x = value;
		break;
	case REG_BG3VOFS:
		value &= 0x01FF;
		glRenderer->bg[3].y = value;
		break;
	case REG_BG2PA:
		glRenderer->bg[2].affine[0].dx = value;
		break;
	case REG_BG2PB:
		glRenderer->bg[2].affine[0].dmx = value;
		break;
	case REG_BG2PC:
		glRenderer->bg[2].affine[0].dy = value;
		break;
	case REG_BG2PD:
		glRenderer->bg[2].affine[0].dmy = value;
		break;
	case REG_BG2X_LO:
		GBAVideoGLRendererWriteBGX_LO(&glRenderer->bg[2], value);
		break;
	case REG_BG2X_HI:
		GBAVideoGLRendererWriteBGX_HI(&glRenderer->bg[2], value);
		break;
	case REG_BG2Y_LO:
		GBAVideoGLRendererWriteBGY_LO(&glRenderer->bg[2], value);
		break;
	case REG_BG2Y_HI:
		GBAVideoGLRendererWriteBGY_HI(&glRenderer->bg[2], value);
		break;
	case REG_BG3PA:
		glRenderer->bg[3].affine[0].dx = value;
		break;
	case REG_BG3PB:
		glRenderer->bg[3].affine[0].dmx = value;
		break;
	case REG_BG3PC:
		glRenderer->bg[3].affine[0].dy = value;
		break;
	case REG_BG3PD:
		glRenderer->bg[3].affine[0].dmy = value;
		break;
	case REG_BG3X_LO:
		GBAVideoGLRendererWriteBGX_LO(&glRenderer->bg[3], value);
		break;
	case REG_BG3X_HI:
		GBAVideoGLRendererWriteBGX_HI(&glRenderer->bg[3], value);
		break;
	case REG_BG3Y_LO:
		GBAVideoGLRendererWriteBGY_LO(&glRenderer->bg[3], value);
		break;
	case REG_BG3Y_HI:
		GBAVideoGLRendererWriteBGY_HI(&glRenderer->bg[3], value);
		break;
	case REG_BLDCNT:
		GBAVideoGLRendererWriteBLDCNT(glRenderer, value);
		value &= 0x3FFF;
		break;
	case REG_BLDALPHA:
		glRenderer->blda = value & 0x1F;
		if (glRenderer->blda > 0x10) {
			glRenderer->blda = 0x10;
		}
		glRenderer->bldb = (value >> 8) & 0x1F;
		if (glRenderer->bldb > 0x10) {
			glRenderer->bldb = 0x10;
		}
		value &= 0x1F1F;
		break;
	case REG_BLDY:
		value &= 0x1F;
		if (value > 0x10) {
			value = 0x10;
		}
		if (glRenderer->bldy != value) {
			glRenderer->bldy = value;
		}
		break;
	case REG_WIN0H:
		/*glRenderer->winN[0].h.end = value;
		glRenderer->winN[0].h.start = value >> 8;
		if (glRenderer->winN[0].h.start > GBA_VIDEO_HORIZONTAL_PIXELS && glRenderer->winN[0].h.start > glRenderer->winN[0].h.end) {
			glRenderer->winN[0].h.start = 0;
		}
		if (glRenderer->winN[0].h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
			glRenderer->winN[0].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
			if (glRenderer->winN[0].h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
				glRenderer->winN[0].h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
			}
		}*/
		break;
	case REG_WIN1H:
		/*glRenderer->winN[1].h.end = value;
		glRenderer->winN[1].h.start = value >> 8;
		if (glRenderer->winN[1].h.start > GBA_VIDEO_HORIZONTAL_PIXELS && glRenderer->winN[1].h.start > glRenderer->winN[1].h.end) {
			glRenderer->winN[1].h.start = 0;
		}
		if (glRenderer->winN[1].h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
			glRenderer->winN[1].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
			if (glRenderer->winN[1].h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
				glRenderer->winN[1].h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
			}
		}*/
		break;
	case REG_WIN0V:
		/*glRenderer->winN[0].v.end = value;
		glRenderer->winN[0].v.start = value >> 8;
		if (glRenderer->winN[0].v.start > GBA_VIDEO_VERTICAL_PIXELS && glRenderer->winN[0].v.start > glRenderer->winN[0].v.end) {
			glRenderer->winN[0].v.start = 0;
		}
		if (glRenderer->winN[0].v.end > GBA_VIDEO_VERTICAL_PIXELS) {
			glRenderer->winN[0].v.end = GBA_VIDEO_VERTICAL_PIXELS;
			if (glRenderer->winN[0].v.start > GBA_VIDEO_VERTICAL_PIXELS) {
				glRenderer->winN[0].v.start = GBA_VIDEO_VERTICAL_PIXELS;
			}
		}*/
		break;
	case REG_WIN1V:
		/*glRenderer->winN[1].v.end = value;
		glRenderer->winN[1].v.start = value >> 8;
		if (glRenderer->winN[1].v.start > GBA_VIDEO_VERTICAL_PIXELS && glRenderer->winN[1].v.start > glRenderer->winN[1].v.end) {
			glRenderer->winN[1].v.start = 0;
		}
		if (glRenderer->winN[1].v.end > GBA_VIDEO_VERTICAL_PIXELS) {
			glRenderer->winN[1].v.end = GBA_VIDEO_VERTICAL_PIXELS;
			if (glRenderer->winN[1].v.start > GBA_VIDEO_VERTICAL_PIXELS) {
				glRenderer->winN[1].v.start = GBA_VIDEO_VERTICAL_PIXELS;
			}
		}*/
		break;
	case REG_WININ:
		value &= 0x3F3F;
		//glRenderer->winN[0].control.packed = value;
		//glRenderer->winN[1].control.packed = value >> 8;
		break;
	case REG_WINOUT:
		value &= 0x3F3F;
		//glRenderer->winout.packed = value;
		//glRenderer->objwin.packed = value >> 8;
		break;
	case REG_MOSAIC:
		glRenderer->mosaic = value;
		break;
	case REG_GREENSWP:
		mLOG(GBA_VIDEO, STUB, "Stub video register write: 0x%03X", address);
		break;
	default:
		mLOG(GBA_VIDEO, GAME_ERROR, "Invalid video register: 0x%03X", address);
	}
	return value;
}

void GBAVideoGLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (glRenderer->paletteDirty) {
		glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 16, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, glRenderer->d.palette);
		glRenderer->paletteDirty = false;
	}
	if (glRenderer->oamDirty) {
		glBindTexture(GL_TEXTURE_2D, glRenderer->oamTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, 4, 128, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, glRenderer->d.oam);
		glRenderer->oamDirty = false;
	}
	int i;
	for (i = 0; i < 24; ++i) {
		if (!(glRenderer->vramDirty & (1 << i))) {
			continue;
		}
		// TODO: PBOs
		glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 8 * i, 256, 8, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, &glRenderer->d.vram[2048 * i]);
	}
	glRenderer->vramDirty = 0;

	uint32_t backdrop = M_RGB5_TO_RGB8(renderer->palette[0]);
	glClearColor(((backdrop >> 16) & 0xFF) / 256., ((backdrop >> 8) & 0xFF) / 256., (backdrop & 0xFF) / 256., 0.f);
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[1]);
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, y * glRenderer->scale, GBA_VIDEO_HORIZONTAL_PIXELS * glRenderer->scale, glRenderer->scale);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
	if (y == 0) {
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glClearColor(1, (glRenderer->target1Bd | (glRenderer->target2Bd * 2)) / 4.f, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (GBARegisterDISPCNTGetMode(glRenderer->dispcnt) != 0) {
		if (glRenderer->firstAffine < 0) {
			memcpy(&glRenderer->bg[2].affine[3], &glRenderer->bg[2].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[3].affine[3], &glRenderer->bg[3].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[2].affine[2], &glRenderer->bg[2].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[3].affine[2], &glRenderer->bg[3].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[2].affine[1], &glRenderer->bg[2].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[3].affine[1], &glRenderer->bg[3].affine[0], sizeof(struct GBAVideoGLAffine));
			glRenderer->firstAffine = y;
		} else if (y - glRenderer->firstAffine == 1) {
			memcpy(&glRenderer->bg[2].affine[1], &glRenderer->bg[2].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[3].affine[1], &glRenderer->bg[3].affine[0], sizeof(struct GBAVideoGLAffine));			
		}
	} else {
		glRenderer->firstAffine = -1;
	}

	unsigned priority;
	for (priority = 4; priority--;) {
		if (TEST_LAYER_ENABLED(0) && GBARegisterDISPCNTGetMode(glRenderer->dispcnt) < 2) {
			GBAVideoGLRendererDrawBackgroundMode0(glRenderer, &glRenderer->bg[0], y);
		}
		if (TEST_LAYER_ENABLED(1) && GBARegisterDISPCNTGetMode(glRenderer->dispcnt) < 2) {
			GBAVideoGLRendererDrawBackgroundMode0(glRenderer, &glRenderer->bg[1], y);
		}
		if (TEST_LAYER_ENABLED(2)) {
			switch (GBARegisterDISPCNTGetMode(glRenderer->dispcnt)) {
			case 0:
				GBAVideoGLRendererDrawBackgroundMode0(glRenderer, &glRenderer->bg[2], y);
				break;
			case 1:
			case 2:
				GBAVideoGLRendererDrawBackgroundMode2(glRenderer, &glRenderer->bg[2], y);
				break;
			case 3:
				//GBAVideoGLRendererDrawBackgroundMode3(glRenderer, &glRenderer->bg[2], y);
				break;
			case 4:
				//GBAVideoGLRendererDrawBackgroundMode4(glRenderer, &glRenderer->bg[2], y);
				break;
			case 5:
				//GBAVideoGLRendererDrawBackgroundMode5(glRenderer, &glRenderer->bg[2], y);
				break;
			}
		}
		if (TEST_LAYER_ENABLED(3)) {
			switch (GBARegisterDISPCNTGetMode(glRenderer->dispcnt)) {
			case 0:
				GBAVideoGLRendererDrawBackgroundMode0(glRenderer, &glRenderer->bg[3], y);
				break;
			case 2:
				GBAVideoGLRendererDrawBackgroundMode2(glRenderer, &glRenderer->bg[3], y);
				break;
			}
		}
	}

	if (GBARegisterDISPCNTGetMode(glRenderer->dispcnt) != 0) {
		memcpy(&glRenderer->bg[2].affine[3], &glRenderer->bg[2].affine[2], sizeof(struct GBAVideoGLAffine));
		memcpy(&glRenderer->bg[3].affine[3], &glRenderer->bg[3].affine[2], sizeof(struct GBAVideoGLAffine));
		memcpy(&glRenderer->bg[2].affine[2], &glRenderer->bg[2].affine[1], sizeof(struct GBAVideoGLAffine));
		memcpy(&glRenderer->bg[3].affine[2], &glRenderer->bg[3].affine[1], sizeof(struct GBAVideoGLAffine));
		memcpy(&glRenderer->bg[2].affine[1], &glRenderer->bg[2].affine[0], sizeof(struct GBAVideoGLAffine));
		memcpy(&glRenderer->bg[3].affine[1], &glRenderer->bg[3].affine[0], sizeof(struct GBAVideoGLAffine));

		glRenderer->bg[2].affine[0].sx += glRenderer->bg[2].affine[0].dmx;
		glRenderer->bg[2].affine[0].sy += glRenderer->bg[2].affine[0].dmy;
		glRenderer->bg[3].affine[0].sx += glRenderer->bg[3].affine[0].dmx;
		glRenderer->bg[3].affine[0].sy += glRenderer->bg[3].affine[0].dmy;
	}
}

void GBAVideoGLRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->firstAffine = -1;
	glRenderer->bg[2].affine[0].sx = glRenderer->bg[2].refx;
	glRenderer->bg[2].affine[0].sy = glRenderer->bg[2].refy;
	glRenderer->bg[3].affine[0].sx = glRenderer->bg[3].refx;
	glRenderer->bg[3].affine[0].sy = glRenderer->bg[3].refy;
	glFlush();
}

void GBAVideoGLRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {

}

void GBAVideoGLRendererPutPixels(struct GBAVideoRenderer* renderer, size_t stride, const void* pixels) {

}

static void _enableBg(struct GBAVideoGLRenderer* renderer, int bg, bool active) {
	int wasActive = renderer->bg[bg].enabled;
	if (!active) {
		renderer->bg[bg].enabled = 0;
	} else if (!wasActive && active) {
		/*if (renderer->nextY == 0 || GBARegisterDISPCNTGetMode(renderer->dispcnt) > 2) {
			// TODO: Investigate in more depth how switching background works in different modes
			renderer->bg[bg].enabled = 4;
		} else {
			renderer->bg[bg].enabled = 1;
		}*/
		renderer->bg[bg].enabled = 4;
	}
}

static void GBAVideoGLRendererUpdateDISPCNT(struct GBAVideoGLRenderer* renderer) {
	_enableBg(renderer, 0, GBARegisterDISPCNTGetBg0Enable(renderer->dispcnt));
	_enableBg(renderer, 1, GBARegisterDISPCNTGetBg1Enable(renderer->dispcnt));
	_enableBg(renderer, 2, GBARegisterDISPCNTGetBg2Enable(renderer->dispcnt));
	_enableBg(renderer, 3, GBARegisterDISPCNTGetBg3Enable(renderer->dispcnt));
}

static void GBAVideoGLRendererWriteBGCNT(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->priority = GBARegisterBGCNTGetPriority(value);
	bg->charBase = GBARegisterBGCNTGetCharBase(value) << 13;
	bg->mosaic = GBARegisterBGCNTGetMosaic(value);
	bg->multipalette = GBARegisterBGCNTGet256Color(value);
	bg->screenBase = GBARegisterBGCNTGetScreenBase(value) << 10;
	bg->overflow = GBARegisterBGCNTGetOverflow(value);
	bg->size = GBARegisterBGCNTGetSize(value);
}

static void GBAVideoGLRendererWriteBGX_LO(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refx = (bg->refx & 0xFFFF0000) | value;
	bg->affine[0].sx = bg->refx;
}

static void GBAVideoGLRendererWriteBGX_HI(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refx = (bg->refx & 0x0000FFFF) | (value << 16);
	bg->refx <<= 4;
	bg->refx >>= 4;
	bg->affine[0].sx = bg->refx;
}

static void GBAVideoGLRendererWriteBGY_LO(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refy = (bg->refy & 0xFFFF0000) | value;
	bg->affine[0].sy = bg->refy;
}

static void GBAVideoGLRendererWriteBGY_HI(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refy = (bg->refy & 0x0000FFFF) | (value << 16);
	bg->refy <<= 4;
	bg->refy >>= 4;
	bg->affine[0].sy = bg->refy;
}

static void GBAVideoGLRendererWriteBLDCNT(struct GBAVideoGLRenderer* renderer, uint16_t value) {
	renderer->bg[0].target1 = GBARegisterBLDCNTGetTarget1Bg0(value);
	renderer->bg[1].target1 = GBARegisterBLDCNTGetTarget1Bg1(value);
	renderer->bg[2].target1 = GBARegisterBLDCNTGetTarget1Bg2(value);
	renderer->bg[3].target1 = GBARegisterBLDCNTGetTarget1Bg3(value);
	renderer->bg[0].target2 = GBARegisterBLDCNTGetTarget2Bg0(value);
	renderer->bg[1].target2 = GBARegisterBLDCNTGetTarget2Bg1(value);
	renderer->bg[2].target2 = GBARegisterBLDCNTGetTarget2Bg2(value);
	renderer->bg[3].target2 = GBARegisterBLDCNTGetTarget2Bg3(value);

	renderer->blendEffect = GBARegisterBLDCNTGetEffect(value);
	renderer->target1Obj = GBARegisterBLDCNTGetTarget1Obj(value);
	renderer->target1Bd = GBARegisterBLDCNTGetTarget1Bd(value);
	renderer->target2Obj = GBARegisterBLDCNTGetTarget2Obj(value);
	renderer->target2Bd = GBARegisterBLDCNTGetTarget2Bd(value);
}

static void _compositeLayer(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y, int flags) {
	if ((y & 0x1F) != 0x1F) {
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo[1]);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glScissor(0, (y * renderer->scale) % (0x20 * renderer->scale), GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, 0x20 * renderer->scale);
	glUseProgram(renderer->compositeProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, background->tex);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, renderer->outputTex);
	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, renderer->layers[2]);
	glUniform2i(0, 0x20, y & ~0x1F);
	glUniform3i(1, (background->priority << 3) + (background->index << 1) + 1, flags, renderer->blendEffect);
	glUniform1i(2, renderer->scale);
	glUniform3f(3, renderer->blda / 16.f, renderer->bldb / 16.f, renderer->bldy / 16.f);
	glUniform1i(4, 0);
	glUniform1i(5, 1);
	glUniform1i(6, 2);
	glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, _vertices);
	glEnableVertexAttribArray(0);
	glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void GBAVideoGLRendererDrawBackgroundMode0(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	int inY = y + background->y;
	int yBase = inY & 0xFF;
	if (background->size == 2) {
		yBase += inY & 0x100;
	} else if (background->size == 3) {
		yBase += (inY & 0x100) << 1;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glScissor(0, y * renderer->scale, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, renderer->scale);
	glUseProgram(renderer->bgProgram[background->multipalette ? 1 : 0]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->vramTex);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, renderer->paletteTex);
	glUniform2i(0, 1, y);
	glUniform1i(1, 0);
	glUniform1i(2, 1);
	glUniform1i(3, background->screenBase);
	glUniform1i(4, background->charBase);
	glUniform1i(5, background->size);
	glUniform2i(6, background->x, yBase - y);
	glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, _vertices);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	_compositeLayer(renderer, background, y, background->target1 | (background->target2 * 2));

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAVideoGLRendererDrawBackgroundMode2(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glScissor(0, y * renderer->scale, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, renderer->scale);
	glUseProgram(renderer->bgProgram[background->overflow ? 2 : 3]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->vramTex);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, renderer->paletteTex);
	glUniform2i(0, 1, y);
	glUniform1i(1, 0);
	glUniform1i(2, 1);
	glUniform1i(3, background->screenBase);
	glUniform1i(4, background->charBase);
	glUniform1i(5, background->size);
	if (renderer->scale > 1) {
		glUniform2iv(6, 4, (GLint[]) {
			background->affine[0].sx, background->affine[0].sy,
			background->affine[1].sx, background->affine[1].sy,
			background->affine[2].sx, background->affine[2].sy,
			background->affine[3].sx, background->affine[3].sy,
		});
		glUniform2iv(10, 4, (GLint[]) {
			background->affine[0].dx, background->affine[0].dy,
			background->affine[1].dx, background->affine[1].dy,
			background->affine[2].dx, background->affine[2].dy,
			background->affine[3].dx, background->affine[3].dy,
		});
	} else {
		glUniform2iv(6, 4, (GLint[]) {
			background->affine[0].sx, background->affine[0].sy,
			background->affine[0].sx, background->affine[0].sy,
			background->affine[0].sx, background->affine[0].sy,
			background->affine[0].sx, background->affine[0].sy,
		});
		glUniform2iv(10, 4, (GLint[]) {
			background->affine[0].dx, background->affine[0].dy,
			background->affine[0].dx, background->affine[0].dy,
			background->affine[0].dx, background->affine[0].dy,
			background->affine[0].dx, background->affine[0].dy,
		});
	}
	glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, _vertices);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	_compositeLayer(renderer, background, y, background->target1 | (background->target2 * 2));

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}