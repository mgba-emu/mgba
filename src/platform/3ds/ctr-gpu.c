/* Copyright (c) 2015 Yuri Kunde Schlesner
 * Copyright (c) 2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <3ds.h>
#include <3ds/gpu/gpu.h>
#include <3ds/gpu/gx.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ctr-gpu.h"

#include "uishader.h"
#include "uishader.shbin.h"

struct ctrUIVertex {
	short x, y;
	short w, h;
	short u, v;
	short uw, vh;
	u32 abgr;
	float rotate[2];
};

#define MAX_NUM_QUADS 4096
#define VERTEX_BUFFER_SIZE MAX_NUM_QUADS * sizeof(struct ctrUIVertex)

static struct ctrUIVertex* ctrVertexBuffer = NULL;
static int ctrNumVerts = 0;
static int ctrVertStart = 0;

static const C3D_Tex* activeTexture = NULL;

static shaderProgram_s uiProgram;
static DVLB_s* uiShader = NULL;
static int GSH_FVEC_projectionMtx;
static int GSH_FVEC_textureMtx;

bool ctrInitGpu(void) {
	// Load vertex shader binary
	uiShader = DVLB_ParseFile((u32*) uishader, uishader_size);
	if (uiShader == NULL) {
		return false;
	}

	// Create shader
	shaderProgramInit(&uiProgram);
	Result res = shaderProgramSetVsh(&uiProgram, &uiShader->DVLE[0]);
	if (res < 0) {
		return false;
	}
	res = shaderProgramSetGsh(&uiProgram, &uiShader->DVLE[1], 4);
	if (res < 0) {
		return false;
	}
	C3D_BindProgram(&uiProgram);
	GSH_FVEC_projectionMtx = shaderInstanceGetUniformLocation(uiProgram.geometryShader, "projectionMtx");
	GSH_FVEC_textureMtx = shaderInstanceGetUniformLocation(uiProgram.geometryShader, "textureMtx");

	// Allocate buffers
	ctrVertexBuffer = linearAlloc(VERTEX_BUFFER_SIZE);
	if (ctrVertexBuffer == NULL) {
		return false;
	}

	C3D_CullFace(GPU_CULL_NONE);
	C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
	C3D_AlphaTest(false, GPU_ALWAYS, 0);
	C3D_BlendingColor(0);

	C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
	AttrInfo_Init(attrInfo);
	AttrInfo_AddLoader(attrInfo, 0, GPU_SHORT, 4); // in_pos
	AttrInfo_AddLoader(attrInfo, 1, GPU_SHORT, 4); // in_tc0
	AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4); // in_col
	AttrInfo_AddLoader(attrInfo, 3, GPU_FLOAT, 2); // in_rot

	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, ctrVertexBuffer, sizeof(struct ctrUIVertex), 4, 0x3210);

	return true;
}

void ctrDeinitGpu(void) {
	if (ctrVertexBuffer) {
		linearFree(ctrVertexBuffer);
		ctrVertexBuffer = NULL;
	}

	shaderProgramFree(&uiProgram);

	if (uiShader) {
		DVLB_Free(uiShader);
		uiShader = NULL;
	}
}

void ctrSetViewportSize(s16 w, s16 h, bool tilt) {
	C3D_SetViewport(0, 0, h, w);
	C3D_Mtx projectionMtx;
	if (tilt) {
		Mtx_OrthoTilt(&projectionMtx, 0.0, w, h, 0.0, 0.0, 1.0, true);
	} else {
		Mtx_Ortho(&projectionMtx, 0.0, w, 0.0, h, 0.0, 1.0, true);
	}
	C3D_FVUnifMtx4x4(GPU_GEOMETRY_SHADER, GSH_FVEC_projectionMtx, &projectionMtx);
}

void ctrFlushBatch(void) {
	int thisBatch = ctrNumVerts - ctrVertStart;
	if (!thisBatch) {
		return;
	}
	if (thisBatch < 0) {
		svcBreak(USERBREAK_PANIC);
	}
	C3D_DrawArrays(GPU_GEOMETRY_PRIM, ctrVertStart, thisBatch);
	ctrVertStart = ctrNumVerts;
}

void ctrActivateTexture(const C3D_Tex* texture) {
	if (texture == activeTexture) {
		return;
	}

	if (activeTexture) {
		ctrFlushBatch();
	}

	activeTexture = texture;
	C3D_TexBind(0, activeTexture);

	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	if (texture->fmt < GPU_LA8) {
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	} else {
		C3D_TexEnvSrc(env, C3D_RGB, GPU_PRIMARY_COLOR, 0, 0);
		C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
		C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
		C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
	}
	env = C3D_GetTexEnv(1);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
	env = C3D_GetTexEnv(2);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	C3D_Mtx textureMtx = {
		.m = {
			// Rows are in the order w z y x, because ctrulib
			0.0f, 0.0f, 0.0f, 1.0f / activeTexture->width,
			0.0f, 0.0f, 1.0f / activeTexture->height, 0.0f 
		}
	};
	C3D_FVUnifMtx2x4(GPU_GEOMETRY_SHADER, GSH_FVEC_textureMtx, &textureMtx);
}

void ctrTextureMultiply(void) {
	C3D_TexEnv* env = C3D_GetTexEnv(1);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_TEXTURE0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
}

void ctrTextureBias(u32 color) {
	C3D_TexEnv* env = C3D_GetTexEnv(2);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_CONSTANT, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_ADD);
	C3D_TexEnvColor(env, color);
}

void ctrAddRectEx(u32 color, s16 x, s16 y, s16 w, s16 h, s16 u, s16 v, s16 uw, s16 vh, float rotate) {
	if (x >= 400 && w >= 0) {
		return;
	}
	if (y >= 240 && h >= 0) {
		return;
	}

	if (ctrNumVerts == MAX_NUM_QUADS) {
		abort();
	}

	struct ctrUIVertex* vtx = &ctrVertexBuffer[ctrNumVerts];
	vtx->x = x;
	vtx->y = y;
	vtx->w = w;
	vtx->h = h;
	vtx->u = u;
	vtx->v = v;
	vtx->uw = uw;
	vtx->vh = vh;
	vtx->abgr = color;
	vtx->rotate[0] = cosf(rotate);
	vtx->rotate[1] = sinf(rotate);

	++ctrNumVerts;
}

void ctrAddRect(u32 color, s16 x, s16 y, s16 u, s16 v, s16 w, s16 h) {
	ctrAddRectEx(color, x, y, w, h, u, v, w, h, 0);
}

void ctrStartFrame(void) {
	ctrNumVerts = 0;
	ctrVertStart = 0;
	activeTexture = NULL;
}

void ctrEndFrame(void) {
	ctrFlushBatch();
}
