/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/renderers/gl.h>

#if defined(BUILD_GLES2) || defined(BUILD_GLES3)

#include <mgba/core/cache-set.h>
#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/cache-set.h>
#include <mgba-util/memory.h>

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

static void GBAVideoGLRendererDrawSprite(struct GBAVideoGLRenderer* renderer, struct GBAObj* sprite, int y, int spriteY);
static void GBAVideoGLRendererDrawBackgroundMode0(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawBackgroundMode2(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawBackgroundMode3(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawBackgroundMode4(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawBackgroundMode5(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y);
static void GBAVideoGLRendererDrawWindow(struct GBAVideoGLRenderer* renderer, int y);

static void _finalizeLayers(struct GBAVideoGLRenderer* renderer);

#define TEST_LAYER_ENABLED(X) !renderer->disableBG[X] && glRenderer->bg[X].enabled == 4

struct GBAVideoGLUniform {
	const char* name;
	int type;
};

static const GLchar* const _gl3Header =
	"#version 130\n";

static const char* const _vertexShader =
	"in vec2 position;\n"
	"uniform ivec2 loc;\n"
	"uniform ivec2 maxPos;\n"
	"out vec2 texCoord;\n"

	"void main() {\n"
	"	vec2 local = vec2(position.x, float(position.y * loc.x + loc.y) / abs(maxPos.y));\n"
	"	gl_Position = vec4((local * 2. - 1.) * sign(maxPos), 0., 1.);\n"
	"	texCoord = local * abs(maxPos);\n"
	"}";

static const char* const _renderTile16 =
	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord) {\n"
	"	int address = charBase + tile * 16 + (localCoord.x >> 2) + (localCoord.y << 1);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - (localCoord.x & 3)] * 15.9);\n"
	"	vec4 color = texelFetch(palette, ivec2(entry, paletteId), 0);\n"
	"	if (entry == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	color.a = 1;\n"
	"	return color;\n"
	"}";

static const char* const _renderTile256 =
	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord) {\n"
	"	int address = charBase + tile * 32 + (localCoord.x >> 1) + (localCoord.y << 2);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - 2 * (localCoord.x & 1)] * 15.9);\n"
	"	int pal2 = int(halfrow[2 - 2 * (localCoord.x & 1)] * 15.9);\n"
	"	vec4 color = texelFetch(palette, ivec2(entry, pal2 + (paletteId & 16)), 0);\n"
	"	if ((pal2 | entry) == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	color.a = 1.;\n"
	"	return color;\n"
	"}";

static const struct GBAVideoGLUniform _uniformsMode0[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "vram", GBA_GL_BG_VRAM, },
	{ "palette", GBA_GL_BG_PALETTE, },
	{ "screenBase", GBA_GL_BG_SCREENBASE, },
	{ "charBase", GBA_GL_BG_CHARBASE, },
	{ "size", GBA_GL_BG_SIZE, },
	{ "offset", GBA_GL_BG_OFFSET, },
	{ "inflags", GBA_GL_BG_INFLAGS, },
	{ 0 }
};

static const char* const _renderMode0 =
	"in vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int screenBase;\n"
	"uniform int charBase;\n"
	"uniform int size;\n"
	"uniform ivec2 offset;\n"
	"uniform ivec4 inflags;\n"
	"out vec4 color;\n"
	"out vec4 flags;\n"
	"const vec4 flagCoeff = vec4(32., 32., 16., 16.);\n"

	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord);\n"

	"void main() {\n"
	"	ivec2 coord = ivec2(texCoord) + offset;\n"
	"	if ((size & 1) == 1) {\n"
	"		coord.y += coord.x & 256;\n"
	"	}\n"
	"	coord.x &= 255;\n"
	"	int mapAddress = screenBase + (coord.x >> 3) + (coord.y >> 3) * 32;\n"
	"	vec4 map = texelFetch(vram, ivec2(mapAddress & 255, mapAddress >> 8), 0);\n"
	"	int tileFlags = int(map.g * 15.9);\n"
	"	if ((tileFlags & 4) == 4) {\n"
	"		coord.x ^= 7;\n"
	"	}\n"
	"	if ((tileFlags & 8) == 8) {\n"
	"		coord.y ^= 7;\n"
	"	}\n"
	"	int tile = int(map.a * 15.9) + int(map.b * 15.9) * 16 + (tileFlags & 0x3) * 256;\n"
	"	color = renderTile(tile, int(map.r * 15.9), coord & 7);\n"
	"	flags = inflags / flagCoeff;\n"
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
	"		discard;\n"
	"	}\n"
	"	return renderTile(coord);\n"
	"}";

static const struct GBAVideoGLUniform _uniformsMode2[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "vram", GBA_GL_BG_VRAM, },
	{ "palette", GBA_GL_BG_PALETTE, },
	{ "screenBase", GBA_GL_BG_SCREENBASE, },
	{ "charBase", GBA_GL_BG_CHARBASE, },
	{ "size", GBA_GL_BG_SIZE, },
	{ "inflags", GBA_GL_BG_INFLAGS, },
	{ "offset", GBA_GL_BG_OFFSET, },
	{ "transform", GBA_GL_BG_TRANSFORM, },
	{ "range", GBA_GL_BG_RANGE, },
	{ 0 }
};

static const char* const _renderMode2 =
	"in vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int screenBase;\n"
	"uniform int charBase;\n"
	"uniform int size;\n"
	"uniform ivec4 inflags;\n"
	"uniform ivec2[4] offset;\n"
	"uniform ivec2[4] transform;\n"
	"uniform vec2 range;\n"
	"out vec4 color;\n"
	"out vec4 flags;\n"
	"const vec4 flagCoeff = vec4(32., 32., 16., 16.);\n"
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
	"	if ((pal2 | entry) == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	color.a = 1.;\n"
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
	"	float y = texCoord.y - range.x;\n"
	"	float lin = 0.5 - y / range.y * 0.25;\n"
	"	vec2 mixedTransform = interpolate(transform, lin);\n"
	"	vec2 mixedOffset = interpolate(offset, lin);\n"
	"	color = fetchTile(ivec2(mixedTransform * texCoord.x + mixedOffset));\n"
	"	flags = inflags / flagCoeff;\n"
	"}";

static const struct GBAVideoGLUniform _uniformsObj[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "vram", GBA_GL_OBJ_VRAM, },
	{ "palette", GBA_GL_OBJ_PALETTE, },
	{ "charBase", GBA_GL_OBJ_CHARBASE, },
	{ "stride", GBA_GL_OBJ_STRIDE, },
	{ "localPalette", GBA_GL_OBJ_LOCALPALETTE, },
	{ "inflags", GBA_GL_OBJ_INFLAGS, },
	{ "transform", GBA_GL_OBJ_TRANSFORM, },
	{ "dims", GBA_GL_OBJ_DIMS, },
	{ "objwin", GBA_GL_OBJ_OBJWIN, },
	{ 0 }
};

