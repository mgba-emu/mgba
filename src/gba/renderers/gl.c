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

static void _cleanRegister(struct GBAVideoGLRenderer* renderer, int address, uint16_t value);
static void _drawScanlines(struct GBAVideoGLRenderer* renderer, int lastY);
static void _finalizeLayers(struct GBAVideoGLRenderer* renderer);

#define TEST_LAYER_ENABLED(X) !glRenderer->d.disableBG[X] && glRenderer->bg[X].enabled == 4

struct GBAVideoGLUniform {
	const char* name;
	int type;
};

#define PALETTE_ENTRY "#define PALETTE_ENTRY(x) (vec3((ivec3(0x1F, 0x3E0, 0x7C00) & (x)) >> ivec3(0, 5, 10)) / 31.)\n"

static const GLchar* const _gles3Header =
	"#version 300 es\n"
	"#define OUT(n) layout(location = n)\n"
	PALETTE_ENTRY
	"precision highp float;\n"
	"precision highp int;\n"
	"precision highp sampler2D;\n"
	"precision highp isampler2D;\n";

static const GLchar* const _gl3Header =
	"#version 150 core\n"
	"#define OUT(n)\n"
	PALETTE_ENTRY
	"precision highp float;\n";

static const char* const _vertexShader =
	"in vec2 position;\n"
	"uniform ivec2 loc;\n"
	"uniform ivec2 maxPos;\n"
	"out vec2 texCoord;\n"

	"void main() {\n"
	"	vec2 local = vec2(position.x, float(position.y * float(loc.x) + float(loc.y)) / float(maxPos.y));\n"
	"	gl_Position = vec4((local * 2. - 1.) * vec2(sign(maxPos)), 0., 1.);\n"
	"	texCoord = local * vec2(abs(maxPos));\n"
	"}";

static const char* const _renderTile16 =
	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord) {\n"
	"	int address = charBase + tile * 16 + (localCoord.x >> 2) + (localCoord.y << 1);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - (localCoord.x & 3)] * 15.9);\n"
	"	if (entry == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	int paletteEntry = palette[paletteId * 16 + entry];\n"
	"	vec4 color = vec4(PALETTE_ENTRY(paletteEntry), 1.);\n"
	"	return color;\n"
	"}";

static const char* const _renderTile256 =
	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord) {\n"
	"	int address = charBase + tile * 32 + (localCoord.x >> 1) + (localCoord.y << 2);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - 2 * (localCoord.x & 1)] * 15.9);\n"
	"	int pal2 = int(halfrow[2 - 2 * (localCoord.x & 1)] * 15.9);\n"
	"	if ((pal2 | entry) == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	int paletteEntry = palette[pal2 * 16 + entry];\n"
	"	vec4 color = vec4(PALETTE_ENTRY(paletteEntry), 1.);\n"
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
	{ "mosaic", GBA_GL_BG_MOSAIC, },
	{ 0 }
};

static const char* const _renderMode0 =
	"in vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform int palette[256];\n"
	"uniform int screenBase;\n"
	"uniform int charBase;\n"
	"uniform int size;\n"
	"uniform int offset[160];\n"
	"uniform ivec4 inflags;\n"
	"uniform ivec2 mosaic;\n"
	"OUT(0) out vec4 color;\n"
	"OUT(1) out ivec4 flags;\n"

	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord);\n"

	"void main() {\n"
	"	ivec2 coord = ivec2(texCoord);\n"
	"	if (mosaic.x > 1) {\n"
	"		coord.x -= coord.x % mosaic.x;\n"
	"	}\n"
	"	if (mosaic.y > 1) {\n"
	"		coord.y -= coord.y % mosaic.y;\n"
	"	}\n"
	"	coord += (ivec2(0x1FF, 0x1FF000) & offset[int(texCoord.y)]) >> ivec2(0, 12);\n"
	"	ivec2 wrap = ivec2(255, 255);\n"
	"	int doty = 0;\n"
	"	if ((size & 1) == 1) {\n"
	"		wrap.x = 511;\n"
	"		++doty;\n"
	"	}\n"
	"	if ((size & 2) == 2) {\n"
	"		wrap.y = 511;\n"
	"		++doty;\n"
	"	}\n"
	"	coord &= wrap;\n"
	"	wrap = coord & 256;\n"
	"	coord &= 255;\n"
	"	coord.y += wrap.x + wrap.y * doty;\n"
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
	"	flags = inflags;\n"
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
	{ "mosaic", GBA_GL_BG_MOSAIC, },
	{ 0 }
};

static const char* const _interpolate =
	"vec2 interpolate(ivec2 arr[4], float x) {\n"
	"	float x1m = 1. - x;\n"
	"	return x1m * x1m * x1m * vec2(arr[0]) +"
		" 3. * x1m * x1m * x   * vec2(arr[1]) +"
		" 3. * x1m * x   * x   * vec2(arr[2]) +"
		"      x   * x   * x   * vec2(arr[3]);\n"
	"}\n"

	"void loadAffine(int y, out ivec2 mat[4], out ivec2 aff[4]) {\n"
	"	int start = max(range.x, y - 3);\n"
	"	mat[0] = transform[start + 0].xy;\n"
	"	aff[0] = transform[start + 0].zw;\n"
	"	mat[1] = transform[start + 1].xy;\n"
	"	aff[1] = transform[start + 1].zw;\n"
	"	mat[2] = transform[start + 2].xy;\n"
	"	aff[2] = transform[start + 2].zw;\n"
	"	mat[3] = transform[start + 3].xy;\n"
	"	aff[3] = transform[start + 3].zw;\n"
	"}\n";

static const char* const _renderMode2 =
	"in vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform int palette[256];\n"
	"uniform int screenBase;\n"
	"uniform int charBase;\n"
	"uniform int size;\n"
	"uniform ivec4 inflags;\n"
	"uniform ivec4 transform[160];\n"
	"uniform ivec2 range;\n"
	"uniform ivec2 mosaic;\n"
	"OUT(0) out vec4 color;\n"
	"OUT(1) out ivec4 flags;\n"

	"vec4 fetchTile(ivec2 coord);\n"
	"vec2 interpolate(ivec2 arr[4], float x);\n"
	"void loadAffine(int y, out ivec2 mat[4], out ivec2 aff[4]);\n"

	"vec4 renderTile(ivec2 coord) {\n"
	"	int map = (coord.x >> 11) + (((coord.y >> 7) & 0x7F0) << size);\n"
	"	int mapAddress = screenBase + (map >> 1);\n"
	"	vec4 twomaps = texelFetch(vram, ivec2(mapAddress & 255, mapAddress >> 8), 0);\n"
	"	int tile = int(twomaps[3 - 2 * (map & 1)] * 15.9) + int(twomaps[2 - 2 * (map & 1)] * 15.9) * 16;\n"
	"	int address = charBase + tile * 32 + ((coord.x >> 9) & 3) + ((coord.y >> 6) & 0x1C);\n"
	"	vec4 halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0);\n"
	"	int entry = int(halfrow[3 - ((coord.x >> 7) & 2)] * 15.9);\n"
	"	int pal2 = int(halfrow[2 - ((coord.x >> 7) & 2)] * 15.9);\n"
	"	if ((pal2 | entry) == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	int paletteEntry = palette[pal2 * 16 + entry];\n"
	"	vec4 color = vec4(PALETTE_ENTRY(paletteEntry), 1.);\n"
	"	return color;\n"
	"}\n"

	"void main() {\n"
	"	ivec2 mat[4];\n"
	"	ivec2 offset[4];\n"
	"	vec2 incoord = texCoord;\n"
	"	if (mosaic.x > 1) {\n"
	"		incoord.x = floor(incoord.x - float(int(incoord.x) % mosaic.x));\n"
	"	}\n"
	"	if (mosaic.y > 1) {\n"
	"		incoord.y = floor(incoord.y - float(int(incoord.y) % mosaic.y));\n"
	"	}\n"
	"	loadAffine(int(incoord.y), mat, offset);\n"
	"	float y = fract(incoord.y);\n"
	"	float start = 0.75;\n"
	"	if (int(incoord.y) - range.x < 4) {\n"
	"		y = incoord.y - float(range.x);\n"
	"		start = 0.;\n"
	"	}\n"
	"	float lin = start + y * 0.25;\n"
	"	vec2 mixedTransform = interpolate(mat, lin);\n"
	"	vec2 mixedOffset = interpolate(offset, lin);\n"
	"	color = fetchTile(ivec2(mixedTransform * incoord.x + mixedOffset));\n"
	"	flags = inflags;\n"
	"}";

