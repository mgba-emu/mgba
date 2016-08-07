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

#include "uishader_v.h"
#include "uishader_v.shbin.h"
#include "uishader_g.h"
#include "uishader_g.shbin.h"

struct ctrUIVertex {
	short x, y;
	short w, h;
	short u, v;
	short uw, vh;
	u32 abgr;
};

#define MAX_NUM_QUADS 256
#define VERTEX_BUFFER_SIZE MAX_NUM_QUADS * sizeof(struct ctrUIVertex)

static struct ctrUIVertex* ctrVertexBuffer = NULL;
static int ctrNumVerts = 0;
static int ctrVertStart = 0;

static C3D_Tex* activeTexture = NULL;

static shaderProgram_s gpuShader;
static DVLB_s* vertexShader = NULL;
static DVLB_s* geometryShader = NULL;

bool ctrInitGpu() {
	// Load vertex shader binary
	vertexShader = DVLB_ParseFile((u32*) uishader_v, uishader_v_size);
	if (vertexShader == NULL) {
		return false;
	}

	// Load geometry shader binary
	geometryShader = DVLB_ParseFile((u32*) uishader_g, uishader_g_size);
	if (geometryShader == NULL) {
		return false;
	}

	// Create shader
	shaderProgramInit(&gpuShader);
	Result res = shaderProgramSetVsh(&gpuShader, &vertexShader->DVLE[0]);
	if (res < 0) {
		return false;
	}
	res = shaderProgramSetGsh(&gpuShader, &geometryShader->DVLE[0], 3);
	if (res < 0) {
		return false;
	}
	C3D_BindProgram(&gpuShader);

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

	return true;
}

void ctrDeinitGpu() {
	if (ctrVertexBuffer) {
		linearFree(ctrVertexBuffer);
		ctrVertexBuffer = NULL;
	}

	shaderProgramFree(&gpuShader);

	if (vertexShader) {
		DVLB_Free(vertexShader);
		vertexShader = NULL;
	}

	if (geometryShader) {
		DVLB_Free(geometryShader);
		geometryShader = NULL;
	}
}

void ctrSetViewportSize(s16 w, s16 h) {
	C3D_SetViewport(0, 0, h, w);
	C3D_Mtx projectionMtx;
	Mtx_OrthoTilt(&projectionMtx, 0.0, w, h, 0.0, 0.0, 1.0);
	C3D_FVUnifMtx4x4(GPU_GEOMETRY_SHADER, GSH_FVEC_projectionMtx, &projectionMtx);
}

void ctrActivateTexture(C3D_Tex* texture) {
	if (texture == activeTexture) {
		return;
	}
	if (activeTexture) {
		ctrFlushBatch();
	}

	activeTexture = texture;
	C3D_TexBind(0, activeTexture);

	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	if (texture->fmt < GPU_LA8) {
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	} else {
		C3D_TexEnvSrc(env, C3D_RGB, GPU_PRIMARY_COLOR, 0, 0);
		C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
		C3D_TexEnvFunc(env, C3D_RGB, GPU_REPLACE);
		C3D_TexEnvFunc(env, C3D_Alpha, GPU_MODULATE);
	}

	C3D_Mtx textureMtx = {
		.m = {
			// Rows are in the order w z y x, because ctrulib
			0.0f, 0.0f, 0.0f, 1.0f / activeTexture->width,
			0.0f, 0.0f, 1.0f / activeTexture->height, 0.0f 
		}
	};
	C3D_FVUnifMtx2x4(GPU_GEOMETRY_SHADER, GSH_FVEC_textureMtx, &textureMtx);
}

void ctrAddRectScaled(u32 color, s16 x, s16 y, s16 w, s16 h, s16 u, s16 v, s16 uw, s16 vh) {
	if (x >= 400 && w >= 0) {
		return;
	}
	if (y >= 240 && h >= 0) {
		return;
	}

	if (ctrNumVerts + ctrVertStart == MAX_NUM_QUADS) {
		ctrFlushBatch();
		C3D_Flush();
		ctrNumVerts = 0;
		ctrVertStart = 0;
	}

	struct ctrUIVertex* vtx = &ctrVertexBuffer[ctrVertStart + ctrNumVerts];
	vtx->x = x;
	vtx->y = y;
	vtx->w = w;
	vtx->h = h;
	vtx->u = u;
	vtx->v = v;
	vtx->uw = uw;
	vtx->vh = vh;
	vtx->abgr = color;

	++ctrNumVerts;
}

void ctrAddRect(u32 color, s16 x, s16 y, s16 u, s16 v, s16 w, s16 h) {
	ctrAddRectScaled(color, x, y, w, h, u, v, w, h);
}

void ctrFlushBatch(void) {
	if (ctrNumVerts == 0) {
		return;
	}

	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, &ctrVertexBuffer[ctrVertStart], sizeof(struct ctrUIVertex), 3, 0x210);

	GSPGPU_FlushDataCache(&ctrVertexBuffer[ctrVertStart], sizeof(struct ctrUIVertex) * ctrNumVerts);
	C3D_DrawArrays(GPU_GEOMETRY_PRIM, 0, ctrNumVerts);

	ctrVertStart += ctrNumVerts;
	ctrNumVerts = 0;
}

void ctrFinalize(void) {
	ctrFlushBatch();
	C3D_Flush();
	ctrNumVerts = 0;
	ctrVertStart = 0;
}