static const char* const _renderObj =
	"in vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int charBase;\n"
	"uniform int stride;\n"
	"uniform int localPalette;\n"
	"uniform ivec4 inflags;\n"
	"uniform mat2x2 transform;\n"
	"uniform ivec4 dims;\n"
	"uniform vec3 objwin;\n"
	"out vec4 color;\n"
	"out vec4 flags;\n"
	"out vec2 window;\n"
	"const vec4 flagCoeff = vec4(32., 32., 16., 16.);\n"

	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord);\n"

	"void main() {\n"
	"	ivec2 coord = ivec2(transform * (texCoord - dims.zw / 2) + dims.xy / 2);\n"
	"	if ((coord & ~(dims.xy - 1)) != ivec2(0, 0)) {\n"
	"		discard;\n"
	"	}\n"
	"	vec4 pix = renderTile((coord.x >> 3) + (coord.y >> 3) * stride, 16 + localPalette, coord & 7);\n"
	"	if (objwin.x > 0) {\n"
	"		pix.a = 0;\n"
	"	}\n"
	"	color = pix;\n"
	"	flags = inflags / flagCoeff;\n"
	"	window = objwin.yz;\n"
	"}";

static const struct GBAVideoGLUniform _uniformsFinalize[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "scale", GBA_GL_FINALIZE_SCALE, },
	{ "layers", GBA_GL_FINALIZE_LAYERS, },
	{ "flags", GBA_GL_FINALIZE_FLAGS, },
	{ "window", GBA_GL_FINALIZE_WINDOW, },
	{ "backdrop", GBA_GL_FINALIZE_BACKDROP, },
	{ "backdropFlags", GBA_GL_FINALIZE_BACKDROPFLAGS, },
	{ 0 }
};

