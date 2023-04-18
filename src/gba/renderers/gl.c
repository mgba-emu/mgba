/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/renderers/gl.h>

#ifdef BUILD_GLES3

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
#define MOSAIC \
	"#define MOSAIC(LHS, RHS) (((int(LHS) * mosaicTable[RHS]) >> 12) * RHS)\n" \
	"const int mosaicTable[17] = int[17](0, 4096, 2048, 1366, 1024, 820, 683, 586, 512, 456, 410, 373, 342, 316, 293, 274, 256);\n"

static const GLchar* const _gles3Header =
	"#version 300 es\n"
	"#define OUT(n) layout(location = n)\n"
	PALETTE_ENTRY
	"precision highp float;\n"
	"precision highp int;\n"
	"precision highp sampler2D;\n"
	"precision highp isampler2D;\n";

static const GLchar* const _gl3Header =
	"#version 330 core\n"
	"#define OUT(n) layout(location = n)\n"
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
	"#ifndef VRAM_MASK\n"
	"#define VRAM_MASK\n"
	"#endif\n"
	"int renderTile(int tile, int paletteId, ivec2 localCoord) {\n"
	"	int address = charBase + tile * 16 + (localCoord.x >> 2) + (localCoord.y << 1);\n"
	"	int halfrow = texelFetch(vram, ivec2(address & 255, (address >> 8) VRAM_MASK), 0).r;\n"
	"	int entry = (halfrow >> (4 * (localCoord.x & 3))) & 15;\n"
	"	if (entry == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	return paletteId * 16 + entry;\n"
	"}\n"
	"int mask(int tile) {\n"
	"	return tile & 31;\n"
	"}";

static const char* const _renderTile256 =
	"#ifndef VRAM_MASK\n"
	"#define VRAM_MASK\n"
	"#endif\n"
	"int renderTile(int tile, int paletteId, ivec2 localCoord) {\n"
	"	int address = charBase + tile * 32 + (localCoord.x >> 1) + (localCoord.y << 2);\n"
	"	int halfrow = texelFetch(vram, ivec2(address & 255, (address >> 8) VRAM_MASK), 0).r;\n"
	"	int entry = (halfrow >> (8 * (localCoord.x & 1))) & 255;\n"
	"	if (entry == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	return entry;\n"
	"}"
	"int mask(int tile) {\n"
	"	return tile & 15;\n"
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
	{ "mosaic", GBA_GL_BG_MOSAIC, },
	{ 0 }
};

static const char* const _renderMode0 =
	MOSAIC
	"in vec2 texCoord;\n"
	"uniform isampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int screenBase;\n"
	"uniform int charBase;\n"
	"uniform int size;\n"
	"uniform int offset[160];\n"
	"uniform ivec2 mosaic;\n"
	"OUT(0) out vec4 color;\n"

	"int renderTile(int tile, int paletteId, ivec2 localCoord);\n"

	"void main() {\n"
	"	ivec2 coord = ivec2(texCoord);\n"
	"	if (mosaic.x > 1) {\n"
	"		coord.x = MOSAIC(coord.x, mosaic.x);\n"
	"	}\n"
	"	if (mosaic.y > 1) {\n"
	"		coord.y = MOSAIC(coord.y, mosaic.y);\n"
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
	"	int map = texelFetch(vram, ivec2(mapAddress & 255, mapAddress >> 8), 0).r;\n"
	"	if ((map & 1024) == 1024) {\n"
	"		coord.x ^= 7;\n"
	"	}\n"
	"	if ((map & 2048) == 2048) {\n"
	"		coord.y ^= 7;\n"
	"	}\n"
	"	int tile = map & 1023;\n"
	"	int paletteEntry = renderTile(tile, (map >> 12) & 15, coord & 7);\n"
	"	color = texelFetch(palette, ivec2(paletteEntry, int(texCoord.y)), 0);\n"
	"}\n";

static const char* const _fetchTileOverflow =
	"int fetchTile(ivec2 coord) {\n"
	"	int sizeAdjusted = (0x8000 << size) - 1;\n"
	"	coord &= sizeAdjusted;\n"
	"	return renderTile(coord);\n"
	"}";

static const char* const _fetchTileNoOverflow =
	"int fetchTile(ivec2 coord) {\n"
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
	{ "oldCharBase", GBA_GL_BG_OLDCHARBASE, },
	{ "charBase", GBA_GL_BG_CHARBASE, },
	{ "size", GBA_GL_BG_SIZE, },
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
	"}\n"

	"ivec2 affineInterpolate() {\n"
	"	ivec2 mat[4];\n"
	"	ivec2 offset[4];\n"
	"	vec2 incoord = texCoord;\n"
	"	if (mosaic.x > 1) {\n"
	"		incoord.x = float(MOSAIC(incoord.x, mosaic.x));\n"
	"	}\n"
	"	if (mosaic.y > 1) {\n"
	"		incoord.y = float(MOSAIC(incoord.y, mosaic.y));\n"
	"	}\n"
	"	loadAffine(int(incoord.y), mat, offset);\n"
	"	float y = fract(incoord.y);\n"
	"	float start = 2. / 3.;\n"
	"	if (int(incoord.y) - range.x < 4) {\n"
	"		y = incoord.y - float(range.x);\n"
	"		start -= 1.;\n"
	"	}\n"
	"	float lin = start + y / 3.;\n"
	"	vec2 mixedTransform = interpolate(mat, lin);\n"
	"	vec2 mixedOffset = interpolate(offset, lin);\n"
	"	return ivec2(mixedTransform * incoord.x + mixedOffset);\n"
	"}\n";