static const struct GBAVideoGLUniform _uniformsMode35[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "vram", GBA_GL_BG_VRAM, },
	{ "charBase", GBA_GL_BG_CHARBASE, },
	{ "size", GBA_GL_BG_SIZE, },
	{ "inflags", GBA_GL_BG_INFLAGS, },
	{ "offset", GBA_GL_BG_OFFSET, },
	{ "transform", GBA_GL_BG_TRANSFORM, },
	{ "range", GBA_GL_BG_RANGE, },
	{ "mosaic", GBA_GL_BG_MOSAIC, },
	{ 0 }
};

static const char* const _renderMode35 =
	"in vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform int charBase;\n"
	"uniform ivec2 size;\n"
	"uniform ivec4 inflags;\n"
	"uniform ivec4 transform[160];\n"
	"uniform ivec2 range;\n"
	"uniform ivec2 mosaic;\n"
	"OUT(0) out vec4 color;\n"
	"OUT(1) out ivec4 flags;\n"

	"vec2 interpolate(ivec2 arr[4], float x);\n"
	"void loadAffine(int y, out ivec2 mat[4], out ivec2 aff[4]);\n"

	"void main() {\n"
	"	ivec2 mat[4];\n"
	"	ivec2 offset[4];\n"
	"	vec2 incoord = texCoord;\n"
	"	if (mosaic.x > 1) {\n"
	"		incoord.x = floor(incoord.x - float(int(incoord.x) % mosaic.x));\n"
	"	}\n"
	"	if (mosaic.y > 1) {\n"
	"		incoord.y = floor(incoord.y - float(int(incoord.y) % mosaic.y));\n"
	"	}\n"
	"	loadAffine(int(incoord.y), mat, offset);\n"
	"	float y = fract(incoord.y);\n"
	"	float start = 0.75;\n"
	"	if (int(incoord.y) - range.x < 4) {\n"
	"		y = incoord.y - float(range.x);\n"
	"		start = 0.;\n"
	"	}\n"
	"	float lin = start + y * 0.25;\n"
	"	vec2 mixedTransform = interpolate(mat, lin);\n"
	"	vec2 mixedOffset = interpolate(offset, lin);\n"
	"	ivec2 coord = ivec2(mixedTransform * incoord.x + mixedOffset);\n"
	"	if (coord.x < 0 || coord.x >= (size.x << 8)) {\n"
	"		discard;\n"
	"	}\n"
	"	if (coord.y < 0 || coord.y >= (size.y << 8)) {\n"
	"		discard;\n"
	"	}\n"
	"	int address = charBase + (coord.x >> 8) + (coord.y >> 8) * size.x;\n"
	"	ivec4 entry = ivec4(texelFetch(vram, ivec2(address & 255, address >> 8), 0) * 15.9);\n"
	"	int sixteen = (entry.x << 12) | (entry.y << 8) | (entry.z << 4) | entry.w;\n"
	"	color = vec4(float(sixteen & 0x1F) / 31., float((sixteen >> 5) & 0x1F) / 31., float((sixteen >> 10) & 0x1F) / 31., 1.);\n"
	"	flags = inflags;\n"
	"}";

static const struct GBAVideoGLUniform _uniformsMode4[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "vram", GBA_GL_BG_VRAM, },
	{ "palette", GBA_GL_BG_PALETTE, },
	{ "charBase", GBA_GL_BG_CHARBASE, },
	{ "size", GBA_GL_BG_SIZE, },
	{ "inflags", GBA_GL_BG_INFLAGS, },
	{ "offset", GBA_GL_BG_OFFSET, },
	{ "transform", GBA_GL_BG_TRANSFORM, },
	{ "range", GBA_GL_BG_RANGE, },
	{ "mosaic", GBA_GL_BG_MOSAIC, },
	{ 0 }
};

static const char* const _renderMode4 =
	"in vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform int palette[256];\n"
	"uniform int charBase;\n"
	"uniform ivec2 size;\n"
	"uniform ivec4 inflags;\n"
	"uniform ivec4 transform[160];\n"
	"uniform ivec2 range;\n"
	"uniform ivec2 mosaic;\n"
	"OUT(0) out vec4 color;\n"
	"OUT(1) out ivec4 flags;\n"

	"vec2 interpolate(ivec2 arr[4], float x);\n"
	"void loadAffine(int y, out ivec2 mat[4], out ivec2 aff[4]);\n"

	"void main() {\n"
	"	ivec2 mat[4];\n"
	"	ivec2 offset[4];\n"
	"	vec2 incoord = texCoord;\n"
	"	if (mosaic.x > 1) {\n"
	"		incoord.x = floor(incoord.x - float(int(incoord.x) % mosaic.x));\n"
	"	}\n"
	"	if (mosaic.y > 1) {\n"
	"		incoord.y = floor(incoord.y - float(int(incoord.y) % mosaic.y));\n"
	"	}\n"
	"	loadAffine(int(incoord.y), mat, offset);\n"
	"	float y = fract(incoord.y);\n"
	"	float start = 0.75;\n"
	"	if (int(incoord.y) - range.x < 4) {\n"
	"		y = incoord.y - float(range.x);\n"
	"		start = 0.;\n"
	"	}\n"
	"	float lin = start + y * 0.25;\n"
	"	vec2 mixedTransform = interpolate(mat, lin);\n"
	"	vec2 mixedOffset = interpolate(offset, lin);\n"
	"	ivec2 coord = ivec2(mixedTransform * incoord.x + mixedOffset);\n"
	"	if (coord.x < 0 || coord.x >= (size.x << 8)) {\n"
	"		discard;\n"
	"	}\n"
	"	if (coord.y < 0 || coord.y >= (size.y << 8)) {\n"
	"		discard;\n"
	"	}\n"
	"	int address = charBase + (coord.x >> 8) + (coord.y >> 8) * size.x;\n"
	"	vec4 twoEntries = texelFetch(vram, ivec2((address >> 1) & 255, address >> 9), 0);\n"
	"	ivec2 entry = ivec2(twoEntries[3 - 2 * (address & 1)] * 15.9, twoEntries[2 - 2 * (address & 1)] * 15.9);\n"
	"	int paletteEntry = palette[entry.y * 16 + entry.x];\n"
	"	color = vec4(PALETTE_ENTRY(paletteEntry), 1.);\n"
	"	flags = inflags;\n"
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
	{ "mosaic", GBA_GL_OBJ_MOSAIC, },
	{ 0 }
};

