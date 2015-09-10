/* Copyright (c) 2015 Yuri Kunde Schlesner
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <3ds.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ctr-gpu.h"

#include "uishader.h"
#include "uishader.shbin.h"

struct ctrUIVertex {
	s16 x,y;
	s16 u,v;
	u32 abgr;
};

#define VRAM_BASE 0x18000000u

#define MAX_NUM_QUADS 1024
#define COMMAND_LIST_LENGTH (16 * 1024)
// Each quad requires 4 vertices and 2*3 indices for the two triangles used to draw it
#define VERTEX_INDEX_BUFFER_SIZE (MAX_NUM_QUADS * (4 * sizeof(struct ctrUIVertex) + 6 * sizeof(u16)))

static struct ctrUIVertex* ctrVertexBuffer = NULL;
static u16* ctrIndexBuffer = NULL;
static u16 ctrNumQuads = 0;

static void* gpuColorBuffer = NULL;
static u32* gpuCommandList = NULL;
static void* screenTexture = NULL;

static shaderProgram_s gpuShader;
static DVLB_s* passthroughShader = NULL;

static const struct ctrTexture* activeTexture = NULL;

static u32 _f24FromFloat(float f) {
	u32 i;
	memcpy(&i, &f, 4);

	u32 mantissa = (i << 9) >>  9;
	s32 exponent = (i << 1) >> 24;
	u32 sign     = (i << 0) >> 31;

	// Truncate mantissa
	mantissa >>= 7;

	// Re-bias exponent
	exponent = exponent - 127 + 63;
	if (exponent < 0) {
		// Underflow: flush to zero
		return sign << 23;
	} else if (exponent > 0x7F) {
		// Overflow: saturate to infinity
		return sign << 23 | 0x7F << 16;
	}

	return sign << 23 | exponent << 16 | mantissa;
}

static u32 _f31FromFloat(float f) {
	u32 i;
	memcpy(&i, &f, 4);

	u32 mantissa = (i << 9) >>  9;
	s32 exponent = (i << 1) >> 24;
	u32 sign     = (i << 0) >> 31;

	// Re-bias exponent
	exponent = exponent - 127 + 63;
	if (exponent < 0) {
		// Underflow: flush to zero
		return sign << 30;
	} else if (exponent > 0x7F) {
		// Overflow: saturate to infinity
		return sign << 30 | 0x7F << 23;
	}

	return sign << 30 | exponent << 23 | mantissa;
}

// Replacements for the limiting GPU_SetViewport function in ctrulib
static void _GPU_SetFramebuffer(intptr_t colorBuffer, intptr_t depthBuffer, u16 w, u16 h) {
	u32 buf[4];

	// Unknown
	GPUCMD_AddWrite(GPUREG_0111, 0x00000001);
	GPUCMD_AddWrite(GPUREG_0110, 0x00000001);

	// Set depth/color buffer address and dimensions
	buf[0] = depthBuffer >> 3;
	buf[1] = colorBuffer >> 3;
	buf[2] = (0x01) << 24 | ((h-1) & 0xFFF) << 12 | (w & 0xFFF) << 0;
	GPUCMD_AddIncrementalWrites(GPUREG_DEPTHBUFFER_LOC, buf, 3);
	GPUCMD_AddWrite(GPUREG_006E, buf[2]);

	// Set depth/color buffer pixel format
	GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_FORMAT, 3 /* D248S */ );
	GPUCMD_AddWrite(GPUREG_COLORBUFFER_FORMAT, 0 /* RGBA8 */ << 16 | 2 /* Unknown */);
	GPUCMD_AddWrite(GPUREG_011B, 0); // Unknown

	// Enable color/depth buffers
	buf[0] = colorBuffer != 0 ? 0xF : 0x0;
	buf[1] = buf[0];
	buf[2] = depthBuffer != 0 ? 0x2 : 0x0;
	buf[3] = buf[2];
	GPUCMD_AddIncrementalWrites(GPUREG_0112, buf, 4);
}