static const char* const _renderMode2 =
	MOSAIC
	"in vec2 texCoord;\n"
	"uniform isampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int screenBase;\n"
	"uniform ivec2 oldCharBase;\n"
	"uniform int charBase;\n"
	"uniform int size;\n"
	"uniform ivec4 transform[160];\n"
	"uniform ivec2 range;\n"
	"uniform ivec2 mosaic;\n"
	"OUT(0) out vec4 color;\n"

	"int fetchTile(ivec2 coord);\n"
	"ivec2 affineInterpolate();\n"

	"int renderTile(ivec2 coord) {\n"
	"	int map = (coord.x >> 11) + (((coord.y >> 7) & 0x7F0) << size);\n"
	"	int mapAddress = screenBase + (map >> 1);\n"
	"	int twomaps = texelFetch(vram, ivec2(mapAddress & 255, mapAddress >> 8), 0).r;\n"
	"	int tile = (twomaps >> (8 * (map & 1))) & 255;\n"
	"	int newCharBase = charBase;\n"
	"	if (newCharBase != oldCharBase.x) {\n"
	"		int y = int(texCoord.y);\n"
	// If the charbase has changed (and the scale is greater than 1), we might still be drawing
	// the tile associated with the pixel above us. If we're still on that tile, we want to use
	// the charbase associated with it instead of the new one. Cf. https://mgba.io/i/1631
	"		if (y == oldCharBase.y && transform[y - 1].w >> 11 == coord.y >> 11) {\n"
	"			newCharBase = oldCharBase.x;\n"
	"		}\n"
	"	}\n"
	"	int address = newCharBase + tile * 32 + ((coord.x >> 9) & 3) + ((coord.y >> 6) & 0x1C);\n"
	"	int halfrow = texelFetch(vram, ivec2(address & 255, address >> 8), 0).r;\n"
	"	int entry = (halfrow >> (8 * ((coord.x >> 8) & 1))) & 255;\n"
	"	if (entry == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	return entry;\n"
	"}\n"

	"void main() {\n"
	"	int paletteEntry = fetchTile(affineInterpolate());\n"
	"	color = texelFetch(palette, ivec2(paletteEntry, int(texCoord.y)), 0);\n"
	"}";

static const struct GBAVideoGLUniform _uniformsMode35[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "vram", GBA_GL_BG_VRAM, },
	{ "charBase", GBA_GL_BG_CHARBASE, },
	{ "size", GBA_GL_BG_SIZE, },
	{ "offset", GBA_GL_BG_OFFSET, },
	{ "transform", GBA_GL_BG_TRANSFORM, },
	{ "range", GBA_GL_BG_RANGE, },
	{ "mosaic", GBA_GL_BG_MOSAIC, },
	{ 0 }
};

static const char* const _renderMode35 =
	MOSAIC
	"in vec2 texCoord;\n"
	"uniform isampler2D vram;\n"
	"uniform int charBase;\n"
	"uniform ivec2 size;\n"
	"uniform ivec4 transform[160];\n"
	"uniform ivec2 range;\n"
	"uniform ivec2 mosaic;\n"
	"OUT(0) out vec4 color;\n"

	"ivec2 affineInterpolate();\n"

	"void main() {\n"
	"	ivec2 coord = affineInterpolate();\n"
	"	if (coord.x < 0 || coord.x >= (size.x << 8)) {\n"
	"		discard;\n"
	"	}\n"
	"	if (coord.y < 0 || coord.y >= (size.y << 8)) {\n"
	"		discard;\n"
	"	}\n"
	"	int address = charBase + (coord.x >> 8) + (coord.y >> 8) * size.x;\n"
	"	int entry = texelFetch(vram, ivec2(address & 255, address >> 8), 0).r;\n"
	"	color = vec4(float(entry & 0x1F) / 31., float((entry >> 5) & 0x1F) / 31., float((entry >> 10) & 0x1F) / 31., 1.);\n"
	"}";

static const struct GBAVideoGLUniform _uniformsMode4[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "vram", GBA_GL_BG_VRAM, },
	{ "palette", GBA_GL_BG_PALETTE, },
	{ "charBase", GBA_GL_BG_CHARBASE, },
	{ "size", GBA_GL_BG_SIZE, },
	{ "offset", GBA_GL_BG_OFFSET, },
	{ "transform", GBA_GL_BG_TRANSFORM, },
	{ "range", GBA_GL_BG_RANGE, },
	{ "mosaic", GBA_GL_BG_MOSAIC, },
	{ 0 }
};

static const char* const _renderMode4 =
	MOSAIC
	"in vec2 texCoord;\n"
	"uniform isampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int charBase;\n"
	"uniform ivec2 size;\n"
	"uniform ivec4 transform[160];\n"
	"uniform ivec2 range;\n"
	"uniform ivec2 mosaic;\n"
	"OUT(0) out vec4 color;\n"

	"ivec2 affineInterpolate();\n"

	"void main() {\n"
	"	ivec2 coord = affineInterpolate();\n"
	"	if (coord.x < 0 || coord.x >= (size.x << 8)) {\n"
	"		discard;\n"
	"	}\n"
	"	if (coord.y < 0 || coord.y >= (size.y << 8)) {\n"
	"		discard;\n"
	"	}\n"
	"	int address = charBase + (coord.x >> 8) + (coord.y >> 8) * size.x;\n"
	"	int twoEntries = texelFetch(vram, ivec2((address >> 1) & 255, address >> 9), 0).r;\n"
	"	int entry = (twoEntries >> (8 * (address & 1))) & 255;\n"
	"	if (entry == 0) {\n"
	"		discard;\n"
	"	}\n"
	"	color = texelFetch(palette, ivec2(entry, int(texCoord.y)), 0);\n"
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
	{ "cyclesRemaining", GBA_GL_OBJ_CYCLES, },
	{ "tile", GBA_GL_OBJ_TILE, },
	{ 0 }
};