static const char* const _renderObj =
	"in vec2 texCoord;\n"
	"uniform sampler2D vram;\n"
	"uniform int palette[256];\n"
	"uniform int charBase;\n"
	"uniform int stride;\n"
	"uniform int localPalette;\n"
	"uniform ivec4 inflags;\n"
	"uniform mat2x2 transform;\n"
	"uniform ivec4 dims;\n"
	"uniform ivec4 objwin;\n"
	"uniform ivec4 mosaic;\n"
	"OUT(0) out vec4 color;\n"
	"OUT(1) out ivec4 flags;\n"
	"OUT(2) out ivec4 window;\n"

	"vec4 renderTile(int tile, int paletteId, ivec2 localCoord);\n"

	"void main() {\n"
	"	vec2 incoord = texCoord;\n"
	"	if (mosaic.x > 1) {\n"
	"		int x = int(incoord.x);\n"
	"		incoord.x = float(clamp(x - (mosaic.z + x) % mosaic.x, 0, dims.z - 1));\n"
	"	} else if (mosaic.x < -1) {\n"
	"		int x = dims.z - int(incoord.x) - 1;\n"
	"		incoord.x = float(clamp(dims.z - x + (mosaic.z + x) % -mosaic.x - 1, 0, dims.z - 1));\n"
	"	}\n"
	"	if (mosaic.y > 1) {\n"
	"		int y = int(incoord.y);\n"
	"		incoord.y = float(clamp(y - (mosaic.w + y) % mosaic.y, 0, dims.w - 1));\n"
	"	}\n"
	"	ivec2 coord = ivec2(transform * (incoord - vec2(dims.zw) / 2.) + vec2(dims.xy) / 2.);\n"
	"	if ((coord & ~(dims.xy - 1)) != ivec2(0, 0)) {\n"
	"		discard;\n"
	"	}\n"
	"	vec4 pix = renderTile((coord.x >> 3) + (coord.y >> 3) * stride, localPalette, coord & 7);\n"
	"	color = pix;\n"
	"	flags = inflags;\n"
	"	gl_FragDepth = float(flags.x) / 16.;\n"
	"	window = ivec4(objwin.yzw, 0);\n"
	"}";

static const struct GBAVideoGLUniform _uniformsObjPriority[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "inflags", GBA_GL_OBJ_INFLAGS, },
	{ 0 }
};

static const char* const _renderObjPriority =
	"in vec2 texCoord;\n"
	"uniform ivec4 inflags;\n"
	"OUT(0) out vec4 color;\n"
	"OUT(1) out ivec4 flags;\n"

	"void main() {\n"
	"	flags = inflags;\n"
	"	gl_FragDepth = float(flags.x) / 16.;\n"
	"	color = vec4(0., 0., 0., 0.);"
	"}";

static const struct GBAVideoGLUniform _uniformsWindow[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "dispcnt", GBA_GL_WIN_DISPCNT, },
	{ "blend", GBA_GL_WIN_BLEND, },
	{ "flags", GBA_GL_WIN_FLAGS, },
	{ "win0", GBA_GL_WIN_WIN0, },
	{ "win1", GBA_GL_WIN_WIN1, },
	{ 0 }
};

static const char* const _renderWindow =
	"in vec2 texCoord;\n"
	"uniform int dispcnt;\n"
	"uniform ivec2 blend;\n"
	"uniform ivec3 flags;\n"
	"uniform ivec4 win0[160];\n"
	"uniform ivec4 win1[160];\n"
	"OUT(0) out ivec4 window;\n"

	"void crop(vec4 windowParams, int flags, inout ivec3 windowFlags) {\n"
	"	bvec4 compare = lessThan(texCoord.xxyy, windowParams);\n"
	"	compare = equal(compare, bvec4(true, false, true, false));\n"
	"	if (any(compare)) {\n"
	"		vec2 h = windowParams.xy;\n"
	"		vec2 v = windowParams.zw;\n"
	"		if (v.x > v.y) {\n"
	"			if (compare.z && compare.w) {\n"
	"				return;\n"
	"			}\n"
	"		} else if (compare.z || compare.w) {\n"
	"			return;\n"
	"		}\n"
	"		if (h.x > h.y) {\n"
	"			if (compare.x && compare.y) {\n"
	"				return;\n"
	"			}\n"
	"		} else if (compare.x || compare.y) {\n"
	"			return;\n"
	"		}\n"
	"	}\n"
	"	windowFlags.x = flags;\n"
	"}\n"

	"vec4 interpolate(ivec4 win[160]) {\n"
	"	vec4 bottom = vec4(win[int(texCoord.y) - 1]);\n"
	"	vec4 top = vec4(win[int(texCoord.y)]);\n"
	"	if (distance(top, bottom) > 40.) {\n"
	"		return top;\n"
	"	}\n"
	"	return vec4(mix(bottom.xy, top.xy, fract(texCoord.y)), top.zw);\n"
	"}\n"

	"void main() {\n"
	"	int dispflags = (dispcnt & 0x1F) | 0x20;\n"
	"	if ((dispcnt & 0xE0) == 0) {\n"
	"		window = ivec4(dispflags, blend, 0);\n"
	"	} else {\n"
	"		ivec3 windowFlags = ivec3(flags.z, blend);\n"
	"		if ((dispcnt & 0x40) != 0) { \n"
	"			crop(interpolate(win1), flags.y, windowFlags);\n"
	"		}\n"
	"		if ((dispcnt & 0x20) != 0) { \n"
	"			crop(interpolate(win0), flags.x, windowFlags);\n"
	"		}\n"
	"		window = ivec4(windowFlags, 0);\n"
	"	}\n"
	"}\n";

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
	"uniform isampler2D flags[5];\n"
	"uniform isampler2D window;\n"
	"uniform sampler2D backdrop;\n"
	"uniform isampler2D backdropFlags;\n"
	"out vec4 color;\n"

	"void composite(vec4 pixel, ivec4 flags, inout vec4 topPixel, inout ivec4 topFlags, inout vec4 bottomPixel, inout ivec4 bottomFlags) {\n"
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
	"	vec4 topPixel = texelFetch(backdrop, ivec2(0, texCoord.y), 0);\n"
	"	vec4 bottomPixel = topPixel;\n"
	"	ivec4 topFlags = ivec4(texelFetch(backdropFlags, ivec2(0, texCoord.y), 0));\n"
	"	ivec4 bottomFlags = topFlags;\n"
	"	ivec2 coord = ivec2(texCoord * float(scale));\n"
	"	ivec4 windowFlags = texelFetch(window, coord, 0);\n"
	"	int layerWindow = windowFlags.x;\n"
	"	if ((layerWindow & 16) != 0) {\n"
	"		vec4 pix = texelFetch(layers[4], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(flags[4], coord, 0));\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 1) != 0) {\n"
	"		vec4 pix = texelFetch(layers[0], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(flags[0], coord, 0).xyz, 0);\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 2) != 0) {\n"
	"		vec4 pix = texelFetch(layers[1], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(flags[1], coord, 0).xyz, 0);\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 4) != 0) {\n"
	"		vec4 pix = texelFetch(layers[2], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(flags[2], coord, 0).xyz, 0);\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 8) != 0) {\n"
	"		vec4 pix = texelFetch(layers[3], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(flags[3], coord, 0).xyz, 0);\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 32) == 0) {\n"
	"		topFlags.y &= ~1;\n"
	"	}\n"
	"	if (((topFlags.y & 13) == 5 || topFlags.w > 0) && (bottomFlags.y & 2) == 2) {\n"
	"		topPixel.rgb *= float(topFlags.z) / 16.;\n"
	"		topPixel.rgb += bottomPixel.rgb * float(windowFlags.y) / 16.;\n"
	"	} else if ((topFlags.y & 13) == 9) {\n"
	"		topPixel.rgb += (1. - topPixel.rgb) * float(windowFlags.z) / 16.;\n"
	"	} else if ((topFlags.y & 13) == 13) {\n"
	"		topPixel.rgb -= topPixel.rgb * float(windowFlags.z) / 16.;\n"
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

	renderer->d.highlightBG[0] = false;
	renderer->d.highlightBG[1] = false;
	renderer->d.highlightBG[2] = false;
	renderer->d.highlightBG[3] = false;
	int i;
	for (i = 0; i < 128; ++i) {
		renderer->d.highlightOBJ[i] = false;
	}
	renderer->d.highlightColor = 0xFFFFFF;
	renderer->d.highlightAmount = 0;

	renderer->scale = 1;
}

