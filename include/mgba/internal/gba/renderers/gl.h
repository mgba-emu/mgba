/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VIDEO_GL_H
#define VIDEO_GL_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/core.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/renderers/common.h>
#include <mgba/internal/gba/video.h>

#ifdef BUILD_GLES3

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#elif defined(BUILD_GL)
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#else
#include <GLES3/gl3.h>
#endif

struct GBAVideoGLAffine {
	int16_t dx;
	int16_t dmx;
	int16_t dy;
	int16_t dmy;
	int32_t sx;
	int32_t sy;
};

struct GBAVideoGLBackground {
	GLuint fbo;
	GLuint tex;

	unsigned index;
	int enabled;
	unsigned priority;
	uint32_t charBase;
	uint32_t oldCharBase;
	int mosaic;
	int multipalette;
	uint32_t screenBase;
	int overflow;
	int size;
	int target1;
	int target2;
	uint16_t x;
	uint16_t y;
	int32_t refx;
	int32_t refy;
	int32_t offsetX;
	int32_t offsetY;

	struct GBAVideoGLAffine affine;

	GLint scanlineAffine[GBA_VIDEO_VERTICAL_PIXELS * 4];
	GLint scanlineOffset[GBA_VIDEO_VERTICAL_PIXELS];
};

enum {
	GBA_GL_FBO_OBJ = 0,
	GBA_GL_FBO_BACKDROP,
	GBA_GL_FBO_WINDOW,
	GBA_GL_FBO_OUTPUT,
	GBA_GL_FBO_MAX
};

enum {
	GBA_GL_TEX_OBJ_COLOR = 0,
	GBA_GL_TEX_OBJ_FLAGS,
	GBA_GL_TEX_OBJ_DEPTH,
	GBA_GL_TEX_BACKDROP,
	GBA_GL_TEX_WINDOW,
	GBA_GL_TEX_MAX
};

enum {
	GBA_GL_VS_LOC = 0,
	GBA_GL_VS_MAXPOS,

	GBA_GL_BG_VRAM = 2,
	GBA_GL_BG_PALETTE,
	GBA_GL_BG_SCREENBASE,
	GBA_GL_BG_CHARBASE,
	GBA_GL_BG_SIZE,
	GBA_GL_BG_OFFSET,
	GBA_GL_BG_TRANSFORM,
	GBA_GL_BG_RANGE,
	GBA_GL_BG_MOSAIC,
	GBA_GL_BG_OLDCHARBASE,

	GBA_GL_OBJ_VRAM = 2,
	GBA_GL_OBJ_PALETTE,
	GBA_GL_OBJ_CHARBASE,
	GBA_GL_OBJ_TILE,
	GBA_GL_OBJ_STRIDE,
	GBA_GL_OBJ_LOCALPALETTE,
	GBA_GL_OBJ_INFLAGS,
	GBA_GL_OBJ_TRANSFORM,
	GBA_GL_OBJ_DIMS,
	GBA_GL_OBJ_OBJWIN,
	GBA_GL_OBJ_MOSAIC,
	GBA_GL_OBJ_CYCLES,

	GBA_GL_WIN_DISPCNT = 2,
	GBA_GL_WIN_BLEND,
	GBA_GL_WIN_FLAGS,
	GBA_GL_WIN_WIN0,
	GBA_GL_WIN_WIN1,

	GBA_GL_FINALIZE_SCALE = 2,
	GBA_GL_FINALIZE_LAYERS,
	GBA_GL_FINALIZE_FLAGS,
	GBA_GL_FINALIZE_WINDOW,
	GBA_GL_FINALIZE_PALETTE,
	GBA_GL_FINALIZE_BACKDROP,

	GBA_GL_UNIFORM_MAX = 14
};

struct GBAVideoGLShader {
	GLuint program;
	GLuint vao;
	GLuint uniforms[GBA_GL_UNIFORM_MAX];
};

struct GBAVideoGLRenderer {
	struct GBAVideoRenderer d;

	uint32_t* temporaryBuffer;

	struct GBAVideoGLBackground bg[4];

	int oamMax;
	bool oamDirty;
	struct GBAVideoRendererSprite sprites[128];
	int16_t objOffsetX;
	int16_t objOffsetY;

	GLuint fbo[GBA_GL_FBO_MAX];
	GLuint layers[GBA_GL_TEX_MAX];
	GLuint vbo;

	GLuint outputTex;
	bool outputTexDirty;

	GLuint paletteTex;
	uint16_t shadowPalette[GBA_VIDEO_VERTICAL_PIXELS][512];
	int nextPalette;
	int paletteDirtyScanlines;
	bool paletteDirty;

	GLuint vramTex;
	unsigned vramDirty;

	uint16_t shadowRegs[0x30];
	uint64_t regsDirty;

	struct GBAVideoGLShader bgShader[6];
	struct GBAVideoGLShader objShader[3];
	struct GBAVideoGLShader windowShader;
	struct GBAVideoGLShader finalizeShader;

	GBARegisterDISPCNT dispcnt;

	unsigned target1Obj;
	unsigned target1Bd;
	unsigned target2Obj;
	unsigned target2Bd;
	enum GBAVideoBlendEffect blendEffect;
	uint16_t blda;
	uint16_t bldb;
	uint16_t bldy;

	GBAMosaicControl mosaic;

	struct GBAVideoGLWindowN {
		struct GBAVideoWindowRegion h;
		struct GBAVideoWindowRegion v;
		GBAWindowControl control;
		int16_t offsetX;
		int16_t offsetY;
	} winN[2];

	GLint winNHistory[2][GBA_VIDEO_VERTICAL_PIXELS * 4];
	GLint spriteCycles[GBA_VIDEO_VERTICAL_PIXELS];

	GBAWindowControl winout;
	GBAWindowControl objwin;

	int firstAffine;
	int firstY;

	int scale;
};

void GBAVideoGLRendererCreate(struct GBAVideoGLRenderer* renderer);
void GBAVideoGLRendererSetScale(struct GBAVideoGLRenderer* renderer, int scale);

#endif

CXX_GUARD_END

#endif