static const char* const _renderObj =
	MOSAIC
	"in vec2 texCoord;\n"
	"uniform isampler2D vram;\n"
	"uniform sampler2D palette;\n"
	"uniform int charBase;\n"
	"uniform int tile;\n"
	"uniform int stride;\n"
	"uniform int localPalette;\n"
	"uniform ivec4 inflags;\n"
	"uniform mat2x2 transform;\n"
	"uniform ivec4 dims;\n"
	"uniform ivec3 objwin;\n"
	"uniform ivec4 mosaic;\n"
	"uniform int cyclesRemaining[160];\n"
	"OUT(0) out vec4 color;\n"
	"OUT(1) out ivec4 flags;\n"
	"OUT(2) out ivec4 window;\n"

	"int renderTile(int tile, int paletteId, ivec2 localCoord);\n"
	"int mask(int);\n"
	"#define VRAM_MASK & 191\n"

	"void main() {\n"
	"	vec2 incoord = texCoord;\n"
	"	if (mosaic.x > 1) {\n"
	"		int x = int(incoord.x);\n"
	"		x = MOSAIC(mosaic.z + x, mosaic.x) - mosaic.z;\n"
	"		incoord.x = float(clamp(x, 0, dims.z - 1));\n"
	"	} else if (mosaic.x < -1) {\n"
	"		int x = dims.z - int(incoord.x) - 1;\n"
	"		x = dims.z - MOSAIC(mosaic.z + x, -mosaic.x) + mosaic.z - 1;\n"
	"		incoord.x = float(clamp(x, 0, dims.z - 1));\n"
	"	}\n"
	"	if (cyclesRemaining[int(incoord.y) + mosaic.w] <= 0) {\n"
	"		discard;\n"
	"	}\n"
	"	if (mosaic.y > 1) {\n"
	"		int y = int(incoord.y);\n"
	"		y = MOSAIC(mosaic.w + y, mosaic.y) - mosaic.w;"
	"		incoord.y = float(clamp(y, 0, dims.w - 1));\n"
	"	}\n"
	"	ivec2 coord = ivec2(transform * (incoord - vec2(dims.zw) / 2.) + vec2(dims.xy) / 2.);\n"
	"	if ((coord & ~(dims.xy - 1)) != ivec2(0, 0)) {\n"
	"		discard;\n"
	"	}\n"
	"	int paletteEntry = renderTile(mask((coord.x >> 3) + tile) + (coord.y >> 3) * stride, localPalette, coord & 7);\n"
	"	color = texelFetch(palette, ivec2(paletteEntry + 256, int(texCoord.y) + mosaic.w), 0);\n"
	"	flags = inflags;\n"
	"	gl_FragDepth = float(flags.x) / 16.;\n"
	"	window = ivec4(objwin, 0);\n"
	"}\n";

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

	"bool crop(vec4 windowParams) {\n"
	"	bvec4 compare = lessThan(texCoord.xxyy, windowParams);\n"
	"	compare = equal(compare, bvec4(true, false, true, false));\n"
	"	if (any(compare)) {\n"
	"		vec2 h = windowParams.xy;\n"
	"		vec2 v = windowParams.zw;\n"
	"		if (v.x > v.y) {\n"
	"			if (compare.z && compare.w) {\n"
	"				return false;\n"
	"			}\n"
	"		} else if (compare.z || compare.w) {\n"
	"			return false;\n"
	"		}\n"
	"		if (h.x > h.y) {\n"
	"			if (compare.x && compare.y) {\n"
	"				return false;\n"
	"			}\n"
	"		} else if (compare.x || compare.y) {\n"
	"			return false;\n"
	"		}\n"
	"	}\n"
	"	return true;\n"
	"}\n"

	"vec4 interpolate(vec4 top, vec4 bottom) {\n"
	"	if (distance(top, bottom) > 40.) {\n"
	"		return top;\n"
	"	}\n"
	"	return vec4(mix(bottom.xy, top.xy, fract(texCoord.y)), top.zw);\n"
	"}\n"

	"void main() {\n"
	"	ivec4 windowFlags = ivec4(flags.z, blend, 0);\n"
	"	int top = int(texCoord.y);\n"
	"	int bottom = max(top - 1, 0);\n"
	"	if ((dispcnt & 0x20) != 0 && crop(interpolate(vec4(win0[top]), vec4(win0[bottom])))) { \n"
	"		windowFlags.x = flags.x;\n"
	"	} else if ((dispcnt & 0x40) != 0 && crop(interpolate(vec4(win1[top]), vec4(win1[bottom])))) {\n"
	"		windowFlags.x = flags.y;\n"
	"	}\n"
	"	window = windowFlags;\n"
	"}\n";

static const struct GBAVideoGLUniform _uniformsFinalize[] = {
	{ "loc", GBA_GL_VS_LOC, },
	{ "maxPos", GBA_GL_VS_MAXPOS, },
	{ "scale", GBA_GL_FINALIZE_SCALE, },
	{ "layers", GBA_GL_FINALIZE_LAYERS, },
	{ "objFlags", GBA_GL_FINALIZE_FLAGS, },
	{ "window", GBA_GL_FINALIZE_WINDOW, },
	{ "palette", GBA_GL_FINALIZE_PALETTE, },
	{ "backdropFlags", GBA_GL_FINALIZE_BACKDROP, },
	{ 0 }
};