static void _compileShader(struct GBAVideoGLRenderer* glRenderer, struct GBAVideoGLShader* shader, const char** shaderBuffer, int shaderBufferLines, GLuint vs, const struct GBAVideoGLUniform* uniforms, const char* const* outFrags, char* log) {
	GLuint program = glCreateProgram();
	shader->program = program;

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glShaderSource(fs, shaderBufferLines, shaderBuffer, 0);
	glCompileShader(fs);
	glGetShaderInfoLog(fs, 2048, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Fragment shader compilation failure: %s", log);
	}
	size_t i;
#ifndef BUILD_GLES3
	for (i = 0; outFrags[i]; ++i) {
		glBindFragDataLocation(program, i, outFrags[i]);
	}
#else
	UNUSED(outFrags);
#endif
	glLinkProgram(program);
	glGetProgramInfoLog(program, 2048, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Program link failure: %s", log);
	}
	glDeleteShader(fs);

	glGenVertexArrays(1, &shader->vao);
	glBindVertexArray(shader->vao);
	glBindBuffer(GL_ARRAY_BUFFER, glRenderer->vbo);
	GLuint positionLocation = glGetAttribLocation(program, "position");
	glEnableVertexAttribArray(positionLocation);
	glVertexAttribPointer(positionLocation, 2, GL_INT, GL_FALSE, 0, NULL);

	for (i = 0; uniforms[i].name; ++i) {
		shader->uniforms[uniforms[i].type] = glGetUniformLocation(program, uniforms[i].name);
	}
}

static void _deleteShader(struct GBAVideoGLShader* shader) {
	glDeleteProgram(shader->program);
	glDeleteVertexArrays(1, &shader->vao);
}

static void _initFramebufferTextureEx(GLuint tex, GLenum internalFormat, GLenum format, GLenum type, GLenum attachment, int scale) {
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, scale > 0 ? GBA_VIDEO_HORIZONTAL_PIXELS * scale : 1, GBA_VIDEO_VERTICAL_PIXELS * (scale > 0 ? scale : 1), 0, format, type, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex, 0);	
}

static void _initFramebufferTexture(GLuint tex, GLenum format, GLenum attachment, int scale) {
	_initFramebufferTextureEx(tex, format, format, GL_UNSIGNED_BYTE, attachment, scale);
}

static void _initFramebuffers(struct GBAVideoGLRenderer* glRenderer) {
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OBJ]);
	_initFramebufferTexture(glRenderer->layers[GBA_GL_TEX_OBJ_COLOR], GL_RGBA, GL_COLOR_ATTACHMENT0, glRenderer->scale);
	_initFramebufferTextureEx(glRenderer->layers[GBA_GL_TEX_OBJ_FLAGS], GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, GL_COLOR_ATTACHMENT1, glRenderer->scale);
	_initFramebufferTextureEx(glRenderer->layers[GBA_GL_TEX_WINDOW], GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, GL_COLOR_ATTACHMENT2, glRenderer->scale);
	_initFramebufferTextureEx(glRenderer->layers[GBA_GL_TEX_OBJ_DEPTH], GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL_ATTACHMENT, glRenderer->scale);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_BACKDROP]);
	_initFramebufferTexture(glRenderer->layers[GBA_GL_TEX_BACKDROP_COLOR], GL_RGB, GL_COLOR_ATTACHMENT0, 0);
	_initFramebufferTextureEx(glRenderer->layers[GBA_GL_TEX_BACKDROP_FLAGS], GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, GL_COLOR_ATTACHMENT1, glRenderer->scale);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_WINDOW]);
	_initFramebufferTextureEx(glRenderer->layers[GBA_GL_TEX_WINDOW], GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, GL_COLOR_ATTACHMENT0, glRenderer->scale);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OUTPUT]);
	_initFramebufferTexture(glRenderer->outputTex, GL_RGB, GL_COLOR_ATTACHMENT0, glRenderer->scale);

	int i;
	for (i = 0; i < 4; ++i) {
		struct GBAVideoGLBackground* bg = &glRenderer->bg[i];
		glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
		_initFramebufferTexture(bg->tex, GL_RGBA, GL_COLOR_ATTACHMENT0, glRenderer->scale);
		_initFramebufferTextureEx(bg->flags, GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, GL_COLOR_ATTACHMENT1, glRenderer->scale);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAVideoGLRendererInit(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->temporaryBuffer = NULL;

	glGenFramebuffers(GBA_GL_FBO_MAX, glRenderer->fbo);
	glGenTextures(GBA_GL_TEX_MAX, glRenderer->layers);

	glGenTextures(1, &glRenderer->vramTex);
	glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 256, 192, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0);

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
		bg->affine.dx = 256;
		bg->affine.dmx = 0;
		bg->affine.dy = 0;
		bg->affine.dmy = 256;
		bg->affine.sx = 0;
		bg->affine.sy = 0;
		glGenFramebuffers(1, &bg->fbo);
		glGenTextures(1, &bg->tex);
		glGenTextures(1, &bg->flags);
	}

	_initFramebuffers(glRenderer);

	char log[2048];
	const GLchar* shaderBuffer[4];
	const GLubyte* version = glGetString(GL_VERSION);
	if (strncmp((const char*) version, "OpenGL ES ", strlen("OpenGL ES "))) {
		shaderBuffer[0] = _gl3Header;
	} else {
		shaderBuffer[0] = _gles3Header;
	}

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	shaderBuffer[1] = _vertexShader;
	glShaderSource(vs, 2, shaderBuffer, 0);
	glCompileShader(vs);
	glGetShaderInfoLog(vs, 2048, 0, log);
	if (log[0]) {
		mLOG(GBA_VIDEO, ERROR, "Vertex shader compilation failure: %s", log);
	}

	const char* const noWindow[] = {"color", "flags", NULL};
	const char* const window[] = {"color", "flags", "window", NULL};
	const char* const onlyWindow[] = {"window", NULL};
	const char* const onlyColor[] = {"color", NULL};

	shaderBuffer[1] = _renderMode0;

	shaderBuffer[2] = _renderTile16;
	_compileShader(glRenderer, &glRenderer->bgShader[0], shaderBuffer, 3, vs, _uniformsMode0, noWindow, log);

	shaderBuffer[2] = _renderTile256;
	_compileShader(glRenderer, &glRenderer->bgShader[1], shaderBuffer, 3, vs, _uniformsMode0, noWindow, log);

	shaderBuffer[1] = _renderMode2;
	shaderBuffer[2] = _interpolate;

	shaderBuffer[3] = _fetchTileOverflow;
	_compileShader(glRenderer, &glRenderer->bgShader[2], shaderBuffer, 4, vs, _uniformsMode2, noWindow, log);

	shaderBuffer[3] = _fetchTileNoOverflow;
	_compileShader(glRenderer, &glRenderer->bgShader[3], shaderBuffer, 4, vs, _uniformsMode2, noWindow, log);

	shaderBuffer[1] = _renderMode4;
	shaderBuffer[2] = _interpolate;
	_compileShader(glRenderer, &glRenderer->bgShader[4], shaderBuffer, 3, vs, _uniformsMode4, noWindow, log);

	shaderBuffer[1] = _renderMode35;
	shaderBuffer[2] = _interpolate;
	_compileShader(glRenderer, &glRenderer->bgShader[5], shaderBuffer, 3, vs, _uniformsMode35, noWindow, log);

	shaderBuffer[1] = _renderObj;

	shaderBuffer[2] = _renderTile16;
	_compileShader(glRenderer, &glRenderer->objShader[0], shaderBuffer, 3, vs, _uniformsObj, window, log);

	shaderBuffer[2] = _renderTile256;
	_compileShader(glRenderer, &glRenderer->objShader[1], shaderBuffer, 3, vs, _uniformsObj, window, log);

	shaderBuffer[1] = _renderObjPriority;
	_compileShader(glRenderer, &glRenderer->objShader[2], shaderBuffer, 2, vs, _uniformsObjPriority, noWindow, log);

	shaderBuffer[1] = _renderWindow;
	_compileShader(glRenderer, &glRenderer->windowShader, shaderBuffer, 2, vs, _uniformsWindow, onlyWindow, log);

	shaderBuffer[1] = _finalize;
	_compileShader(glRenderer, &glRenderer->finalizeShader, shaderBuffer, 2, vs, _uniformsFinalize, onlyColor, log);

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
	glDeleteTextures(1, &glRenderer->vramTex);
	glDeleteBuffers(1, &glRenderer->vbo);

	_deleteShader(&glRenderer->bgShader[0]);
	_deleteShader(&glRenderer->bgShader[1]);
	_deleteShader(&glRenderer->bgShader[2]);
	_deleteShader(&glRenderer->bgShader[3]);
	_deleteShader(&glRenderer->objShader[0]);
	_deleteShader(&glRenderer->objShader[1]);
	_deleteShader(&glRenderer->finalizeShader);

	int i;
	for (i = 0; i < 4; ++i) {
		struct GBAVideoGLBackground* bg = &glRenderer->bg[i];
		glDeleteFramebuffers(1, &bg->fbo);
		glDeleteTextures(1, &bg->tex);
		glDeleteTextures(1, &bg->flags);
	}
}