static void _GPU_SetViewportEx(u16 x, u16 y, u16 w, u16 h) {
	u32 buf[4];

	buf[0] = _f24FromFloat(w / 2.0f);
	buf[1] = _f31FromFloat(2.0f / w) << 1;
	buf[2] = _f24FromFloat(h / 2.0f);
	buf[3] = _f31FromFloat(2.0f / h) << 1;
	GPUCMD_AddIncrementalWrites(GPUREG_0041, buf, 4);

	GPUCMD_AddWrite(GPUREG_0068, (y & 0xFFFF) << 16 | (x & 0xFFFF) << 0);

	buf[0] = 0;
	buf[1] = 0;
	buf[2] = ((h-1) & 0xFFFF) << 16 | ((w-1) & 0xFFFF) << 0;
	GPUCMD_AddIncrementalWrites(GPUREG_SCISSORTEST_MODE, buf, 3);
}

static void _setDummyTexEnv(int id) {
	GPU_SetTexEnv(id,
		GPU_TEVSOURCES(GPU_PREVIOUS, 0, 0),
		GPU_TEVSOURCES(GPU_PREVIOUS, 0, 0),
		GPU_TEVOPERANDS(0, 0, 0),
		GPU_TEVOPERANDS(0, 0, 0),
		GPU_REPLACE,
		GPU_REPLACE,
		0x00000000);
}

Result ctrInitGpu() {
	Result res = -1;

	// Allocate buffers
	gpuColorBuffer = vramAlloc(400 * 240 * 4);
	gpuCommandList = linearAlloc(COMMAND_LIST_LENGTH * sizeof(u32));
	ctrVertexBuffer = linearAlloc(VERTEX_INDEX_BUFFER_SIZE);
	if (gpuColorBuffer == NULL || gpuCommandList == NULL || ctrVertexBuffer == NULL) {
		res = -1;
		goto error_allocs;
	}
	// Both buffers share the same allocation, index buffer follows the vertex buffer
	ctrIndexBuffer = (u16*)(ctrVertexBuffer + (4 * MAX_NUM_QUADS));

	// Load vertex shader binary
	passthroughShader = DVLB_ParseFile((u32*)uishader, uishader_size);
	if (passthroughShader == NULL) {
		res = -1;
		goto error_dvlb;
	}

	// Create shader
	shaderProgramInit(&gpuShader);
	res = shaderProgramSetVsh(&gpuShader, &passthroughShader->DVLE[0]);
	if (res < 0) {
		goto error_shader;
	}

	// Initialize the GPU in ctrulib and assign the command buffer to accept submission of commands
	GPU_Init(NULL);
	GPUCMD_SetBuffer(gpuCommandList, COMMAND_LIST_LENGTH, 0);

	return 0;

error_shader:
	shaderProgramFree(&gpuShader);

error_dvlb:
	if (passthroughShader != NULL) {
		DVLB_Free(passthroughShader);
		passthroughShader = NULL;
	}

error_allocs:
	if (ctrVertexBuffer != NULL) {
		linearFree(ctrVertexBuffer);
		ctrVertexBuffer = NULL;
		ctrIndexBuffer = NULL;
	}

	if (gpuCommandList != NULL) {
		GPUCMD_SetBuffer(NULL, 0, 0);
		linearFree(gpuCommandList);
		gpuCommandList = NULL;
	}

	if (gpuColorBuffer != NULL) {
		vramFree(gpuColorBuffer);
		gpuColorBuffer = NULL;
	}
	return res;
}

void ctrDeinitGpu() {
	shaderProgramFree(&gpuShader);

	DVLB_Free(passthroughShader);
	passthroughShader = NULL;

	linearFree(screenTexture);
	screenTexture = NULL;

	linearFree(ctrVertexBuffer);
	ctrVertexBuffer = NULL;
	ctrIndexBuffer = NULL;

	GPUCMD_SetBuffer(NULL, 0, 0);
	linearFree(gpuCommandList);
	gpuCommandList = NULL;

	vramFree(gpuColorBuffer);
	gpuColorBuffer = NULL;
}