static const char* const _finalize =
	"in vec2 texCoord;\n"
	"uniform int scale;\n"
	"uniform sampler2D layers[5];\n"
	"uniform isampler2D objFlags;\n"
	"uniform isampler2D window;\n"
	"uniform sampler2D palette;\n"
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
	"	vec4 topPixel = texelFetch(palette, ivec2(0, texCoord.y), 0);\n"
	"	vec4 bottomPixel = topPixel;\n"
	"	ivec4 topFlags = ivec4(texelFetch(backdropFlags, ivec2(0, texCoord.y), 0));\n"
	"	ivec4 bottomFlags = topFlags;\n"
	"	ivec2 coord = ivec2(texCoord * float(scale));\n"
	"	ivec4 windowFlags = texelFetch(window, coord, 0);\n"
	"	int layerWindow = windowFlags.x;\n"
	"	if ((layerWindow & 16) != 0) {\n"
	"		vec4 pix = texelFetch(layers[4], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(objFlags, coord, 0));\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 1) != 0) {\n"
	"		vec4 pix = texelFetch(layers[0], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(backdropFlags, ivec2(1, texCoord.y), 0));\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 2) != 0) {\n"
	"		vec4 pix = texelFetch(layers[1], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(backdropFlags, ivec2(2, texCoord.y), 0));\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 4) != 0) {\n"
	"		vec4 pix = texelFetch(layers[2], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(backdropFlags, ivec2(3, texCoord.y), 0));\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 8) != 0) {\n"
	"		vec4 pix = texelFetch(layers[3], coord, 0);\n"
	"		if (pix.a != 0.) {\n"
	"			ivec4 inflags = ivec4(texelFetch(backdropFlags, ivec2(4, texCoord.y), 0));\n"
	"			composite(pix, inflags, topPixel, topFlags, bottomPixel, bottomFlags);\n"
	"		}\n"
	"	}\n"
	"	if ((layerWindow & 32) == 0) {\n"
	"		topFlags.y &= ~1;\n"
	"	}\n"
	"	if (((topFlags.y & 13) == 5 || topFlags.w > 0) && (bottomFlags.y & 2) == 2) {\n"
	"		topPixel.rgb *= float(topFlags.z) / 16.;\n"
	"		topPixel.rgb += bottomPixel.rgb * float(windowFlags.y) / 16.;\n"
	"	} else if (topFlags.w == 0) { \n"
	"		if ((topFlags.y & 13) == 9) {\n"
	"			topPixel.rgb += (1. - topPixel.rgb) * float(windowFlags.z) / 16.;\n"
	"		} else if ((topFlags.y & 13) == 13) {\n"
	"			topPixel.rgb -= topPixel.rgb * float(windowFlags.z) / 16.;\n"
	"		}\n"
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
	renderer->d.highlightColor = M_COLOR_WHITE;
	renderer->d.highlightAmount = 0;

	renderer->scale = 1;
}

static void _compileShader(struct GBAVideoGLRenderer* glRenderer, struct GBAVideoGLShader* shader, const char** shaderBuffer, int shaderBufferLines, GLuint vs, const struct GBAVideoGLUniform* uniforms, char* log) {
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

	size_t i;
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
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, scale > 0 ? GBA_VIDEO_HORIZONTAL_PIXELS * scale : 8, GBA_VIDEO_VERTICAL_PIXELS * (scale > 0 ? scale : 1), 0, format, type, 0);
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
	_initFramebufferTextureEx(glRenderer->layers[GBA_GL_TEX_BACKDROP], GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE, GL_COLOR_ATTACHMENT0, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_WINDOW]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glRenderer->layers[GBA_GL_TEX_WINDOW], 0);

	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OUTPUT]);
	_initFramebufferTexture(glRenderer->outputTex, GL_RGB, GL_COLOR_ATTACHMENT0, glRenderer->scale);
	glRenderer->outputTexDirty = false;

	int i;
	for (i = 0; i < 4; ++i) {
		struct GBAVideoGLBackground* bg = &glRenderer->bg[i];
		glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
		_initFramebufferTexture(bg->tex, GL_RGBA, GL_COLOR_ATTACHMENT0, glRenderer->scale);
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, 256, 192, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, 0);

	glGenTextures(1, &glRenderer->paletteTex);
	glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, GBA_VIDEO_VERTICAL_PIXELS, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);

	glGenBuffers(1, &glRenderer->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, glRenderer->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(_vertices), _vertices, GL_STATIC_DRAW);

	int i;
	for (i = 0; i < 4; ++i) {
		struct GBAVideoGLBackground* bg = &glRenderer->bg[i];
		bg->index = i;
		glGenFramebuffers(1, &bg->fbo);
		glGenTextures(1, &bg->tex);
	}

	_initFramebuffers(glRenderer);

	char log[2048];
	const GLchar* shaderBuffer[4];
	const GLubyte* version = glGetString(GL_VERSION);
	if (strncmp((const char*) version, "OpenGL ES ", strlen("OpenGL ES ")) != 0) {
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
	shaderBuffer[1] = _renderMode0;

	shaderBuffer[2] = _renderTile16;
	_compileShader(glRenderer, &glRenderer->bgShader[0], shaderBuffer, 3, vs, _uniformsMode0, log);

	shaderBuffer[2] = _renderTile256;
	_compileShader(glRenderer, &glRenderer->bgShader[1], shaderBuffer, 3, vs, _uniformsMode0, log);

	shaderBuffer[1] = _renderMode2;
	shaderBuffer[2] = _interpolate;

	shaderBuffer[3] = _fetchTileOverflow;
	_compileShader(glRenderer, &glRenderer->bgShader[2], shaderBuffer, 4, vs, _uniformsMode2, log);

	shaderBuffer[3] = _fetchTileNoOverflow;
	_compileShader(glRenderer, &glRenderer->bgShader[3], shaderBuffer, 4, vs, _uniformsMode2, log);

	shaderBuffer[1] = _renderMode4;
	shaderBuffer[2] = _interpolate;
	_compileShader(glRenderer, &glRenderer->bgShader[4], shaderBuffer, 3, vs, _uniformsMode4, log);

	shaderBuffer[1] = _renderMode35;
	shaderBuffer[2] = _interpolate;
	_compileShader(glRenderer, &glRenderer->bgShader[5], shaderBuffer, 3, vs, _uniformsMode35, log);

	shaderBuffer[1] = _renderObj;

	shaderBuffer[2] = _renderTile16;
	_compileShader(glRenderer, &glRenderer->objShader[0], shaderBuffer, 3, vs, _uniformsObj, log);

	shaderBuffer[2] = _renderTile256;
	_compileShader(glRenderer, &glRenderer->objShader[1], shaderBuffer, 3, vs, _uniformsObj, log);

	shaderBuffer[1] = _renderObjPriority;
	_compileShader(glRenderer, &glRenderer->objShader[2], shaderBuffer, 2, vs, _uniformsObjPriority, log);

	shaderBuffer[1] = _renderWindow;
	_compileShader(glRenderer, &glRenderer->windowShader, shaderBuffer, 2, vs, _uniformsWindow, log);

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
	glDeleteTextures(1, &glRenderer->vramTex);
	glDeleteTextures(1, &glRenderer->paletteTex);
	glDeleteBuffers(1, &glRenderer->vbo);

	_deleteShader(&glRenderer->bgShader[0]);
	_deleteShader(&glRenderer->bgShader[1]);
	_deleteShader(&glRenderer->bgShader[2]);
	_deleteShader(&glRenderer->bgShader[3]);
	_deleteShader(&glRenderer->objShader[0]);
	_deleteShader(&glRenderer->objShader[1]);
	_deleteShader(&glRenderer->objShader[2]);
	_deleteShader(&glRenderer->finalizeShader);

	int i;
	for (i = 0; i < 4; ++i) {
		struct GBAVideoGLBackground* bg = &glRenderer->bg[i];
		glDeleteFramebuffers(1, &bg->fbo);
		glDeleteTextures(1, &bg->tex);
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
	glRenderer->nextPalette = 0;
	glRenderer->paletteDirtyScanlines = GBA_VIDEO_VERTICAL_PIXELS;
	memset(glRenderer->shadowRegs, 0, sizeof(glRenderer->shadowRegs));
	glRenderer->shadowRegs[REG_DISPCNT >> 1] = glRenderer->dispcnt;
	glRenderer->regsDirty = 0xFFFFFFFFFFFEULL;

	glRenderer->objOffsetX = 0;
	glRenderer->objOffsetY = 0;

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
		bg->offsetX = 0;
		bg->offsetY = 0;
	}

	glRenderer->winN[0].offsetX = 0;
	glRenderer->winN[0].offsetY = 0;
	glRenderer->winN[1].offsetX = 0;
	glRenderer->winN[1].offsetY = 0;

	for (i = 0; i < 512; ++i) {
		int r = M_R5(glRenderer->d.palette[i]);
		int g = M_G5(glRenderer->d.palette[i]) << 1;
		g |= g >> 5;
		int b = M_B5(glRenderer->d.palette[i]);
		glRenderer->shadowPalette[0][i] = (r << 11) | (g << 5) | b;
	}
}

