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

#define MAX_NUM_QUADS 1024
#define VERTEX_BUFFER_SIZE MAX_NUM_QUADS * sizeof(struct ctrUIVertex)

static struct ctrUIVertex* ctrVertexBuffer = NULL;
static u16 ctrNumQuads = 0;

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

	// Set up TexEnv and other parameters
	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvOp(env, C3D_Both, 0, 0, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

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

	C3D_BufInfo* bufInfo = C3D_GetBufInfo();
	BufInfo_Init(bufInfo);
	BufInfo_Add(bufInfo, ctrVertexBuffer, sizeof(struct ctrUIVertex), 3, 0x210);

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

	C3D_TexBind(0, texture);
	activeTexture = texture;

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
	if (ctrNumQuads == MAX_NUM_QUADS) {
		ctrFlushBatch();
	}

	struct ctrUIVertex* vtx = &ctrVertexBuffer[ctrNumQuads];
	vtx->x = x;
	vtx->y = y;
	vtx->w = w;
	vtx->h = h;
	vtx->u = u;
	vtx->v = v;
	vtx->uw = uw;
	vtx->vh = vh;
	vtx->abgr = color;

	++ctrNumQuads;
}

void ctrAddRect(u32 color, s16 x, s16 y, s16 u, s16 v, s16 w, s16 h) {
	ctrAddRectScaled(color, x, y, w, h, u, v, w, h);
}

void ctrFlushBatch(void) {
	if (ctrNumQuads == 0) {
		return;
	}

	GSPGPU_FlushDataCache(ctrVertexBuffer, VERTEX_BUFFER_SIZE);
	C3D_DrawArrays(GPU_GEOMETRY_PRIM, 0, ctrNumQuads);
	C3D_Flush();

	ctrNumQuads = 0;
}