void GBAVideoGLRendererReset(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;

	glRenderer->oamDirty = true;
	glRenderer->paletteDirty = true;
	glRenderer->vramDirty = 0xFFFFFF;
	glRenderer->firstAffine = -1;
	glRenderer->firstY = -1;
	glRenderer->dispcnt = 0x0080;
	glRenderer->mosaic = 0;
	memset(glRenderer->shadowRegs, 0, sizeof(glRenderer->shadowRegs));
	glRenderer->regsDirty = 0xFFFFFFFFFFFEULL;
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
	UNUSED(address);
	UNUSED(value);
	glRenderer->paletteDirty = true;
}

uint16_t GBAVideoGLRendererWriteVideoRegister(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (renderer->cache) {
		GBAVideoCacheWriteVideoRegister(renderer->cache, address, value);
	}

	bool dirty = false;
	switch (address) {
	case REG_DISPCNT:
		value &= 0xFFF7;
		dirty = true;
		break;
	case REG_BG0CNT:
	case REG_BG1CNT:
		value &= 0xDFFF;
		dirty = true;
		break;
	case REG_BG0HOFS:
		value &= 0x01FF;
		glRenderer->bg[0].x = value;
		dirty = false;
		break;
	case REG_BG0VOFS:
		value &= 0x01FF;
		glRenderer->bg[0].y = value;
		dirty = false;
		break;
	case REG_BG1HOFS:
		value &= 0x01FF;
		glRenderer->bg[1].x = value;
		dirty = false;
		break;
	case REG_BG1VOFS:
		value &= 0x01FF;
		glRenderer->bg[1].y = value;
		dirty = false;
		break;
	case REG_BG2HOFS:
		value &= 0x01FF;
		glRenderer->bg[2].x = value;
		dirty = false;
		break;
	case REG_BG2VOFS:
		value &= 0x01FF;
		glRenderer->bg[2].y = value;
		dirty = false;
		break;
	case REG_BG3HOFS:
		value &= 0x01FF;
		glRenderer->bg[3].x = value;
		dirty = false;
		break;
	case REG_BG3VOFS:
		value &= 0x01FF;
		glRenderer->bg[3].y = value;
		dirty = false;
		break;
	case REG_BG2PA:
		glRenderer->bg[2].affine.dx = value;
		break;
	case REG_BG2PB:
		glRenderer->bg[2].affine.dmx = value;
		break;
	case REG_BG2PC:
		glRenderer->bg[2].affine.dy = value;
		break;
	case REG_BG2PD:
		glRenderer->bg[2].affine.dmy = value;
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
		glRenderer->bg[3].affine.dx = value;
		break;
	case REG_BG3PB:
		glRenderer->bg[3].affine.dmx = value;
		break;
	case REG_BG3PC:
		glRenderer->bg[3].affine.dy = value;
		break;
	case REG_BG3PD:
		glRenderer->bg[3].affine.dmy = value;
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
	case REG_BLDALPHA:
		value &= 0x1F1F;
		dirty = true;
		break;
	case REG_BLDY:
		value &= 0x1F;
		if (value > 0x10) {
			value = 0x10;
		}
		dirty = true;
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
	case REG_WINOUT:
		value &= 0x3F3F;
		dirty = true;
		break;
	default:
		dirty = true;
		break;
	}
	if (glRenderer->shadowRegs[address >> 1] == value) {
		dirty = false;
	} else {
		glRenderer->shadowRegs[address >> 1] = value;
	}
	if (dirty) {
		glRenderer->regsDirty |= 1ULL << (address >> 1);
	}
	return value;
}

void _cleanRegister(struct GBAVideoGLRenderer* glRenderer, int address, uint16_t value) {
	switch (address) {
	case REG_DISPCNT:
		glRenderer->dispcnt = value;
		GBAVideoGLRendererUpdateDISPCNT(glRenderer);
		break;
	case REG_BG0CNT:
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[0], value);
		break;
	case REG_BG1CNT:
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[1], value);
		break;
	case REG_BG2CNT:
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[2], value);
		break;
	case REG_BG3CNT:
		GBAVideoGLRendererWriteBGCNT(&glRenderer->bg[3], value);
		break;
	case REG_BLDCNT:
		GBAVideoGLRendererWriteBLDCNT(glRenderer, value);
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
		break;
	case REG_BLDY:
		glRenderer->bldy = value;
		break;
	case REG_WININ:
		glRenderer->winN[0].control = value;
		glRenderer->winN[1].control = value >> 8;
		break;
	case REG_WINOUT:
		glRenderer->winout = value;
		glRenderer->objwin = value >> 8;
		break;
	case REG_MOSAIC:
		glRenderer->mosaic = value;
		break;
	default:
		break;
	}
}

static bool _dirtyMode0(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	UNUSED(y);
	if (!background->enabled) {
		return false;
	}
	unsigned screenBase = background->screenBase >> 11; // Lops off one extra bit
	unsigned screenMask = (7 << screenBase) & 0xFFFF; // Technically overzealous
	if (renderer->vramDirty & screenMask) {
		return true;
	}
	unsigned charBase = background->charBase >> 11;
	unsigned charMask = (0xFFFF << charBase) & 0xFFFF;
	if (renderer->vramDirty & charMask) {
		return true;
	}
	return false;
}

static bool _dirtyMode2(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	UNUSED(y);
	if (!background->enabled) {
		return false;
	}
	unsigned screenBase = background->screenBase >> 11; // Lops off one extra bit
	unsigned screenMask = (0xF << screenBase) & 0xFFFF;
	if (renderer->vramDirty & screenMask) {
		return true;
	}
	unsigned charBase = background->charBase >> 11;
	unsigned charMask = (0x3FFF << charBase) & 0xFFFF;
	if (renderer->vramDirty & charMask) {
		return true;
	}
	return false;
}

static bool _dirtyMode3(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	UNUSED(y);
	if (!background->enabled) {
		return false;
	}
	if (renderer->vramDirty & 0xFFFFF) {
		return true;
	}
	return false;
}

static bool _dirtyMode45(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	UNUSED(y);
	if (!background->enabled) {
		return false;
	}
	int start = GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt) ? 5 : 0;
	int mask = 0x3FF << start;
	if (renderer->vramDirty & mask) {
		return true;
	}
	return false;
}