void GBAVideoGLRendererWriteVRAM(struct GBAVideoRenderer* renderer, uint32_t address) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (renderer->cache) {
		mCacheSetWriteVRAM(renderer->cache, address);
	}
	glRenderer->vramDirty |= 1 << (address >> 12);
}

void GBAVideoGLRendererWriteOAM(struct GBAVideoRenderer* renderer, uint32_t oam) {
	UNUSED(oam);
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	glRenderer->oamDirty = true;
}

void GBAVideoGLRendererWritePalette(struct GBAVideoRenderer* renderer, uint32_t address, uint16_t value) {
	struct GBAVideoGLRenderer* glRenderer = (struct GBAVideoGLRenderer*) renderer;
	if (renderer->cache) {
		mCacheSetWritePalette(renderer->cache, address >> 1, mColorFrom555(value));
	}
	glRenderer->paletteDirty = true;
	int r = M_R5(value);
	int g = M_G5(value) << 1;
	g |= g >> 5;
	int b = M_B5(value);
	glRenderer->paletteDirtyScanlines = GBA_VIDEO_VERTICAL_PIXELS;
	glRenderer->shadowPalette[glRenderer->nextPalette][address >> 1] = (r << 11) | (g << 5) | b;
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

	if (_needsVramUpload(glRenderer, y) || glRenderer->oamDirty || glRenderer->regsDirty) {
		if (glRenderer->firstY >= 0) {
			_drawScanlines(glRenderer, y - 1);
			glRenderer->firstY = y;
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

	glRenderer->winNHistory[0][y * 4 + 0] = glRenderer->winN[0].h.start + glRenderer->winN[0].offsetX;
	glRenderer->winNHistory[0][y * 4 + 1] = glRenderer->winN[0].h.end + glRenderer->winN[0].offsetX;
	glRenderer->winNHistory[0][y * 4 + 2] = glRenderer->winN[0].v.start + glRenderer->winN[0].offsetY;
	glRenderer->winNHistory[0][y * 4 + 3] = glRenderer->winN[0].v.end + glRenderer->winN[0].offsetY;
	glRenderer->winNHistory[1][y * 4 + 0] = glRenderer->winN[1].h.start + glRenderer->winN[1].offsetX;
	glRenderer->winNHistory[1][y * 4 + 1] = glRenderer->winN[1].h.end + glRenderer->winN[1].offsetX;
	glRenderer->winNHistory[1][y * 4 + 2] = glRenderer->winN[1].v.start + glRenderer->winN[1].offsetY;
	glRenderer->winNHistory[1][y * 4 + 3] = glRenderer->winN[1].v.end + glRenderer->winN[1].offsetY;

	glRenderer->bg[0].scanlineOffset[y] = (glRenderer->bg[0].x - glRenderer->bg[0].offsetX) & 0x1FF;
	glRenderer->bg[0].scanlineOffset[y] |= ((glRenderer->bg[0].y - glRenderer->bg[0].offsetY) & 0x1FF) << 12;
	glRenderer->bg[1].scanlineOffset[y] = (glRenderer->bg[1].x - glRenderer->bg[1].offsetX) & 0x1FF;
	glRenderer->bg[1].scanlineOffset[y] |= ((glRenderer->bg[1].y - glRenderer->bg[1].offsetY) & 0x1FF) << 12;
	glRenderer->bg[2].scanlineOffset[y] = (glRenderer->bg[2].x - glRenderer->bg[2].offsetX) & 0x1FF;
	glRenderer->bg[2].scanlineOffset[y] |= ((glRenderer->bg[2].y - glRenderer->bg[2].offsetY) & 0x1FF) << 12;
	glRenderer->bg[2].scanlineAffine[y * 4] = glRenderer->bg[2].affine.dx;
	glRenderer->bg[2].scanlineAffine[y * 4 + 1] = glRenderer->bg[2].affine.dy;
	glRenderer->bg[2].scanlineAffine[y * 4 + 2] = glRenderer->bg[2].affine.sx;
	glRenderer->bg[2].scanlineAffine[y * 4 + 3] = glRenderer->bg[2].affine.sy;
	glRenderer->bg[3].scanlineOffset[y] = (glRenderer->bg[3].x - glRenderer->bg[3].offsetX) & 0x1FF;
	glRenderer->bg[3].scanlineOffset[y] |= ((glRenderer->bg[3].y - glRenderer->bg[3].offsetY) & 0x1FF) << 12;
	glRenderer->bg[3].scanlineAffine[y * 4] = glRenderer->bg[3].affine.dx;
	glRenderer->bg[3].scanlineAffine[y * 4 + 1] = glRenderer->bg[3].affine.dy;
	glRenderer->bg[3].scanlineAffine[y * 4 + 2] = glRenderer->bg[3].affine.sx;
	glRenderer->bg[3].scanlineAffine[y * 4 + 3] = glRenderer->bg[3].affine.sy;

	int oldPalette = glRenderer->nextPalette;
	glRenderer->nextPalette = y + 1;
	if (glRenderer->nextPalette >= GBA_VIDEO_VERTICAL_PIXELS) {
		glRenderer->nextPalette = 0;
	}
	if (glRenderer->paletteDirty) {
		memcpy(glRenderer->shadowPalette[glRenderer->nextPalette], glRenderer->shadowPalette[oldPalette], sizeof(glRenderer->shadowPalette[0]));
		if (glRenderer->paletteDirtyScanlines > 0) {
			--glRenderer->paletteDirtyScanlines;
		}
		if (!glRenderer->paletteDirtyScanlines) {
			glRenderer->paletteDirty = false;
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, GBA_VIDEO_VERTICAL_PIXELS, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, glRenderer->shadowPalette);
		}
	}

	if (_needsVramUpload(glRenderer, y)) {
		int first = -1;
		glBindTexture(GL_TEXTURE_2D, glRenderer->vramTex);
		for (i = 0; i < 25; ++i) {
			if (!(glRenderer->vramDirty & (1 << i))) {
				if (first >= 0) {
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 8 * first, 256, 8 * (i - first), GL_RED_INTEGER, GL_UNSIGNED_SHORT, &glRenderer->d.vram[2048 * first]);
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
#ifdef GL_GLES_PROTOTYPES
		glClearDepthf(1.f);
#else
		glClearDepth(1);
#endif
		glClearStencil(0);
		glDepthMask(GL_TRUE);
		glStencilMask(1);
		glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_OBJ]);
		glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		for (i = 0; i < 4; ++i) {
			glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->bg[i].fbo);
			glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
			glClear(GL_COLOR_BUFFER_BIT);
		}

		int spriteCyclesRemaining = GBARegisterDISPCNTIsHblankIntervalFree(glRenderer->dispcnt) ? OBJ_HBLANK_FREE_LENGTH : OBJ_LENGTH;
		for (i = 0; i < GBA_VIDEO_VERTICAL_PIXELS; ++i) {
			glRenderer->spriteCycles[i] = spriteCyclesRemaining;
		}
	}

	if (GBARegisterDISPCNTGetMode(glRenderer->dispcnt) != 0) {
		if (glRenderer->bg[2].enabled == 4) {
			glRenderer->bg[2].affine.sx += glRenderer->bg[2].affine.dmx;
			glRenderer->bg[2].affine.sy += glRenderer->bg[2].affine.dmy;
		}
		if (glRenderer->bg[3].enabled == 4) {
			glRenderer->bg[3].affine.sx += glRenderer->bg[3].affine.dmx;
			glRenderer->bg[3].affine.sy += glRenderer->bg[3].affine.dmy;
		}
	}
}