void ctrGpuBeginFrame(void) {
	shaderProgramUse(&gpuShader);

	void* gpuColorBufferEnd = (char*)gpuColorBuffer + 240 * 400 * 4;

	GX_SetMemoryFill(NULL,
			gpuColorBuffer, 0x00000000, gpuColorBufferEnd, GX_FILL_32BIT_DEPTH | GX_FILL_TRIGGER,
			NULL, 0, NULL, 0);
	gspWaitForPSC0();

	_GPU_SetFramebuffer(osConvertVirtToPhys((u32)gpuColorBuffer), 0, 240, 400);

	// Disable depth and stencil testing
	GPU_SetDepthTestAndWriteMask(false, GPU_ALWAYS, GPU_WRITE_COLOR);
	GPU_SetStencilTest(false, GPU_ALWAYS, 0, 0xFF, 0);
	GPU_SetStencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
	GPU_DepthMap(-1.0f, 0.0f);

	// Enable alpha blending
	GPU_SetAlphaBlending(
			GPU_BLEND_ADD, GPU_BLEND_ADD, // Operation RGB, Alpha
			GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, // Color src, dst
			GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA); // Alpha src, dst
	GPU_SetBlendingColor(0, 0, 0, 0);

	// Disable alpha testing
	GPU_SetAlphaTest(false, GPU_ALWAYS, 0);

	// Unknown
	GPUCMD_AddMaskedWrite(GPUREG_0062, 0x1, 0);
	GPUCMD_AddWrite(GPUREG_0118, 0);

	GPU_SetTexEnv(0,
			GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0), // RGB
			GPU_TEVSOURCES(GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0), // Alpha
			GPU_TEVOPERANDS(0, 0, 0), // RGB
			GPU_TEVOPERANDS(0, 0, 0), // Alpha
			GPU_MODULATE, GPU_MODULATE, // Operation RGB, Alpha
			0x00000000); // Constant color
	_setDummyTexEnv(1);
	_setDummyTexEnv(2);
	_setDummyTexEnv(3);
	_setDummyTexEnv(4);
	_setDummyTexEnv(5);

	// Configure vertex attribute format
	u32 bufferOffsets[] = { osConvertVirtToPhys((u32)ctrVertexBuffer) - VRAM_BASE };
	u64 arrayTargetAttributes[] = { 0x210 };
	u8 numAttributesInArray[] = { 3 };
	GPU_SetAttributeBuffers(
			3, // Number of attributes
			(u32*)VRAM_BASE, // Base address
			GPU_ATTRIBFMT(0, 2, GPU_SHORT) | // Attribute format
				GPU_ATTRIBFMT(1, 2, GPU_SHORT) |
				GPU_ATTRIBFMT(2, 4, GPU_UNSIGNED_BYTE),
			0xFF8, // Non-fixed vertex inputs
			0x210, // Vertex shader input map
			1, // Use 1 vertex array
			bufferOffsets, arrayTargetAttributes, numAttributesInArray);
}

void ctrGpuEndFrame(void* outputFramebuffer, int w, int h) {
	ctrFlushBatch();

	void* colorBuffer = (u8*)gpuColorBuffer + ((400 - w) * 240 * 4);

	const u32 GX_CROP_INPUT_LINES = (1 << 2);

	GX_SetDisplayTransfer(NULL,
			colorBuffer,       GX_BUFFER_DIM(240, 400),
			outputFramebuffer, GX_BUFFER_DIM(h, w),
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
				GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
				GX_CROP_INPUT_LINES);
	gspWaitForPPF();
}