static bool _needsVramUpload(struct GBAVideoGLRenderer* renderer, int y) {
	if (!renderer->vramDirty) {
		return false;
	}
	if (y == 0) {
		return true;
	}

	if (GBARegisterDISPCNTIsObjEnable(renderer->dispcnt) && renderer->vramDirty & 0xFF0000) {
		return true;
	}

	bool dirty = false;
	switch (GBARegisterDISPCNTGetMode(renderer->dispcnt)) {
	case 0:
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[0], y);
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[1], y);
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[2], y);
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[3], y);
		break;
	case 1:
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[0], y);
		dirty = dirty || _dirtyMode0(renderer, &renderer->bg[1], y);
		dirty = dirty || _dirtyMode2(renderer, &renderer->bg[2], y);
		break;
	case 2:
		dirty = dirty || _dirtyMode2(renderer, &renderer->bg[2], y);
		dirty = dirty || _dirtyMode2(renderer, &renderer->bg[3], y);
		break;
	case 3:
		dirty = _dirtyMode3(renderer, &renderer->bg[2], y);
		break;
	case 4:
		dirty = _dirtyMode45(renderer, &renderer->bg[2], y);
		break;
	case 5:
		dirty = _dirtyMode45(renderer, &renderer->bg[2], y);
		break;
	}
	return dirty;
}

void GBAVideoGLRendererDrawScanline(struct GBAVideoRenderer* renderer, int y) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;

	if (GBARegisterDISPCNTGetMode(glRenderer->dispcnt) != 0) {
		if (glRenderer->firstAffine < 0) {
			glRenderer->firstAffine = y;
		}
	} else {
		glRenderer->firstAffine = -1;
	}

	if (glRenderer->paletteDirty || _needsVramUpload(glRenderer, y) || glRenderer->oamDirty || glRenderer->regsDirty) {
		if (glRenderer->firstY >= 0) {
			_drawScanlines(glRenderer, y - 1);
			glBindVertexArray(0);
		}
	}
	if (glRenderer->firstY < 0) {
		glRenderer->firstY = y;
	}

	int i;
	for (i = 0; i < 0x30; ++i) {
		if (!(glRenderer->regsDirty & (1ULL << i))) {
			continue;
		}
		_cleanRegister(glRenderer, i << 1, glRenderer->shadowRegs[i]);
	}
	glRenderer->regsDirty = 0;

	glRenderer->winNHistory[0][y * 4 + 0] = glRenderer->winN[0].h.start;
	glRenderer->winNHistory[0][y * 4 + 1] = glRenderer->winN[0].h.end;
	glRenderer->winNHistory[0][y * 4 + 2] = glRenderer->winN[0].v.start;
	glRenderer->winNHistory[0][y * 4 + 3] = glRenderer->winN[0].v.end;
	glRenderer->winNHistory[1][y * 4 + 0] = glRenderer->winN[1].h.start;
	glRenderer->winNHistory[1][y * 4 + 1] = glRenderer->winN[1].h.end;
	glRenderer->winNHistory[1][y * 4 + 2] = glRenderer->winN[1].v.start;
	glRenderer->winNHistory[1][y * 4 + 3] = glRenderer->winN[1].v.end;

	glRenderer->bg[0].scanlineOffset[y] = glRenderer->bg[0].x;
	glRenderer->bg[0].scanlineOffset[y] |= glRenderer->bg[0].y << 12;
	glRenderer->bg[1].scanlineOffset[y] = glRenderer->bg[1].x;
	glRenderer->bg[1].scanlineOffset[y] |= glRenderer->bg[1].y << 12;
	glRenderer->bg[2].scanlineOffset[y] = glRenderer->bg[2].x;
	glRenderer->bg[2].scanlineOffset[y] |= glRenderer->bg[2].y << 12;
	glRenderer->bg[2].scanlineAffine[y * 4] = glRenderer->bg[2].affine.dx;
	glRenderer->bg[2].scanlineAffine[y * 4 + 1] = glRenderer->bg[2].affine.dy;
	glRenderer->bg[2].scanlineAffine[y * 4 + 2] = glRenderer->bg[2].affine.sx;
	glRenderer->bg[2].scanlineAffine[y * 4 + 3] = glRenderer->bg[2].affine.sy;
	glRenderer->bg[3].scanlineOffset[y] = glRenderer->bg[3].x;
	glRenderer->bg[3].scanlineOffset[y] |= glRenderer->bg[3].y << 12;
	glRenderer->bg[3].scanlineAffine[y * 4] = glRenderer->bg[3].affine.dx;
	glRenderer->bg[3].scanlineAffine[y * 4 + 1] = glRenderer->bg[3].affine.dy;
	glRenderer->bg[3].scanlineAffine[y * 4 + 2] = glRenderer->bg[3].affine.sx;
	glRenderer->bg[3].scanlineAffine[y * 4 + 3] = glRenderer->bg[3].affine.sy;

	if (glRenderer->paletteDirty) {
		for (i = 0; i < 512; ++i) {
			glRenderer->shadowPalette[i] = glRenderer->d.palette[i];
		}
		glRenderer->paletteDirty = false;
	}

	if (_needsVramUpload(glRenderer, y)) {
		int first = -1;
		glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
		for (i = 0; i < 25; ++i) {
			if (!(glRenderer->vramDirty & (1 << i))) {
				if (first >= 0) {
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 8 * first, 256, 8 * (i - first), GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, &glRenderer->d.vram[2048 * first]);
					first = -1;
				}
			} else if (first < 0) {
				first = i;
			}
		}
		glRenderer->vramDirty = 0;
	}

	if (glRenderer->oamDirty) {
		glRenderer->oamMax = GBAVideoRendererCleanOAM(glRenderer->d.oam->obj, glRenderer->sprites, 0);
		glRenderer->oamDirty = false;
	}

	if (y == 0) {
		glDisable(GL_SCISSOR_TEST);
		glClearColor(0, 0, 0, 0);
#ifdef BUILD_GLES3
		glClearDepthf(1.f);
#else
		glClearDepth(1);
#endif
		glClearStencil(0);
		glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OBJ]);
		glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		for (i = 0; i < 4; ++i) {
			glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->bg[i].fbo);
			glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
			glClear(GL_COLOR_BUFFER_BIT);
		}
	}

	if (GBARegisterDISPCNTGetMode(glRenderer->dispcnt) != 0) {
		glRenderer->bg[2].affine.sx += glRenderer->bg[2].affine.dmx;
		glRenderer->bg[2].affine.sy += glRenderer->bg[2].affine.dmy;
		glRenderer->bg[3].affine.sx += glRenderer->bg[3].affine.dmx;
		glRenderer->bg[3].affine.sy += glRenderer->bg[3].affine.dmy;
	}
}

void _drawScanlines(struct GBAVideoGLRenderer* glRenderer, int y) {
	glEnable(GL_SCISSOR_TEST);

	uint32_t backdrop = M_RGB5_TO_RGB8(glRenderer->shadowPalette[0]);
	glViewport(0, 0, 1, GBA_VIDEO_VERTICAL_PIXELS);
	glScissor(0, glRenderer->firstY, 1, y - glRenderer->firstY + 1);
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_BACKDROP]);
	glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	glClearBufferfv(GL_COLOR, 0, (GLfloat[]) { ((backdrop >> 16) & 0xF8) / 248., ((backdrop >> 8) & 0xF8) / 248., (backdrop & 0xF8) / 248., 1.f });
	glClearBufferiv(GL_COLOR, 1, (GLint[]) { 32, glRenderer->target1Bd | (glRenderer->target2Bd * 2) | (glRenderer->blendEffect * 4), glRenderer->blda, 0 });
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });

	GBAVideoGLRendererDrawWindow(glRenderer, y);
	if (GBARegisterDISPCNTIsObjEnable(glRenderer->dispcnt) && !glRenderer->d.disableOBJ) {
		int i;
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glEnable(GL_STENCIL_TEST);
		glDepthFunc(GL_LESS);
		for (i = 0; i < glRenderer->oamMax; ++i) {
			struct GBAVideoRendererSprite* sprite = &glRenderer->sprites[i];
			if ((y < sprite->y && (sprite->endY - 256 < 0 || glRenderer->firstY >= sprite->endY - 256)) || glRenderer->firstY >= sprite->endY) {
				continue;
			}

			GBAVideoGLRendererDrawSprite(glRenderer, &sprite->obj, y, sprite->y);
		}
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
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
			GBAVideoGLRendererDrawBackgroundMode3(glRenderer, &glRenderer->bg[2], y);
			break;
		case 4:
			GBAVideoGLRendererDrawBackgroundMode4(glRenderer, &glRenderer->bg[2], y);
			break;
		case 5:
			GBAVideoGLRendererDrawBackgroundMode5(glRenderer, &glRenderer->bg[2], y);
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
	glRenderer->firstY = -1;
}