void _drawScanlines(struct GBAVideoGLRenderer* glRenderer, int y) {
	glEnable(GL_SCISSOR_TEST);

	glViewport(0, 0, 1, GBA_VIDEO_VERTICAL_PIXELS);
	glScissor(0, glRenderer->firstY, 1, y - glRenderer->firstY + 1);
	glBindFramebuffer(GL_FRAMEBUFFER, glRenderer->fbo[GBA_GL_FBO_BACKDROP]);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
	glClearBufferiv(GL_COLOR, 0, (GLint[]) { 32, glRenderer->target1Bd | (glRenderer->target2Bd * 2) | (glRenderer->blendEffect * 4), glRenderer->blda, 0 });
	int i;
	for (i = 0; i < 4; ++i) {
		glScissor(i + 1, glRenderer->firstY, 1, y - glRenderer->firstY + 1);
		glClearBufferiv(GL_COLOR, 0, (GLint[]) { glRenderer->bg[i].priority,
		                                         glRenderer->bg[i].target1 | (glRenderer->bg[i].target2 << 1) | (glRenderer->blendEffect << 2),
		                                         glRenderer->blda, 0 });
	}

	if (glRenderer->paletteDirty) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, glRenderer->paletteTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, GBA_VIDEO_VERTICAL_PIXELS, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, glRenderer->shadowPalette);
	}

	GBAVideoGLRendererDrawWindow(glRenderer, y);
	if (GBARegisterDISPCNTIsObjEnable(glRenderer->dispcnt) && !glRenderer->d.disableOBJ) {
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
		glDepthFunc(GL_LESS);
		for (i = 0; i < glRenderer->oamMax; ++i) {
			struct GBAVideoRendererSprite* sprite = &glRenderer->sprites[i];
			if ((y < sprite->y && (sprite->endY - 256 < 0 || glRenderer->firstY >= sprite->endY - 256)) || glRenderer->firstY >= sprite->endY) {
				continue;
			}

			GBAVideoGLRendererDrawSprite(glRenderer, &sprite->obj, y, sprite->y);

			int startY = sprite->y;
			int endY = sprite->endY;

			if (endY >= 256) {
				startY -= 256;
				endY -= 256;
			}
			if (startY < glRenderer->firstY) {
				startY = glRenderer->firstY;
			}
			if (endY > y) {
				endY = y;
			}
			int j;
			for (j = startY; j <= endY; ++j) {
				glRenderer->spriteCycles[j] -= sprite->cycles;
			}
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
	bg->oldCharBase = bg->charBase;
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
	if (renderer->outputTexDirty) {
		_initFramebufferTexture(renderer->outputTex, GL_RGB, GL_COLOR_ATTACHMENT0, renderer->scale);
		renderer->outputTexDirty = false;
	}
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
		glBindTexture(GL_TEXTURE_2D, renderer->bg[1].tex);
		glActiveTexture(GL_TEXTURE0 + 5);
		glBindTexture(GL_TEXTURE_2D, renderer->bg[2].tex);
		glActiveTexture(GL_TEXTURE0 + 6);
		glBindTexture(GL_TEXTURE_2D, renderer->bg[3].tex);
		glActiveTexture(GL_TEXTURE0 + 7);
		glBindTexture(GL_TEXTURE_2D, renderer->paletteTex);
		glActiveTexture(GL_TEXTURE0 + 8);
		glBindTexture(GL_TEXTURE_2D, renderer->layers[GBA_GL_TEX_BACKDROP]);

		glUniform2i(uniforms[GBA_GL_VS_LOC], GBA_VIDEO_VERTICAL_PIXELS, 0);
		glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
		glUniform1i(uniforms[GBA_GL_FINALIZE_SCALE], renderer->scale);
		glUniform1iv(uniforms[GBA_GL_FINALIZE_LAYERS], 5, (GLint[]) { 3, 4, 5, 6, 1 });
		glUniform1i(uniforms[GBA_GL_FINALIZE_FLAGS], 2);
		glUniform1i(uniforms[GBA_GL_FINALIZE_WINDOW], 0);
		glUniform1i(uniforms[GBA_GL_FINALIZE_PALETTE], 7);
		glUniform1i(uniforms[GBA_GL_FINALIZE_BACKDROP], 8);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBAVideoGLRendererDrawSprite(struct GBAVideoGLRenderer* renderer, struct GBAObj* sprite, int y, int spriteY) {
	int width = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][0];
	int height = GBAVideoObjSizes[GBAObjAttributesAGetShape(sprite->a) * 4 + GBAObjAttributesBGetSize(sprite->b)][1];
	int32_t x = (uint32_t) GBAObjAttributesBGetX(sprite->b) << 23;
	x >>= 23;
	x += renderer->objOffsetX;

	if (GBARegisterDISPCNTGetMode(renderer->dispcnt) >= 3 && GBAObjAttributesCGetTile(sprite->c) < 512) {
		return;
	}

	int align = GBAObjAttributesAIs256Color(sprite->a) && !GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt);
	unsigned charBase = (BASE_TILE >> 1) + (GBAObjAttributesCGetTile(sprite->c) & ~align) * 0x10;
	unsigned tile = 0;
	if (!GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt)) {
		if (GBAObjAttributesAIs256Color(sprite->a)) {
			tile = (charBase >> 5) & 0xF;
		} else {
			tile = (charBase >> 4) & 0x1F;
		}
		charBase &= ~0x1FF;
	}
	int stride = GBARegisterDISPCNTIsObjCharacterMapping(renderer->dispcnt) ? (width >> 3) : (0x20 >> GBAObjAttributesAGet256Color(sprite->a));

	int totalWidth = width;
	int totalHeight = height;
	if (GBAObjAttributesAIsTransformed(sprite->a) && GBAObjAttributesAIsDoubleSize(sprite->a)) {
		totalWidth <<= 1;
		totalHeight <<= 1;
	}

	spriteY += renderer->objOffsetY;
	if (spriteY + totalHeight >= 256) {
		spriteY -= 256;
	}

	if (x + totalWidth <= 0 || x >= GBA_VIDEO_HORIZONTAL_PIXELS) {
		// These sprites aren't displayed but affect cycle counting
		return;
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
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, renderer->paletteTex);
	glUniform2i(uniforms[GBA_GL_VS_LOC], totalHeight, 0);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], totalWidth, totalHeight);
	glUniform1i(uniforms[GBA_GL_OBJ_VRAM], 0);
	glUniform1i(uniforms[GBA_GL_OBJ_PALETTE], 1);
	glUniform1i(uniforms[GBA_GL_OBJ_CHARBASE], charBase);
	glUniform1i(uniforms[GBA_GL_OBJ_TILE], tile);
	glUniform1i(uniforms[GBA_GL_OBJ_STRIDE], stride);
	glUniform1i(uniforms[GBA_GL_OBJ_LOCALPALETTE], GBAObjAttributesCGetPalette(sprite->c));
	glUniform4i(uniforms[GBA_GL_OBJ_INFLAGS], GBAObjAttributesCGetPriority(sprite->c),
	                                          (renderer->target1Obj || GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT) | (renderer->target2Obj * 2) | (renderer->blendEffect * 4),
	                                          renderer->blda, GBAObjAttributesAGetMode(sprite->a) == OBJ_MODE_SEMITRANSPARENT);
	glUniform1iv(uniforms[GBA_GL_OBJ_CYCLES], GBA_VIDEO_VERTICAL_PIXELS, renderer->spriteCycles);
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
		// OBJWIN writes do not affect pixel priority
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glStencilMask(0);
		int window = renderer->objwin & 0x3F;
		glUniform3i(uniforms[GBA_GL_OBJ_OBJWIN], window, renderer->bldb, renderer->bldy);
		glDrawBuffers(3, (GLenum[]) { GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT2 });
	} else {
		glEnable(GL_STENCIL_TEST);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glStencilMask(1);
		glStencilFunc(GL_ALWAYS, 1, 1);
		glUniform3i(uniforms[GBA_GL_OBJ_OBJWIN], 0, 0, 0);
		glDrawBuffers(2, (GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });
	}
	if (GBAObjAttributesAIsMosaic(sprite->a) && GBAObjAttributesAGetMode(sprite->a) != OBJ_MODE_OBJWIN) {
		int mosaicH = GBAMosaicControlGetObjH(renderer->mosaic) + 1;
		if (GBAObjAttributesBIsHFlip(sprite->b)) {
			mosaicH = -mosaicH;
		}
		glUniform4i(uniforms[GBA_GL_OBJ_MOSAIC], mosaicH, GBAMosaicControlGetObjV(renderer->mosaic) + 1, x, spriteY);
	} else {
		glUniform4i(uniforms[GBA_GL_OBJ_MOSAIC], 0, 0, x, spriteY);
	}
	if (GBAObjAttributesAGetMode(sprite->a) != OBJ_MODE_OBJWIN || GBARegisterDISPCNTIsObjwinEnable(renderer->dispcnt)) {
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	if (GBAObjAttributesAGetMode(sprite->a) != OBJ_MODE_OBJWIN) {
		// Update the pixel priority for already-written pixels
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
	}

	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
}