static const char* const _finalize =
	"in vec2 texCoord;\n"
	"uniform int scale;\n"
	"uniform sampler2D layers[5];\n"
	"uniform sampler2D flags[5];\n"
	"uniform sampler2D window;\n"
	"uniform vec4 backdrop;\n"
	"uniform vec4 backdropFlags;\n"
	"const vec4 flagCoeff = vec4(32., 32., 16., 16.);\n"
	"out vec4 color;\n"

	"void composite(vec4 pixel, ivec4 flags, inout vec4 topPixel, inout ivec4 topFlags, inout vec4 bottomPixel, inout ivec4 bottomFlags) {\n"
	"	if (pixel.a == 0) {\n"
	"		return;\n"
	"	}\n"
	"	if (flags.x >= topFlags.x) {\n"
	"		if (flags.x >= bottomFlags.x) {\n"
	"			return;\n"
	"		}\n"
	"		bottomFlags = flags;\n"
	"		bottomPixel = pixel;\n"
	"	} else {\n"
	"		bottomFlags = topFlags;\n"
	"		topFlags = flags;\n"
	"		bottomPixel = topPixel;\n"
	"		topPixel = pixel;\n"
	"	}\n"
	"}\n"

	"void main() {\n"
	"	vec4 windowFlags = texelFetch(window, ivec2(texCoord * scale), 0);\n"
	"	int layerWindow = int(windowFlags.x * 32) | int(windowFlags.y * 512);\n"
	"	vec4 topPixel = backdrop;\n"
	"	vec4 bottomPixel = backdrop;\n"
	"	ivec4 topFlags = ivec4(backdropFlags * flagCoeff);\n"
	"	ivec4 bottomFlags = ivec4(backdropFlags * flagCoeff);\n"
	"	if ((layerWindow & 1) == 0) {\n"
	"		vec4 pix = texelFetch(layers[0], ivec2(texCoord * scale), 0);\n"
	"		ivec4 inflags = ivec4(texelFetch(flags[0], ivec2(texCoord * scale), 0) * flagCoeff);\n"
	"		composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"	}\n"
	"	if ((layerWindow & 2) == 0) {\n"
	"		vec4 pix = texelFetch(layers[1], ivec2(texCoord * scale), 0);\n"
	"		ivec4 inflags = ivec4(texelFetch(flags[1], ivec2(texCoord * scale), 0) * flagCoeff);\n"
	"		composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"	}\n"
	"	if ((layerWindow & 4) == 0) {\n"
	"		vec4 pix = texelFetch(layers[2], ivec2(texCoord * scale), 0);\n"
	"		ivec4 inflags = ivec4(texelFetch(flags[2], ivec2(texCoord * scale), 0) * flagCoeff);\n"
	"		composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"	}\n"
	"	if ((layerWindow & 8) == 0) {\n"
	"		vec4 pix = texelFetch(layers[3], ivec2(texCoord * scale), 0);\n"
	"		ivec4 inflags = ivec4(texelFetch(flags[3], ivec2(texCoord * scale), 0) * flagCoeff);\n"
	"		composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"	}\n"
	"	if ((layerWindow & 32) != 0) {\n"
	"		topFlags.y &= ~1;\n"
	"	}\n"
	"	if ((layerWindow & 16) == 0) {\n"
	"		vec4 pix = texelFetch(layers[4], ivec2(texCoord * scale), 0);\n"
	"		ivec4 inflags = ivec4(texelFetch(flags[4], ivec2(texCoord * scale), 0) * flagCoeff);\n"
	"		composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"	}\n"
	"	if ((topFlags.y & 13) == 5) {\n"
	"		if ((bottomFlags.y & 2) == 2) {\n"
	"			topPixel *= topFlags.z / 16.;\n"
	"			topPixel += bottomPixel * bottomFlags.w / 16.;\n"
	"		}\n"
	"	} else if ((topFlags.y & 13) == 9) {\n"
	"		topPixel += (1. - topPixel) * windowFlags.z;\n"
	"	} else if ((topFlags.y & 13) == 13) {\n"
	"		topPixel -= topPixel * windowFlags.z;\n"
	"	}\n"
	"	color = topPixel;\n"
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

void _compileShader(struct GBAVideoGLRenderer* glRenderer, struct GBAVideoGLShader* shader, const char** shaderBuffer, int shaderBufferLines, GLuint vs, const struct GBAVideoGLUniform* uniforms, char* log) {
	GLuint program = glCreateProgram();
	shader->program = program;

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
#ifndef BUILD_GLES3
	glBindFragDataLocation(program, 0, "color");
	glBindFragDataLocation(program, 1, "flags");
#endif

	glGenVertexArrays(1, &shader->vao);
	glBindVertexArray(shader->vao);
	glBindBuffer(GL_ARRAY_BUFFER, glRenderer->vbo);
	GLuint positionLocation = glGetAttribLocation(program, "position");
	glVertexAttribPointer(positionLocation, 2, GL_INT, GL_FALSE, 0, NULL);

	size_t i;
	for (i = 0; uniforms[i].name; ++i) {
		shader->uniforms[uniforms[i].type] = glGetUniformLocation(program, uniforms[i].name);
	}
}

static void _initFramebufferTexture(GLuint tex, GLenum format, GLenum attachment, int scale) {
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, format, GBA_VIDEO_HORIZONTAL_PIXELS * scale, GBA_VIDEO_VERTICAL_PIXELS * scale, 0, format, GL_UNSIGNED_BYTE, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex, 0);	
}

void GBAVideoGLRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->temporaryBuffer = NULL;

	glGenFramebuffers(GBA_GL_FBO_MAX, glRenderer->fbo);
	glGenTextures(GBA_GL_TEX_MAX, glRenderer->layers);

	glGenTextures(1, &glRenderer->paletteTex);
	glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenTextures(1, &glRenderer->vramTex);
	glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OBJ]);
	_initFramebufferTexture(glRenderer->layers[GBA_GL_TEX_OBJ_COLOR], GL_RGBA, GL_COLOR_ATTACHMENT0, glRenderer->scale);
	_initFramebufferTexture(glRenderer->layers[GBA_GL_TEX_OBJ_FLAGS], GL_RGBA, GL_COLOR_ATTACHMENT1, glRenderer->scale);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_WINDOW]);
	_initFramebufferTexture(glRenderer->layers[GBA_GL_TEX_WINDOW], GL_RGB, GL_COLOR_ATTACHMENT0, glRenderer->scale);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OUTPUT]);
	_initFramebufferTexture(glRenderer->outputTex, GL_RGB, GL_COLOR_ATTACHMENT0, glRenderer->scale);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenBuffers(1, &glRenderer->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, glRenderer->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_vertices), _vertices, GL_STATIC_DRAW);

	int i;
	for (i = 0; i < 4; ++i) {
		struct GBAVideoGLBackground* bg = &glRenderer->bg[i];
		bg->index = i;
		bg->enabled = 0;
		bg->priority = 0;
		bg->charBase = 0;
		bg->mosaic = 0;
		bg->multipalette = 0;
		bg->screenBase = 0;
		bg->overflow = 0;
		bg->size = 0;
		bg->target1 = 0;
		bg->target2 = 0;
		bg->x = 0;
		bg->y = 0;
		bg->refx = 0;
		bg->refy = 0;
		bg->affine[0].dx = 256;
		bg->affine[0].dmx = 0;
		bg->affine[0].dy = 0;
		bg->affine[0].dmy = 256;
		bg->affine[0].sx = 0;
		bg->affine[0].sy = 0;
		glGenFramebuffers(1, &bg->fbo);
		glGenTextures(1, &bg->tex);
		glGenTextures(1, &bg->flags);
		glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
		_initFramebufferTexture(bg->tex, GL_RGBA, GL_COLOR_ATTACHMENT0, glRenderer->scale);
		_initFramebufferTexture(bg->flags, GL_RGBA, GL_COLOR_ATTACHMENT1, glRenderer->scale);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

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
	_compileShader(glRenderer, &glRenderer->bgShader[0], shaderBuffer, 3, vs, _uniformsMode0, log);

	shaderBuffer[2] = _renderTile256;
	_compileShader(glRenderer, &glRenderer->bgShader[1], shaderBuffer, 3, vs, _uniformsMode0, log);

	shaderBuffer[1] = _renderMode2;

	shaderBuffer[2] = _fetchTileOverflow;
	_compileShader(glRenderer, &glRenderer->bgShader[2], shaderBuffer, 3, vs, _uniformsMode2, log);

	shaderBuffer[2] = _fetchTileNoOverflow;
	_compileShader(glRenderer, &glRenderer->bgShader[3], shaderBuffer, 3, vs, _uniformsMode2, log);

	shaderBuffer[1] = _renderObj;

	shaderBuffer[2] = _renderTile16;
	_compileShader(glRenderer, &glRenderer->objShader[0], shaderBuffer, 3, vs, _uniformsObj, log);
#ifndef BUILD_GLES3
	glBindFragDataLocation(glRenderer->objShader[0].program, 2, "window");
#endif

	shaderBuffer[2] = _renderTile256;
	_compileShader(glRenderer, &glRenderer->objShader[1], shaderBuffer, 3, vs, _uniformsObj, log);
#ifndef BUILD_GLES3
	glBindFragDataLocation(glRenderer->objShader[1].program, 2, "window");
#endif

	shaderBuffer[1] = _finalize;
	_compileShader(glRenderer, &glRenderer->finalizeShader, shaderBuffer, 2, vs, _uniformsFinalize, log);

	glBindVertexArray(0);
	glDeleteShader(vs);

	GBAVideoGLRendererReset(renderer);
}

void GBAVideoGLRendererDeinit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (glRenderer->temporaryBuffer) {
		mappedMemoryFree(glRenderer->temporaryBuffer, GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * glRenderer->scale * glRenderer->scale);
	}
	glDeleteFramebuffers(GBA_GL_FBO_MAX, glRenderer->fbo);
	glDeleteTextures(GBA_GL_TEX_MAX, glRenderer->layers);
	glDeleteTextures(1, &glRenderer->paletteTex);
	glDeleteTextures(1, &glRenderer->vramTex);
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
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
#ifdef BUILD_GLES3
	glRenderer->shadowPalette[address >> 1] = (value & 0x3F) | ((value & 0x7FE0) << 1);
#else
	UNUSED(address);
	UNUSED(value);