void GBAVideoGLRendererFinishFrame(struct GBAVideoRenderer* renderer) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	_drawScanlines(glRenderer, GBA_VIDEO_VERTICAL_PIXELS - 1);
	_finalizeLayers(glRenderer);
	glDisable(GL_SCISSOR_TEST);
	glBindVertexArray(0);
	glRenderer->firstAffine = -1;
	glRenderer->firstY = -1;
	glRenderer->bg[2].affine.sx = glRenderer->bg[2].refx;
	glRenderer->bg[2].affine.sy = glRenderer->bg[2].refy;
	glRenderer->bg[3].affine.sx = glRenderer->bg[3].refx;
	glRenderer->bg[3].affine.sy = glRenderer->bg[3].refy;
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
	// TODO
	UNUSED(renderer);
	UNUSED(stride);
	UNUSED(pixels);
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
	bg->affine.sx = bg->refx;
}

static void GBAVideoGLRendererWriteBGX_HI(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refx = (bg->refx & 0x0000FFFF) | (value << 16);
	bg->refx <<= 4;
	bg->refx >>= 4;
	bg->affine.sx = bg->refx;
}

static void GBAVideoGLRendererWriteBGY_LO(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refy = (bg->refy & 0xFFFF0000) | value;
	bg->affine.sy = bg->refy;
}

static void GBAVideoGLRendererWriteBGY_HI(struct GBAVideoGLBackground* bg, uint16_t value) {
	bg->refy = (bg->refy & 0x0000FFFF) | (value << 16);
	bg->refy <<= 4;
	bg->refy >>= 4;
	bg->affine.sy = bg->refy;
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
	if (GBARegisterDISPCNTIsForcedBlank(renderer->dispcnt)) {
		glClearColor(1.f, 1.f, 1.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
	} else {
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
		glActiveTexture(GL_TEXTURE0 + 11);
		glBindTexture(GL_TEXTURE_2D, renderer->layers[GBA_GL_TEX_BACKDROP_COLOR]);
		glActiveTexture(GL_TEXTURE0 + 12);
		glBindTexture(GL_TEXTURE_2D, renderer->layers[GBA_GL_TEX_BACKDROP_FLAGS]);

		glUniform2i(uniforms[GBA_GL_VS_LOC], GBA_VIDEO_VERTICAL_PIXELS, 0);
		glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
		glUniform1i(uniforms[GBA_GL_FINALIZE_SCALE], renderer->scale);
		glUniform1iv(uniforms[GBA_GL_FINALIZE_LAYERS], 5, (GLint[]) { 3, 5, 7, 9, 1 });
		glUniform1iv(uniforms[GBA_GL_FINALIZE_FLAGS], 5, (GLint[]) { 4, 6, 8, 10, 2 });
		glUniform1i(uniforms[GBA_GL_FINALIZE_WINDOW], 0);
		glUniform1i(uniforms[GBA_GL_FINALIZE_WINDOW], 0);
		glUniform1i(uniforms[GBA_GL_FINALIZE_BACKDROP], 11);
		glUniform1i(uniforms[GBA_GL_FINALIZE_BACKDROPFLAGS], 12);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
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

	int totalWidth = width;
	int totalHeight = height;
	if (GBAObjAttributesAIsTransformed(sprite->a) && GBAObjAttributesAIsDoubleSize(sprite->a)) {
		totalWidth <<= 1;
		totalHeight <<= 1;
	}

	if (spriteY + totalHeight >= 256) {
		spriteY -= 256;
	}

	const struct GBAVideoGLShader* shader = &renderer->objShader[GBAObjAttributesAGet256Color(sprite->a)];
	const GLuint* uniforms = shader->uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo[GBA_GL_FBO_OBJ]);
	glViewport(x * renderer->scale, spriteY * renderer->scale, totalWidth * renderer->scale, totalHeight * renderer->scale);
	glScissor(x * renderer->scale, renderer->firstY * renderer->scale, totalWidth * renderer->scale, (y - renderer->firstY + 1) * renderer->scale);
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->vramTex);
	glUniform2i(uniforms[GBA_GL_VS_LOC], totalHeight, 0);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], totalWidth, totalHeight);
	glUniform1i(uniforms[GBA_GL_OBJ_VRAM], 0);
	glUniform1iv(uniforms[GBA_GL_OBJ_PALETTE], 256, &renderer->shadowPalette[256]);
	glUniform1i(uniforms[GBA_GL_OBJ_CHARBASE], charBase);
	glUniform1i(uniforms[GBA_GL_OBJ_STRIDE], stride);
	glUniform1i(uniforms[GBA_GL_OBJ_LOCALPALETTE], GBAObjAttributesCGetPalette(sprite->c));
	glUniform4i(uniforms[GBA_GL_OBJ_INFLAGS], GBAObjAttributesCGetPriority(sprite->c),
	                                          (renderer->target1Obj || GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT) | (renderer->target2Obj * 2) | (renderer->blendEffect * 4),
	                                          renderer->blda, GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT);
	if (GBAObjAttributesAIsTransformed(sprite->a)) {
		struct GBAOAMMatrix mat;
		LOAD_16(mat.a, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].a);
		LOAD_16(mat.b, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].b);
		LOAD_16(mat.c, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].c);
		LOAD_16(mat.d, 0, &renderer->d.oam->mat[GBAObjAttributesBGetMatIndex(sprite->b)].d);

		glUniformMatrix2fv(uniforms[GBA_GL_OBJ_TRANSFORM], 1, GL_FALSE, (GLfloat[]) { mat.a / 256.f, mat.c / 256.f, mat.b / 256.f, mat.d / 256.f });
	} else {
		int flipX = 1;
		int flipY = 1;
		if (GBAObjAttributesBIsHFlip(sprite->b)) {
			flipX = -1;
		}
		if (GBAObjAttributesBIsVFlip(sprite->b)) {
			flipY = -1;
		}
		glUniformMatrix2fv(uniforms[GBA_GL_OBJ_TRANSFORM], 1, GL_FALSE, (GLfloat[]) { flipX, 0, 0, flipY });
	}
	glUniform4i(uniforms[GBA_GL_OBJ_DIMS], width, height, totalWidth, totalHeight);
	if (GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_OBJWIN) {
		glDisable(GL_DEPTH_TEST);
		int window = renderer->objwin & 0x3F;
		glUniform4i(uniforms[GBA_GL_OBJ_OBJWIN], 1, window, renderer->bldb, renderer->bldy);
		glDrawBuffers(3, (GLenum[]) { GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT2 });
	} else {
		glEnable(GL_DEPTH_TEST);
		glUniform4i(uniforms[GBA_GL_OBJ_OBJWIN], 0, 0, 0, 0);
		glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	}
	if (GBAObjAttributesAIsMosaic(sprite->a) && GBAObjAttributesAGetMode(sprite->a) != OBJ_MODE_OBJWIN) {
		int mosaicH = GBAMosaicControlGetObjH(renderer->mosaic) + 1;
		if (GBAObjAttributesBIsHFlip(sprite->b)) {
			mosaicH = -mosaicH;
		}
		glUniform4i(uniforms[GBA_GL_OBJ_MOSAIC], mosaicH, GBAMosaicControlGetObjV(renderer->mosaic) + 1, x, spriteY);
	} else {
		glUniform4i(uniforms[GBA_GL_OBJ_MOSAIC], 0, 0, 0, 0);
	}
	glStencilFunc(GL_ALWAYS, 1, 1);
	if (GBAObjAttributesAGetMode(sprite->a) != OBJ_MODE_OBJWIN || GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt)) {
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	shader = &renderer->objShader[2];
	uniforms = shader->uniforms;
	glStencilFunc(GL_EQUAL, 1, 1);
	glUseProgram(shader->program);
	glDrawBuffers(2, (GLenum[]) { GL_NONE, GL_COLOR_ATTACHMENT1 });
	glBindVertexArray(shader->vao);
	glUniform2i(uniforms[GBA_GL_VS_LOC], totalHeight, 0);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], totalWidth, totalHeight);
	glUniform4i(uniforms[GBA_GL_OBJ_INFLAGS], GBAObjAttributesCGetPriority(sprite->c),
	                                          (renderer->target1Obj || GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT) | (renderer->target2Obj * 2) | (renderer->blendEffect * 4),
	                                          renderer->blda, GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void _prepareBackground(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, const GLuint* uniforms) {
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->vramTex);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glUniform1i(uniforms[GBA_GL_BG_VRAM], 0);
	glUniform1iv(uniforms[GBA_GL_OBJ_PALETTE], 256, renderer->shadowPalette);
	if (background->mosaic) {
		glUniform2i(uniforms[GBA_GL_BG_MOSAIC], GBAMosaicControlGetBgV(renderer->mosaic) + 1, GBAMosaicControlGetBgH(renderer->mosaic) + 1);
	} else {
		glUniform2i(uniforms[GBA_GL_BG_MOSAIC], 0, 0);
	}
	glUniform4i(uniforms[GBA_GL_BG_INFLAGS], background->priority,
		                                     background->target1 | (background->target2 * 2) | (renderer->blendEffect * 4),
		                                     renderer->blda, 0);
	glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
}

