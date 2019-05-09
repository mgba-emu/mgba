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
#include <mgba/internal/gba/video.h>

#ifdef USE_EPOXY
#include <epoxy/gl.h>
#elif defined(BUILD_GL)
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif
#else
#include <GLES2/gl2.h>
#endif

struct GBAVideoGLBackground {
	GLuint fbo;
	GLuint tex;
	GLuint program;

	unsigned index;
	int enabled;
	unsigned priority;
	uint32_t charBase;
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
	int16_t dx;
	int16_t dmx;
	int16_t dy;
	int16_t dmy;
	int32_t sx;
	int32_t sy;
};

struct GBAVideoGLRenderer {
	struct GBAVideoRenderer d;

	struct GBAVideoGLBackground bg[4];

	GLuint fbo[2];
	GLuint layers[2];

	color_t* outputBuffer;
	int outputBufferStride;

	GLuint paletteTex;
	bool paletteDirty;

	GLuint oamTex;
	bool oamDirty;

	GLuint vramTex;
	unsigned vramDirty;

	GLuint bgPrograms[6];
	GLuint objProgram;

	GLuint compositeProgram;

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
};

void GBAVideoGLRendererCreate(struct GBAVideoGLRenderer* renderer);

CXX_GUARD_END

#endif