#endif
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
		glRenderer->bldy = value;
		break;
	case REG_WIN0H:
		glRenderer->winN[0].h.end = value;
		glRenderer->winN[0].h.start = value >> 8;
		if (glRenderer->winN[0].h.start > GBA_VIDEO_HORIZONTAL_PIXELS && glRenderer->winN[0].h.start > glRenderer->winN[0].h.end) {
			glRenderer->winN[0].h.start = 0;
		}
		if (glRenderer->winN[0].h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
			glRenderer->winN[0].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
			if (glRenderer->winN[0].h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
				glRenderer->winN[0].h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
			}
		}
		break;
	case REG_WIN1H:
		glRenderer->winN[1].h.end = value;
		glRenderer->winN[1].h.start = value >> 8;
		if (glRenderer->winN[1].h.start > GBA_VIDEO_HORIZONTAL_PIXELS && glRenderer->winN[1].h.start > glRenderer->winN[1].h.end) {
			glRenderer->winN[1].h.start = 0;
		}
		if (glRenderer->winN[1].h.end > GBA_VIDEO_HORIZONTAL_PIXELS) {
			glRenderer->winN[1].h.end = GBA_VIDEO_HORIZONTAL_PIXELS;
			if (glRenderer->winN[1].h.start > GBA_VIDEO_HORIZONTAL_PIXELS) {
				glRenderer->winN[1].h.start = GBA_VIDEO_HORIZONTAL_PIXELS;
			}
		}
		break;
	case REG_WIN0V:
		glRenderer->winN[0].v.end = value;
		glRenderer->winN[0].v.start = value >> 8;
		if (glRenderer->winN[0].v.start > GBA_VIDEO_VERTICAL_PIXELS && glRenderer->winN[0].v.start > glRenderer->winN[0].v.end) {
			glRenderer->winN[0].v.start = 0;
		}
		if (glRenderer->winN[0].v.end > GBA_VIDEO_VERTICAL_PIXELS) {
			glRenderer->winN[0].v.end = GBA_VIDEO_VERTICAL_PIXELS;
			if (glRenderer->winN[0].v.start > GBA_VIDEO_VERTICAL_PIXELS) {
				glRenderer->winN[0].v.start = GBA_VIDEO_VERTICAL_PIXELS;
			}
		}
		break;
	case REG_WIN1V:
		glRenderer->winN[1].v.end = value;
		glRenderer->winN[1].v.start = value >> 8;
		if (glRenderer->winN[1].v.start > GBA_VIDEO_VERTICAL_PIXELS && glRenderer->winN[1].v.start > glRenderer->winN[1].v.end) {
			glRenderer->winN[1].v.start = 0;
		}
		if (glRenderer->winN[1].v.end > GBA_VIDEO_VERTICAL_PIXELS) {
			glRenderer->winN[1].v.end = GBA_VIDEO_VERTICAL_PIXELS;
			if (glRenderer->winN[1].v.start > GBA_VIDEO_VERTICAL_PIXELS) {
				glRenderer->winN[1].v.start = GBA_VIDEO_VERTICAL_PIXELS;
			}
		}
		break;
	case REG_WININ:
		value &= 0x3F3F;
		glRenderer->winN[0].control = value;
		glRenderer->winN[1].control = value >> 8;
		break;
	case REG_WINOUT:
		value &= 0x3F3F;
		glRenderer->winout = value;
		glRenderer->objwin = value >> 8;
		break;
	case REG_MOSAIC:
		glRenderer->mosaic = value;
		break;
	default:
		break;
	}
	return value;
}

void GBAVideoGLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (glRenderer->paletteDirty) {
		glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
#ifdef BUILD_GLES3
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, 16, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, glRenderer->shadowPalette);
#else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 16, 32, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, glRenderer->d.palette);
#endif
		glRenderer->paletteDirty = false;
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

	if (y == 0) {
		glDisable(GL_SCISSOR_TEST);		
		glClearColor(0, 0, 0, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OBJ]);
		glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
		glClear(GL_COLOR_BUFFER_BIT);

		for (i = 0; i < 4; ++i) {
			glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->bg[i].fbo);
			glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
			glClear(GL_COLOR_BUFFER_BIT);
		}
		glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
	}
	glEnable(GL_SCISSOR_TEST);

	if (GBARegisterDISPCNTGetMode(glRenderer->dispcnt) != 0) {
		if (glRenderer->firstAffine < 0) {
			memcpy(&glRenderer->bg[2].affine[3], &glRenderer->bg[2].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[3].affine[3], &glRenderer->bg[3].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[2].affine[2], &glRenderer->bg[2].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[3].affine[2], &glRenderer->bg[3].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[2].affine[1], &glRenderer->bg[2].affine[0], sizeof(struct GBAVideoGLAffine));
			memcpy(&glRenderer->bg[3].affine[1], &glRenderer->bg[3].affine[0], sizeof(struct GBAVideoGLAffine));
			glRenderer->firstAffine = y;
		}
	} else {
		glRenderer->firstAffine = -1;
	}

	GBAVideoGLRendererDrawWindow(glRenderer, y);
	if (GBARegisterDISPCNTIsObjEnable(glRenderer->dispcnt) && !glRenderer->d.disableOBJ) {
		if (glRenderer->oamDirty) {
			glRenderer->oamMax = GBAVideoRendererCleanOAM(glRenderer->d.oam->obj, glRenderer->sprites, 0);
			glRenderer->oamDirty = false;
		}
		int i;
		for (i = glRenderer->oamMax; i--;) {
			struct GBAVideoRendererSprite* sprite = &glRenderer->sprites[i];
			if ((y < sprite->y && (sprite->endY - 256 < 0 || y >= sprite->endY - 256)) || y >= sprite->endY) {
				continue;
			}

			GBAVideoGLRendererDrawSprite(glRenderer, &sprite->obj, y, sprite->y);
		}
	}

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
	_finalizeLayers(glRenderer);
	glRenderer->firstAffine = -1;
	glRenderer->bg[2].affine[0].sx = glRenderer->bg[2].refx;
	glRenderer->bg[2].affine[0].sy = glRenderer->bg[2].refy;
	glRenderer->bg[3].affine[0].sx = glRenderer->bg[3].refx;
	glRenderer->bg[3].affine[0].sy = glRenderer->bg[3].refy;
	glFlush();
}