void GBAVideoGLRendererDrawBackgroundMode0(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	const struct GBAVideoGLShader* shader = &renderer->bgShader[background->multipalette ? 1 : 0];
	const GLuint* uniforms = shader->uniforms;
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	_prepareBackground(renderer, background, uniforms);
	glUniform1i(uniforms[GBA_GL_BG_SCREENBASE], background->screenBase);
	glUniform1i(uniforms[GBA_GL_BG_CHARBASE], background->charBase);
	glUniform1i(uniforms[GBA_GL_BG_SIZE], background->size);
	glUniform1iv(uniforms[GBA_GL_BG_OFFSET], GBA_VIDEO_VERTICAL_PIXELS, background->scanlineOffset);

	glScissor(0, renderer->firstY * renderer->scale, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, (y - renderer->firstY + 1) * renderer->scale);
	glUniform2i(uniforms[GBA_GL_VS_LOC], y - renderer->firstY + 1, renderer->firstY);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void _prepareTransform(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, const GLuint* uniforms, int y) {
	glScissor(0, renderer->firstY * renderer->scale, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, renderer->scale * (y - renderer->firstY + 1));
	glUniform2i(uniforms[GBA_GL_VS_LOC], y - renderer->firstY + 1, renderer->firstY);
	glUniform2i(uniforms[GBA_GL_BG_RANGE], renderer->firstAffine, y);

	glUniform4iv(uniforms[GBA_GL_BG_TRANSFORM], GBA_VIDEO_VERTICAL_PIXELS, background->scanlineAffine);
	_prepareBackground(renderer, background, uniforms);
}

void GBAVideoGLRendererDrawBackgroundMode2(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	const struct GBAVideoGLShader* shader = &renderer->bgShader[background->overflow ? 2 : 3];
	const GLuint* uniforms = shader->uniforms;
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	_prepareTransform(renderer, background, uniforms, y);
	glUniform1i(uniforms[GBA_GL_BG_SCREENBASE], background->screenBase);
	glUniform1i(uniforms[GBA_GL_BG_CHARBASE], background->charBase);
	glUniform1i(uniforms[GBA_GL_BG_SIZE], background->size);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void GBAVideoGLRendererDrawBackgroundMode3(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	const struct GBAVideoGLShader* shader = &renderer->bgShader[5];
	const GLuint* uniforms = shader->uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	_prepareTransform(renderer, background, uniforms, y);
	glUniform1i(uniforms[GBA_GL_BG_CHARBASE], 0);
	glUniform2i(uniforms[GBA_GL_BG_SIZE], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void GBAVideoGLRendererDrawBackgroundMode4(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	const struct GBAVideoGLShader* shader = &renderer->bgShader[4];
	const GLuint* uniforms = shader->uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	_prepareTransform(renderer, background, uniforms, y);
	glUniform1i(uniforms[GBA_GL_BG_CHARBASE], GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt) ? 0xA000 : 0);
	glUniform2i(uniforms[GBA_GL_BG_SIZE], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void GBAVideoGLRendererDrawBackgroundMode5(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, int y) {
	const struct GBAVideoGLShader* shader = &renderer->bgShader[5];
	const GLuint* uniforms = shader->uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	_prepareTransform(renderer, background, uniforms, y);
	glUniform1i(uniforms[GBA_GL_BG_CHARBASE], GBARegisterDISPCNTIsFrameSelect(renderer->dispcnt) ? 0x5000 : 0);
	glUniform2i(uniforms[GBA_GL_BG_SIZE], 160, 128);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void GBAVideoGLRendererDrawWindow(struct GBAVideoGLRenderer* renderer, int y) {
	const struct GBAVideoGLShader* shader = &renderer->windowShader;
	const GLuint* uniforms = shader->uniforms;
	glBindFramebuffer(GL_FRAMEBUFFER, renderer->fbo[GBA_GL_FBO_WINDOW]);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glScissor(0, renderer->firstY * renderer->scale, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, renderer->scale * (y - renderer->firstY + 1));
	glUseProgram(shader->program);
	glBindVertexArray(shader->vao);
	glUniform2i(uniforms[GBA_GL_VS_LOC], y - renderer->firstY + 1, renderer->firstY);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glUniform1i(uniforms[GBA_GL_WIN_DISPCNT], renderer->dispcnt >> 8);
	glUniform2i(uniforms[GBA_GL_WIN_BLEND], renderer->bldb, renderer->bldy);
	glUniform3i(uniforms[GBA_GL_WIN_FLAGS], renderer->winN[0].control, renderer->winN[1].control, renderer->winout);
	glUniform4iv(uniforms[GBA_GL_WIN_WIN0], GBA_VIDEO_VERTICAL_PIXELS, renderer->winNHistory[0]);
	glUniform4iv(uniforms[GBA_GL_WIN_WIN1], GBA_VIDEO_VERTICAL_PIXELS, renderer->winNHistory[1]);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void GBAVideoGLRendererSetScale(struct GBAVideoGLRenderer* renderer, int scale) {
	if (scale == renderer->scale) {
		return;
	}
	if (renderer->temporaryBuffer) {
		mappedMemoryFree(renderer->temporaryBuffer, GBA_VIDEO_HORIZONTAL_PIXELS * GBA_VIDEO_VERTICAL_PIXELS * renderer->scale * renderer->scale * BYTES_PER_PIXEL);
		renderer->temporaryBuffer = NULL;
	}
	renderer->scale = scale;
	_initFramebuffers(renderer);
	renderer->paletteDirty = true;
}

#endif