void _prepareBackground(struct GBAVideoGLRenderer* renderer, struct GBAVideoGLBackground* background, const GLuint* uniforms) {
	glBindFramebuffer(GL_FRAMEBUFFER, background->fbo);
	glViewport(0, 0, GBA_VIDEO_HORIZONTAL_PIXELS * renderer->scale, GBA_VIDEO_VERTICAL_PIXELS * renderer->scale);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, renderer->vramTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, renderer->paletteTex);
	glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
	glUniform1i(uniforms[GBA_GL_BG_VRAM], 0);
	glUniform1i(uniforms[GBA_GL_OBJ_PALETTE], 1);
	if (background->mosaic) {
		glUniform2i(uniforms[GBA_GL_BG_MOSAIC], GBAMosaicControlGetBgH(renderer->mosaic) + 1, GBAMosaicControlGetBgV(renderer->mosaic) + 1);
	} else {
		glUniform2i(uniforms[GBA_GL_BG_MOSAIC], 0, 0);
	}
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
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
	glUniform2i(uniforms[GBA_GL_BG_OLDCHARBASE], background->oldCharBase, renderer->firstY);
	glUniform1i(uniforms[GBA_GL_BG_CHARBASE], background->charBase);
	glUniform1i(uniforms[GBA_GL_BG_SIZE], background->size);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
	background->oldCharBase = background->charBase;
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
	glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
	switch (renderer->dispcnt & 0xE000) {
	case 0x0000:
		// No windows are enabled
		glClearBufferiv(GL_COLOR, 0, (GLint[]) { ((renderer->dispcnt >> 8) & 0x1F) | 0x20, renderer->bldb, renderer->bldy, 0 });
		break;
	case 0x8000:
		// Only OBJWIN is enabled
		glClearBufferiv(GL_COLOR, 0, (GLint[]) { renderer->winout, renderer->bldb, renderer->bldy, 0 });
		break;
	default:
		glUseProgram(shader->program);
		glBindVertexArray(shader->vao);
		glUniform2i(uniforms[GBA_GL_VS_LOC], y - renderer->firstY + 1, renderer->firstY);
		glUniform2i(uniforms[GBA_GL_VS_MAXPOS], GBA_VIDEO_HORIZONTAL_PIXELS, GBA_VIDEO_VERTICAL_PIXELS);
		glUniform1i(uniforms[GBA_GL_WIN_DISPCNT], renderer->dispcnt >> 8);
		glUniform2i(uniforms[GBA_GL_WIN_BLEND], renderer->bldb, renderer->bldy);
		glUniform3i(uniforms[GBA_GL_WIN_FLAGS], renderer->winN[0].control, renderer->winN[1].control, renderer->winout);
		glUniform4iv(uniforms[GBA_GL_WIN_WIN0], GBA_VIDEO_VERTICAL_PIXELS, renderer->winNHistory[0]);
		glUniform4iv(uniforms[GBA_GL_WIN_WIN1], GBA_VIDEO_VERTICAL_PIXELS, renderer->winNHistory[1]);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		break;
	}
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