void GBAVideoGLRendererGetPixels(struct GBAVideoRenderer* renderer, size_t* stride, const void** pixels) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	*stride = GBA_VIDEO_HORIZONTAL_PIXELS * glRenderer->scale;
	if (!glRenderer->temporaryBuffer) {
		glRenderer->temporaryBuffer = anonymousMemoryMap(GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * glRenderer->scale * glRenderer->scale * BYTES_PER_PIXEL);
	}
	glFinish();
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OUTPUT]);
	glPixelStorei(GL_PACK_ROW_LENGTH, GBA_VIDEO_HORIZONTAL_PIXELS * glRenderer->scale);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * glRenderer->scale, GBA_VIDEO_VERTICAL_PIXELS * glRenderer->scale, GL_RGBA, GL_UNSIGNED_BYTE, (void*) glRenderer->temporaryBuffer);
	*pixels = glRenderer->temporaryBuffer;
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

void _finalizeLayers(struct GBAVideoGLRenderer* renderer) {
	const GLuint* uniforms = renderer->finalizeShader.uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo[GBA_GL_FBO_OUTPUT]);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glScissor(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glUseProgram(renderer->finalizeShader.program);
	glBindVertexArray(renderer->finalizeShader.vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->layers[GBA_GL_TEX_WINDOW]);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, renderer->layers[GBA_GL_TEX_OBJ_COLOR]);
	glActiveTexture(GL_TEXTURE0 + 2);
	glBindTexture(GL_TEXTURE_2D, renderer->layers[GBA_GL_TEX_OBJ_FLAGS]);
	glActiveTexture(GL_TEXTURE0 + 3);
	glBindTexture(GL_TEXTURE_2D, renderer->bg[0].tex);
	glActiveTexture(GL_TEXTURE0 + 4);
	glBindTexture(GL_TEXTURE_2D, renderer->bg[0].flags);
	glActiveTexture(GL_TEXTURE0 + 5);
	glBindTexture(GL_TEXTURE_2D, renderer->bg[1].tex);
	glActiveTexture(GL_TEXTURE0 + 6);
	glBindTexture(GL_TEXTURE_2D, renderer->bg[1].flags);
	glActiveTexture(GL_TEXTURE0 + 7);
	glBindTexture(GL_TEXTURE_2D, renderer->bg[2].tex);
	glActiveTexture(GL_TEXTURE0 + 8);
	glBindTexture(GL_TEXTURE_2D, renderer->bg[2].flags);
	glActiveTexture(GL_TEXTURE0 + 9);
	glBindTexture(GL_TEXTURE_2D, renderer->bg[3].tex);
	glActiveTexture(GL_TEXTURE0 + 10);
	glBindTexture(GL_TEXTURE_2D, renderer->bg[3].flags);

	uint32_t backdrop = M_RGB5_TO_RGB8(renderer->d.palette[0]);
	glUniform2i(uniforms[GBA_GL_VS_LOC], GBA_VIDEO_VERTICAL_PIXELS, 0);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glUniform1i(uniforms[GBA_GL_FINALIZE_SCALE], renderer->scale);
	glUniform1iv(uniforms[GBA_GL_FINALIZE_LAYERS], 5, (GLint[]) { 3, 5, 7, 9, 1 });
	glUniform1iv(uniforms[GBA_GL_FINALIZE_FLAGS], 5, (GLint[]) { 4, 6, 8, 10, 2 });
	glUniform1i(uniforms[GBA_GL_FINALIZE_WINDOW], 0);
	glUniform4f(uniforms[GBA_GL_FINALIZE_BACKDROP], ((backdrop >> 16) & 0xFF) / 256., ((backdrop >> 8) & 0xFF) / 256., (backdrop & 0xFF) / 256., 0.f);
	glUniform4f(uniforms[GBA_GL_FINALIZE_BACKDROPFLAGS], 1, (renderer->target1Bd | (renderer->target2Bd * 2) | (renderer->blendEffect * 4)) / 32.f, renderer->blda / 16.f, renderer->bldb / 16.f);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAVideoGLRendererDrawSprite(struct GBAVideoGLRenderer* renderer, struct GBAObj* sprite, int y, int spriteY) {
	int width = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][0];
	int height = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][1];
	int32_t x = (uint32_t) GBAObjAttributesBGetX(sprite->b) << 23;
	x >>= 23;

	int align = GBAObjAttributesAIs256Color(sprite->a) && !GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt);
	unsigned charBase = (BASE_TILE >> 1) + (GBAObjAttributesCGetTile(sprite->c) & ~align) * 0x10;
	int stride = GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? (width >> 3) : (0x20 >> GBAObjAttributesAGet256Color(sprite->a));

	if (spriteY + height >= 256) {
		spriteY -= 256;
	}

	if (!GBAObjAttributesAIsTransformed(sprite->a) && GBAObjAttributesBIsVFlip(sprite->b)) {
		spriteY = (y - height) + (y - spriteY) + 1;
	}

	int totalWidth = width;
	int totalHeight = height;
	if (GBAObjAttributesAIsTransformed(sprite->a) && GBAObjAttributesAIsDoubleSize(sprite->a)) {
		totalWidth <<= 1;
		totalHeight <<= 1;
	}

	enum GBAVideoBlendEffect blendEffect = GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT ? BLEND_ALPHA : renderer->blendEffect;

	const struct GBAVideoGLShader* shader = &renderer->objShader[GBAObjAttributesAGet256Color(sprite->a)];
	const GLuint* uniforms = shader->uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo[GBA_GL_FBO_OBJ]);
	glViewport(x * renderer->scale, spriteY * renderer->scale, totalWidth * renderer->scale, totalHeight * renderer->scale);
	glScissor(x * renderer->scale, y * renderer->scale, totalWidth * renderer->scale, renderer->scale);
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->vramTex);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, renderer->paletteTex);
	glUniform2i(uniforms[GBA_GL_VS_LOC], 1, y - spriteY);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], (GBAObjAttributesBIsHFlip(sprite->b) && !GBAObjAttributesAIsTransformed(sprite->a)) ? -totalWidth : totalWidth, totalHeight);
	glUniform1i(uniforms[GBA_GL_OBJ_VRAM], 0);
	glUniform1i(uniforms[GBA_GL_OBJ_PALETTE], 1);
	glUniform1i(uniforms[GBA_GL_OBJ_CHARBASE], charBase);
	glUniform1i(uniforms[GBA_GL_OBJ_STRIDE], stride);
	glUniform1i(uniforms[GBA_GL_OBJ_LOCALPALETTE], GBAObjAttributesCGetPalette(sprite->c));
	glUniform4i(uniforms[GBA_GL_OBJ_INFLAGS], GBAObjAttributesCGetPriority(sprite->c) << 3,
	                                          (renderer->target1Obj || GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT) | (renderer->target2Obj * 2) | (blendEffect * 4),
	                                          renderer->blda, renderer->bldb);
	if (GBAObjAttributesAIsTransformed(sprite->a)) {
		struct GBAOAMMatrix mat;
		LOAD_16(mat.a, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].a);
		LOAD_16(mat.b, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].b);
		LOAD_16(mat.c, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].c);
		LOAD_16(mat.d, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].d);

		glUniformMatrix2fv(uniforms[GBA_GL_OBJ_TRANSFORM], 1, GL_FALSE, (GLfloat[]) { mat.a / 256.f, mat.c / 256.f, mat.b / 256.f, mat.d / 256.f });
	} else {
		glUniformMatrix2fv(uniforms[GBA_GL_OBJ_TRANSFORM], 1, GL_FALSE, (GLfloat[]) { 1.f, 0, 0, 1.f });
	}
	glUniform4i(uniforms[GBA_GL_OBJ_DIMS], width, height, totalWidth, totalHeight);
	if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_OBJWIN) {
		int window = ~renderer->objwin & 0xFF;
		glUniform3f(uniforms[GBA_GL_OBJ_OBJWIN], 1, (window & 0xF) / 32.f, (window >> 4) / 32.f);
		glDrawBuffers(3, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });
	} else {
		glUniform3f(uniforms[GBA_GL_OBJ_OBJWIN], 0, 0, 0);
		glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	}
	glEnableVertexAttribArray(0);
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

	const struct GBAVideoGLShader* shader = &renderer->bgShader[background->multipalette ? 1 : 0];
	const GLuint* uniforms = shader->uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glScissor(0, y * renderer->scale, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, renderer->scale);
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->vramTex);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, renderer->paletteTex);
	glUniform2i(uniforms[GBA_GL_VS_LOC], 1, y);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glUniform1i(uniforms[GBA_GL_BG_VRAM], 0);
	glUniform1i(uniforms[GBA_GL_BG_PALETTE], 1);
	glUniform1i(uniforms[GBA_GL_BG_SCREENBASE], background->screenBase);
	glUniform1i(uniforms[GBA_GL_BG_CHARBASE], background->charBase);
	glUniform1i(uniforms[GBA_GL_BG_SIZE], background->size);
	glUniform2i(uniforms[GBA_GL_BG_OFFSET], background->x, yBase - y);
	glUniform4i(uniforms[GBA_GL_BG_INFLAGS], (background->priority << 3) + (background->index << 1) + 1,
		                                     background->target1 | (background->target2 * 2) | (renderer->blendEffect * 4),
		                                     renderer->blda, renderer->bldb);
	glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void GBAVideoGLRendererDrawBackgroundMode2(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	int reverse;
	int forward;
	switch (y - renderer->firstAffine) {
	case 0:
	case 1:
	case 2:
	case 3:
		return;
	case 4:
		forward = 2;
		reverse = 4;
		break;
	case 5:
		forward = 2;
		reverse = 3;
		break;
	case 6:
		forward = 2;
		reverse = 2;
		break;
	default:
		forward = 1;
		reverse = 1;
	}

	const struct GBAVideoGLShader* shader = &renderer->bgShader[background->overflow ? 2 : 3];
	const GLuint* uniforms = shader->uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glScissor(0, (y - reverse) * renderer->scale, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, renderer->scale * forward);
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->vramTex);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, renderer->paletteTex);
	glUniform2i(uniforms[GBA_GL_VS_LOC], forward, y - reverse);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glUniform1i(uniforms[GBA_GL_BG_VRAM], 0);
	glUniform1i(uniforms[GBA_GL_BG_PALETTE], 1);
	glUniform1i(uniforms[GBA_GL_BG_SCREENBASE], background->screenBase);
	glUniform1i(uniforms[GBA_GL_BG_CHARBASE], background->charBase);
	glUniform1i(uniforms[GBA_GL_BG_SIZE], background->size);
	glUniform2f(uniforms[GBA_GL_BG_RANGE], y, 1);
	glUniform4i(uniforms[GBA_GL_BG_INFLAGS], (background->priority << 3) + (background->index << 1) + 1,
		                                     background->target1 | (background->target2 * 2) | (renderer->blendEffect * 4),
		                                     renderer->blda, renderer->bldb);
	if (renderer->scale > 1) {
		glUniform2iv(uniforms[GBA_GL_BG_OFFSET], 4, (GLint[]) {
			background->affine[0].sx, background->affine[0].sy,
			background->affine[1].sx, background->affine[1].sy,
			background->affine[2].sx, background->affine[2].sy,
			background->affine[3].sx, background->affine[3].sy,
		});
		glUniform2iv(uniforms[GBA_GL_BG_TRANSFORM], 4, (GLint[]) {
			background->affine[0].dx, background->affine[0].dy,
			background->affine[1].dx, background->affine[1].dy,
			background->affine[2].dx, background->affine[2].dy,
			background->affine[3].dx, background->affine[3].dy,
		});
	} else {
		glUniform2iv(uniforms[GBA_GL_BG_OFFSET], 4, (GLint[]) {
			background->affine[0].sx, background->affine[0].sy,
			background->affine[0].sx, background->affine[0].sy,
			background->affine[0].sx, background->affine[0].sy,
			background->affine[0].sx, background->affine[0].sy,
		});
		glUniform2iv(uniforms[GBA_GL_BG_TRANSFORM], 4, (GLint[]) {
			background->affine[0].dx, background->affine[0].dy,
			background->affine[0].dx, background->affine[0].dy,
			background->affine[0].dx, background->affine[0].dy,
			background->affine[0].dx, background->affine[0].dy,
		});
	}
	glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