void ctrSetViewportSize(s16 w, s16 h) {
	// Set up projection matrix mapping (0,0) to the top-left and (w,h) to the
	// bottom-right, taking into account the 3DS' screens' portrait
	// orientation.
	float projectionMtx[4 * 4] = {
		// Rows are in the order w z y x, because ctrulib
		1.0f, 0.0f, -2.0f / h, 0.0f,
		1.0f, 0.0f, 0.0f, -2.0f / w,
		-0.5f, 0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
	};

	GPU_SetFloatUniform(GPU_VERTEX_SHADER, VSH_FVEC_projectionMtx, (u32*)&projectionMtx, 4);
	_GPU_SetViewportEx(0, 0, h, w);
}

void ctrActivateTexture(const struct ctrTexture* texture) {
	if (activeTexture == texture) {
		return;
	}

	ctrFlushBatch();

	GPU_SetTextureEnable(GPU_TEXUNIT0);
	GPU_SetTexture(
			GPU_TEXUNIT0, (u32*)osConvertVirtToPhys((u32)texture->data),
			texture->width, texture->height,
			GPU_TEXTURE_MAG_FILTER(texture->filter) | GPU_TEXTURE_MIN_FILTER(texture->filter) |
				GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_BORDER) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_BORDER),
			texture->format);
	GPU_SetTextureBorderColor(GPU_TEXUNIT0, 0x00000000);

	float textureMtx[2 * 4] = {
		// Rows are in the order w z y x, because ctrulib
		0.0f, 0.0f, 0.0f, 1.0f / texture->width,
		0.0f, 0.0f, 1.0f / texture->height, 0.0f,
	};

	GPU_SetFloatUniform(GPU_VERTEX_SHADER, VSH_FVEC_textureMtx, (u32*)&textureMtx, 2);

	activeTexture = texture;
}

void ctrAddRectScaled(u32 color, s16 x, s16 y, s16 w, s16 h, s16 u, s16 v, s16 uw, s16 vh) {
	if (ctrNumQuads == MAX_NUM_QUADS) {
		ctrFlushBatch();
	}

	u16 index = ctrNumQuads * 4;
	struct ctrUIVertex* vtx = &ctrVertexBuffer[index];
	vtx->x = x; vtx->y = y;
	vtx->u = u; vtx->v = v;
	vtx->abgr = color;
	vtx++;

	vtx->x = x + w; vtx->y = y;
	vtx->u = u + uw; vtx->v = v;
	vtx->abgr = color;
	vtx++;

	vtx->x = x; vtx->y = y + h;
	vtx->u = u; vtx->v = v + vh;
	vtx->abgr = color;
	vtx++;

	vtx->x = x + w; vtx->y = y + h;
	vtx->u = u + uw; vtx->v = v + vh;
	vtx->abgr = color;

	u16* i = &ctrIndexBuffer[ctrNumQuads * 6];
	i[0] = index + 0; i[1] = index + 1; i[2] = index + 2;
	i[3] = index + 2; i[4] = index + 1; i[5] = index + 3;

	ctrNumQuads += 1;
}

void ctrAddRect(u32 color, s16 x, s16 y, s16 u, s16 v, s16 w, s16 h) {
	ctrAddRectScaled(color,
			x, y, w, h,
			u, v, w, h);
}

void ctrFlushBatch(void) {
	if (ctrNumQuads == 0) {
		return;
	}

	GSPGPU_FlushDataCache(NULL, (u8*)ctrVertexBuffer, VERTEX_INDEX_BUFFER_SIZE);
	GPU_DrawElements(GPU_UNKPRIM, (u32*)(osConvertVirtToPhys((u32)ctrIndexBuffer) - VRAM_BASE), ctrNumQuads * 6);

	GPU_FinishDrawing();
	GPUCMD_Finalize();
	GSPGPU_FlushDataCache(NULL, (u8*)gpuCommandList, COMMAND_LIST_LENGTH * sizeof(u32));
	GPUCMD_FlushAndRun(NULL);

	gspWaitForP3D();

	GPUCMD_SetBufferOffset(0);

	ctrNumQuads = 0;
}