static void _clearWindow(GBAWindowControl window, int start, int end, int y, int scale, int bldy) {
	if (start > end) {
		_clearWindow(window, start, GBA_VIDEO_HORIZONTAL_PIXELS, y, scale, bldy);
		_clearWindow(window, 0, end, y, scale, bldy);
		return;
	}
	glScissor(start * scale, y, (end - start) * scale, scale);
	window = ~window & 0xFF;
	glClearColor((window & 0xF) / 32.f, (window >> 4) / 32.f, bldy / 16.f, 0);
	glClear(GL_COLOR_BUFFER_BIT);
}

void GBAVideoGLRendererDrawWindow(struct GBAVideoGLRenderer* renderer, int y) {
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo[GBA_GL_FBO_WINDOW]);
	int dispcnt = ((renderer->dispcnt >> 8) & 0x1F) | 0x20;
	if (!(renderer->dispcnt & 0xE000)) {
		_clearWindow(dispcnt, 0, GBA_VIDEO_HORIZONTAL_PIXELS, y * renderer->scale, renderer->scale, renderer->bldy);
	} else {
		_clearWindow(renderer->winout & dispcnt, 0, GBA_VIDEO_HORIZONTAL_PIXELS, y * renderer->scale, renderer->scale, renderer->bldy);
		if (GBARegisterDISPCNTIsWin1Enable(renderer->dispcnt) && y >= renderer->winN[1].v.start && y < renderer->winN[1].v.end) {
			_clearWindow(renderer->winN[1].control & dispcnt, renderer->winN[1].h.start, renderer->winN[1].h.end, y * renderer->scale, renderer->scale, renderer->bldy);
		}
		if (GBARegisterDISPCNTIsWin0Enable(renderer->dispcnt) && y >= renderer->winN[0].v.start && y < renderer->winN[0].v.end) {
			_clearWindow(renderer->winN[0].control & dispcnt, renderer->winN[0].h.start, renderer->winN[0].h.end, y * renderer->scale, renderer->scale, renderer->bldy);
		}
	}
}

#endif
