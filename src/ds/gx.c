/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/gx.h>

#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/io.h>

mLOG_DEFINE_CATEGORY(DS_GX, "DS GX");

#define DS_GX_FIFO_SIZE 256
#define DS_GX_PIPE_SIZE 4

static void DSGXDummyRendererInit(struct DSGXRenderer* renderer);
static void DSGXDummyRendererReset(struct DSGXRenderer* renderer);
static void DSGXDummyRendererDeinit(struct DSGXRenderer* renderer);
static void DSGXDummyRendererInvalidateTex(struct DSGXRenderer* renderer, int slot);
static void DSGXDummyRendererSetRAM(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXPolygon* polys, unsigned polyCount);
static void DSGXDummyRendererDrawScanline(struct DSGXRenderer* renderer, int y);
static void DSGXDummyRendererGetScanline(struct DSGXRenderer* renderer, int y, color_t** output);

static void DSGXWriteFIFO(struct DSGX* gx, struct DSGXEntry entry);

static const int32_t _gxCommandCycleBase[DS_GX_CMD_MAX] = {
	[DS_GX_CMD_NOP] = 0,
	[DS_GX_CMD_MTX_MODE] = 2,
	[DS_GX_CMD_MTX_PUSH] = 34,
	[DS_GX_CMD_MTX_POP] = 72,
	[DS_GX_CMD_MTX_STORE] = 34,
	[DS_GX_CMD_MTX_RESTORE] = 72,
	[DS_GX_CMD_MTX_IDENTITY] = 38,
	[DS_GX_CMD_MTX_LOAD_4x4] = 68,
	[DS_GX_CMD_MTX_LOAD_4x3] = 60,
	[DS_GX_CMD_MTX_MULT_4x4] = 70,
	[DS_GX_CMD_MTX_MULT_4x3] = 62,
	[DS_GX_CMD_MTX_MULT_3x3] = 56,
	[DS_GX_CMD_MTX_SCALE] = 44,
	[DS_GX_CMD_MTX_TRANS] = 44,
	[DS_GX_CMD_COLOR] = 2,
	[DS_GX_CMD_NORMAL] = 18,
	[DS_GX_CMD_TEXCOORD] = 2,
	[DS_GX_CMD_VTX_16] = 18,
	[DS_GX_CMD_VTX_10] = 16,
	[DS_GX_CMD_VTX_XY] = 16,
	[DS_GX_CMD_VTX_XZ] = 16,
	[DS_GX_CMD_VTX_YZ] = 16,
	[DS_GX_CMD_VTX_DIFF] = 16,
	[DS_GX_CMD_POLYGON_ATTR] = 2,
	[DS_GX_CMD_TEXIMAGE_PARAM] = 2,
	[DS_GX_CMD_PLTT_BASE] = 2,
	[DS_GX_CMD_DIF_AMB] = 8,
	[DS_GX_CMD_SPE_EMI] = 8,
	[DS_GX_CMD_LIGHT_VECTOR] = 12,
	[DS_GX_CMD_LIGHT_COLOR] = 2,
	[DS_GX_CMD_SHININESS] = 64,
	[DS_GX_CMD_BEGIN_VTXS] = 2,
	[DS_GX_CMD_END_VTXS] = 2,
	[DS_GX_CMD_SWAP_BUFFERS] = 784,
	[DS_GX_CMD_VIEWPORT] = 2,
	[DS_GX_CMD_BOX_TEST] = 206,
	[DS_GX_CMD_POS_TEST] = 18,
	[DS_GX_CMD_VEC_TEST] = 10,
};

static const int32_t _gxCommandParams[DS_GX_CMD_MAX] = {
	[DS_GX_CMD_MTX_MODE] = 1,
	[DS_GX_CMD_MTX_POP] = 1,
	[DS_GX_CMD_MTX_STORE] = 1,
	[DS_GX_CMD_MTX_RESTORE] = 1,
	[DS_GX_CMD_MTX_LOAD_4x4] = 16,
	[DS_GX_CMD_MTX_LOAD_4x3] = 12,
	[DS_GX_CMD_MTX_MULT_4x4] = 16,
	[DS_GX_CMD_MTX_MULT_4x3] = 12,
	[DS_GX_CMD_MTX_MULT_3x3] = 9,
	[DS_GX_CMD_MTX_SCALE] = 3,
	[DS_GX_CMD_MTX_TRANS] = 3,
	[DS_GX_CMD_COLOR] = 1,
	[DS_GX_CMD_NORMAL] = 1,
	[DS_GX_CMD_TEXCOORD] = 1,
	[DS_GX_CMD_VTX_16] = 2,
	[DS_GX_CMD_VTX_10] = 1,
	[DS_GX_CMD_VTX_XY] = 1,
	[DS_GX_CMD_VTX_XZ] = 1,
	[DS_GX_CMD_VTX_YZ] = 1,
	[DS_GX_CMD_VTX_DIFF] = 1,
	[DS_GX_CMD_POLYGON_ATTR] = 1,
	[DS_GX_CMD_TEXIMAGE_PARAM] = 1,
	[DS_GX_CMD_PLTT_BASE] = 1,
	[DS_GX_CMD_DIF_AMB] = 1,
	[DS_GX_CMD_SPE_EMI] = 1,
	[DS_GX_CMD_LIGHT_VECTOR] = 1,
	[DS_GX_CMD_LIGHT_COLOR] = 1,
	[DS_GX_CMD_SHININESS] = 32,
	[DS_GX_CMD_BEGIN_VTXS] = 1,
	[DS_GX_CMD_SWAP_BUFFERS] = 1,
	[DS_GX_CMD_VIEWPORT] = 1,
	[DS_GX_CMD_BOX_TEST] = 3,
	[DS_GX_CMD_POS_TEST] = 2,
	[DS_GX_CMD_VEC_TEST] = 1,
};

static struct DSGXRenderer dummyRenderer = {
	.init = DSGXDummyRendererInit,
	.reset = DSGXDummyRendererReset,
	.deinit = DSGXDummyRendererDeinit,
	.invalidateTex = DSGXDummyRendererInvalidateTex,
	.setRAM = DSGXDummyRendererSetRAM,
	.drawScanline = DSGXDummyRendererDrawScanline,
	.getScanline = DSGXDummyRendererGetScanline,
};

static void _pullPipe(struct DSGX* gx) {
	if (CircleBufferSize(&gx->fifo) >= sizeof(struct DSGXEntry)) {
		struct DSGXEntry entry = { 0 };
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.command);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[0]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[1]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[2]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[3]);
		CircleBufferWrite8(&gx->pipe, entry.command);
		CircleBufferWrite8(&gx->pipe, entry.params[0]);
		CircleBufferWrite8(&gx->pipe, entry.params[1]);
		CircleBufferWrite8(&gx->pipe, entry.params[2]);
		CircleBufferWrite8(&gx->pipe, entry.params[3]);
	}
	if (CircleBufferSize(&gx->fifo) >= sizeof(struct DSGXEntry)) {
		struct DSGXEntry entry = { 0 };
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.command);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[0]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[1]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[2]);
		CircleBufferRead8(&gx->fifo, (int8_t*) &entry.params[3]);
		CircleBufferWrite8(&gx->pipe, entry.command);
		CircleBufferWrite8(&gx->pipe, entry.params[0]);
		CircleBufferWrite8(&gx->pipe, entry.params[1]);
		CircleBufferWrite8(&gx->pipe, entry.params[2]);
		CircleBufferWrite8(&gx->pipe, entry.params[3]);
	}
}

static void _updateClipMatrix(struct DSGX* gx) {
	DSGXMtxMultiply(&gx->clipMatrix, &gx->posMatrix, &gx->projMatrix);
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_00 >> 1] = gx->clipMatrix.m[0];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_01 >> 1] = gx->clipMatrix.m[0] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_02 >> 1] = gx->clipMatrix.m[1];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_03 >> 1] = gx->clipMatrix.m[1] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_04 >> 1] = gx->clipMatrix.m[2];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_05 >> 1] = gx->clipMatrix.m[2] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_06 >> 1] = gx->clipMatrix.m[3];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_07 >> 1] = gx->clipMatrix.m[3] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_08 >> 1] = gx->clipMatrix.m[4];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_09 >> 1] = gx->clipMatrix.m[4] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_0A >> 1] = gx->clipMatrix.m[5];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_0B >> 1] = gx->clipMatrix.m[5] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_0C >> 1] = gx->clipMatrix.m[6];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_0D >> 1] = gx->clipMatrix.m[6] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_0E >> 1] = gx->clipMatrix.m[7];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_0F >> 1] = gx->clipMatrix.m[7] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_10 >> 1] = gx->clipMatrix.m[8];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_11 >> 1] = gx->clipMatrix.m[8] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_12 >> 1] = gx->clipMatrix.m[9];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_13 >> 1] = gx->clipMatrix.m[9] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_14 >> 1] = gx->clipMatrix.m[10];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_15 >> 1] = gx->clipMatrix.m[10] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_16 >> 1] = gx->clipMatrix.m[11];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_17 >> 1] = gx->clipMatrix.m[11] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_18 >> 1] = gx->clipMatrix.m[12];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_19 >> 1] = gx->clipMatrix.m[12] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_1A >> 1] = gx->clipMatrix.m[13];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_1B >> 1] = gx->clipMatrix.m[13] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_1C >> 1] = gx->clipMatrix.m[14];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_1D >> 1] = gx->clipMatrix.m[14] >> 16;
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_1E >> 1] = gx->clipMatrix.m[15];
	gx->p->memory.io9[DS9_REG_CLIPMTX_RESULT_1F >> 1] = gx->clipMatrix.m[15] >> 16;
}

static int32_t _dotViewport(struct DSGXVertex* vertex, int32_t* col) {
	int64_t a;
	int64_t b;
	int64_t sum;
	a = col[0];
	b = vertex->x;
	sum = a * b;
	a = col[4];
	b = vertex->y;
	sum += a * b;
	a = col[8];
	b = vertex->z;
	sum += a * b;
	a = col[12];
	b = MTX_ONE;
	sum += a * b;
	return sum >> 8LL;
}

static int16_t _dotTexture(struct DSGXVertex* vertex, int mode, int32_t* col) {
	int64_t a;
	int64_t b;
	int64_t sum;
	switch (mode) {
	case 1:
		a = col[0];
		b = vertex->s;
		sum = a * b;
		a = col[4];
		b = vertex->t;
		sum += a * b;
		a = col[8];
		b = MTX_ONE >> 4;
		sum += a * b;
		a = col[12];
		b = MTX_ONE >> 4;
		sum += a * b;
		break;
	case 2:
		return 0;
	case 3:
		a = col[0];
		b = vertex->vx;
		sum = a * b;
		a = col[4];
		b = vertex->vy;
		sum += a * b;
		a = col[8];
		b = vertex->vz;
		sum += a * b;
		a = col[12];
		b = MTX_ONE;
		sum += a * b;
	}
	return sum >> 12;
}

static void _emitVertex(struct DSGX* gx, uint16_t x, uint16_t y, uint16_t z) {
	if (gx->vertexMode < 0 || gx->vertexIndex == DS_GX_VERTEX_BUFFER_SIZE || gx->polygonIndex == DS_GX_POLYGON_BUFFER_SIZE) {
		return;
	}
	gx->currentVertex.x = x;
	gx->currentVertex.y = y;
	gx->currentVertex.z = z;
	gx->currentVertex.vx = _dotViewport(&gx->currentVertex, &gx->clipMatrix.m[0]);
	gx->currentVertex.vy = _dotViewport(&gx->currentVertex, &gx->clipMatrix.m[1]);
	gx->currentVertex.vz = _dotViewport(&gx->currentVertex, &gx->clipMatrix.m[2]);
	gx->currentVertex.vw = _dotViewport(&gx->currentVertex, &gx->clipMatrix.m[3]);

	// TODO: What to do if w is 0?

	gx->currentVertex.vx = (gx->currentVertex.vx + gx->currentVertex.vw) * (int64_t) (gx->viewportWidth << 12) / (gx->currentVertex.vw * 2) + (gx->viewportX1 << 12);
	gx->currentVertex.vy = (gx->currentVertex.vy + gx->currentVertex.vw) * (int64_t) (gx->viewportHeight << 12) / (gx->currentVertex.vw * 2) + (gx->viewportY1 << 12);
	gx->currentVertex.vw = 0x40000000 / gx->currentVertex.vw;

	if (DSGXTexParamsGetCoordTfMode(gx->currentPoly.texParams) > 0) {
		int32_t m12 = gx->texMatrix.m[12];
		int32_t m13 = gx->texMatrix.m[13];
		if (DSGXTexParamsGetCoordTfMode(gx->currentPoly.texParams) > 1) {
			gx->texMatrix.m[12] = gx->currentVertex.vs;
			gx->texMatrix.m[13] = gx->currentVertex.vt;
		}
		gx->currentVertex.vs = _dotTexture(&gx->currentVertex, DSGXTexParamsGetCoordTfMode(gx->currentPoly.texParams), &gx->texMatrix.m[0]);
		gx->currentVertex.vt = _dotTexture(&gx->currentVertex, DSGXTexParamsGetCoordTfMode(gx->currentPoly.texParams), &gx->texMatrix.m[1]);
		gx->texMatrix.m[12] = m12;
		gx->texMatrix.m[13] = m13;
	} else {
		gx->currentVertex.vs = gx->currentVertex.s;
		gx->currentVertex.vt = gx->currentVertex.t;
	}

	struct DSGXVertex* vbuf = gx->vertexBuffer[gx->bufferIndex];
	vbuf[gx->vertexIndex] = gx->currentVertex;

	gx->currentPoly.vertIds[gx->currentPoly.verts] = gx->vertexIndex;

	++gx->vertexIndex;
	++gx->currentPoly.verts;
	int totalVertices;
	switch (gx->vertexMode) {
	case 0:
	case 2:
		totalVertices = 3;
		break;
	case 1:
	case 3:
		totalVertices = 4;
		break;
	}
	if (gx->currentPoly.verts == totalVertices) {
		struct DSGXPolygon* pbuf = gx->polygonBuffer[gx->bufferIndex];
		pbuf[gx->polygonIndex] = gx->currentPoly;

		switch (gx->vertexMode) {
		case 0:
		case 1:
			gx->currentPoly.verts = 0;
			break;
		case 2:
			gx->currentPoly.vertIds[0] = gx->currentPoly.vertIds[1];
			gx->currentPoly.vertIds[1] = gx->currentPoly.vertIds[2];
			gx->currentPoly.verts = 2;
			break;
		case 3:
			gx->currentPoly.vertIds[0] = gx->currentPoly.vertIds[2];
			gx->currentPoly.vertIds[1] = gx->currentPoly.vertIds[3];
			// Ensure quads don't cross over
			pbuf[gx->polygonIndex].vertIds[2] = gx->currentPoly.vertIds[3];
			pbuf[gx->polygonIndex].vertIds[3] = gx->currentPoly.vertIds[2];
			gx->currentPoly.verts = 2;
			break;
		}
		++gx->polygonIndex;
	}
}

static void _flushOutstanding(struct DSGX* gx) {
	if (CircleBufferSize(&gx->fifo) == (DS_GX_FIFO_SIZE * sizeof(struct DSGXEntry))) {
		return;
	}
	if (gx->p->cpuBlocked & DS_CPU_BLOCK_GX) {
		gx->p->cpuBlocked &= ~DS_CPU_BLOCK_GX;
		DSGXWriteFIFO(gx, gx->outstandingEntry);
		gx->outstandingEntry.command = 0;
	}
	while (gx->outstandingCommand[0] && !gx->outstandingParams[0]) {
		if (CircleBufferSize(&gx->fifo) == (DS_GX_FIFO_SIZE * sizeof(struct DSGXEntry))) {
			return;
		}
		DSGXWriteFIFO(gx, (struct DSGXEntry) { 0 });
	}
}

static void _fifoRun(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSGX* gx = context;
	uint32_t cycles;
	bool first = true;
	while (!gx->swapBuffers) {
		if (CircleBufferSize(&gx->pipe) <= 2 * sizeof(struct DSGXEntry)) {
			_pullPipe(gx);
		}

		if (!CircleBufferSize(&gx->pipe)) {
			cycles = 0;
			break;
		}

		DSRegGXSTAT gxstat = gx->p->memory.io9[DS9_REG_GXSTAT_LO >> 1];
		int projMatrixPointer = DSRegGXSTATGetProjMatrixStackLevel(gxstat);

		struct DSGXEntry entry = { 0 };
		CircleBufferDump(&gx->pipe, (int8_t*) &entry.command, 1);
		cycles = _gxCommandCycleBase[entry.command];

		if (first) {
			first = false;
		} else if (!gx->activeParams && cycles > cyclesLate) {
			break;
		}
		CircleBufferRead8(&gx->pipe, (int8_t*) &entry.command);
		CircleBufferRead8(&gx->pipe, (int8_t*) &entry.params[0]);
		CircleBufferRead8(&gx->pipe, (int8_t*) &entry.params[1]);
		CircleBufferRead8(&gx->pipe, (int8_t*) &entry.params[2]);
		CircleBufferRead8(&gx->pipe, (int8_t*) &entry.params[3]);

		if (gx->activeParams) {
			int index = _gxCommandParams[entry.command] - gx->activeParams;
			gx->activeEntries[index] = entry;
			--gx->activeParams;
		} else {
			gx->activeParams = _gxCommandParams[entry.command];
			if (gx->activeParams) {
				--gx->activeParams;
			}
			if (gx->activeParams) {
				gx->activeEntries[0] = entry;
			}
		}

		if (gx->activeParams) {
			continue;
		}

		switch (entry.command) {
		case DS_GX_CMD_MTX_MODE:
			if (entry.params[0] < 4) {
				gx->mtxMode = entry.params[0];
			} else {
				mLOG(DS_GX, GAME_ERROR, "Invalid GX MTX_MODE %02X", entry.params[0]);
			}
			break;
		case DS_GX_CMD_MTX_PUSH:
			switch (gx->mtxMode) {
			case 0:
				memcpy(&gx->projMatrixStack, &gx->projMatrix, sizeof(gx->projMatrix));
				++projMatrixPointer;
				break;
			case 2:
				memcpy(&gx->vecMatrixStack[gx->pvMatrixPointer & 0x1F], &gx->vecMatrix, sizeof(gx->vecMatrix));
				// Fall through
			case 1:
				memcpy(&gx->posMatrixStack[gx->pvMatrixPointer & 0x1F], &gx->posMatrix, sizeof(gx->posMatrix));
				++gx->pvMatrixPointer;
				break;
			case 3:
				mLOG(DS_GX, STUB, "Unimplemented GX MTX_PUSH mode");
				break;
			}
			break;
		case DS_GX_CMD_MTX_POP: {
			int8_t offset = entry.params[0];
			offset <<= 2;
			offset >>= 2;
			switch (gx->mtxMode) {
			case 0:
				projMatrixPointer -= offset;
				memcpy(&gx->projMatrix, &gx->projMatrixStack, sizeof(gx->projMatrix));
				break;
			case 1:
				gx->pvMatrixPointer -= offset;
				memcpy(&gx->posMatrix, &gx->posMatrixStack[gx->pvMatrixPointer & 0x1F], sizeof(gx->posMatrix));
				break;
			case 2:
				gx->pvMatrixPointer -= offset;
				memcpy(&gx->vecMatrix, &gx->vecMatrixStack[gx->pvMatrixPointer & 0x1F], sizeof(gx->vecMatrix));
				memcpy(&gx->posMatrix, &gx->posMatrixStack[gx->pvMatrixPointer & 0x1F], sizeof(gx->posMatrix));
				break;
			case 3:
				mLOG(DS_GX, STUB, "Unimplemented GX MTX_POP mode");
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_MTX_STORE: {
			int8_t offset = entry.params[0] & 0x1F;
			// TODO: overflow
			switch (gx->mtxMode) {
			case 0:
				memcpy(&gx->projMatrixStack, &gx->projMatrix, sizeof(gx->projMatrixStack));
				break;
			case 2:
				memcpy(&gx->vecMatrixStack[offset], &gx->vecMatrix, sizeof(gx->vecMatrix));
				// Fall through
			case 1:
				memcpy(&gx->posMatrixStack[offset], &gx->posMatrix, sizeof(gx->posMatrix));
				break;
			case 3:
				mLOG(DS_GX, STUB, "Unimplemented GX MTX_STORE mode");
				break;
			}
			break;
		}
		case DS_GX_CMD_MTX_RESTORE: {
			int8_t offset = entry.params[0] & 0x1F;
			// TODO: overflow
			switch (gx->mtxMode) {
			case 0:
				memcpy(&gx->projMatrix, &gx->projMatrixStack, sizeof(gx->projMatrix));
				break;
			case 2:
				memcpy(&gx->vecMatrix, &gx->vecMatrixStack[offset], sizeof(gx->vecMatrix));
				// Fall through
			case 1:
				memcpy(&gx->posMatrix, &gx->posMatrixStack[offset], sizeof(gx->posMatrix));
				break;
			case 3:
				mLOG(DS_GX, STUB, "Unimplemented GX MTX_RESTORE mode");
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_MTX_IDENTITY:
			switch (gx->mtxMode) {
			case 0:
				DSGXMtxIdentity(&gx->projMatrix);
				break;
			case 2:
				DSGXMtxIdentity(&gx->vecMatrix);
				// Fall through
			case 1:
				DSGXMtxIdentity(&gx->posMatrix);
				break;
			case 3:
				DSGXMtxIdentity(&gx->texMatrix);
				break;
			}
			_updateClipMatrix(gx);
			break;
		case DS_GX_CMD_MTX_LOAD_4x4: {
			struct DSGXMatrix m;
			int i;
			for (i = 0; i < 16; ++i) {
				m.m[i] = gx->activeEntries[i].params[0];
				m.m[i] |= gx->activeEntries[i].params[1] << 8;
				m.m[i] |= gx->activeEntries[i].params[2] << 16;
				m.m[i] |= gx->activeEntries[i].params[3] << 24;
			}
			switch (gx->mtxMode) {
			case 0:
				memcpy(&gx->projMatrix, &m, sizeof(gx->projMatrix));
				break;
			case 2:
				memcpy(&gx->vecMatrix, &m, sizeof(gx->vecMatrix));
				// Fall through
			case 1:
				memcpy(&gx->posMatrix, &m, sizeof(gx->posMatrix));
				break;
			case 3:
				memcpy(&gx->texMatrix, &m, sizeof(gx->texMatrix));
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_MTX_LOAD_4x3: {
			struct DSGXMatrix m;
			int i, j;
			for (j = 0; j < 4; ++j) {
				for (i = 0; i < 3; ++i) {
					m.m[i + j * 4] = gx->activeEntries[i + j * 3].params[0];
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[1] << 8;
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[2] << 16;
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[3] << 24;
				}
				m.m[j * 4 + 3] = 0;
			}
			m.m[15] = MTX_ONE;
			switch (gx->mtxMode) {
			case 0:
				memcpy(&gx->projMatrix, &m, sizeof(gx->projMatrix));
				break;
			case 2:
				memcpy(&gx->vecMatrix, &m, sizeof(gx->vecMatrix));
				// Fall through
			case 1:
				memcpy(&gx->posMatrix, &m, sizeof(gx->posMatrix));
				break;
			case 3:
				memcpy(&gx->texMatrix, &m, sizeof(gx->texMatrix));
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_MTX_MULT_4x4: {
			struct DSGXMatrix m;
			int i;
			for (i = 0; i < 16; ++i) {
				m.m[i] = gx->activeEntries[i].params[0];
				m.m[i] |= gx->activeEntries[i].params[1] << 8;
				m.m[i] |= gx->activeEntries[i].params[2] << 16;
				m.m[i] |= gx->activeEntries[i].params[3] << 24;
			}
			switch (gx->mtxMode) {
			case 0:
				DSGXMtxMultiply(&gx->projMatrix, &m, &gx->projMatrix);
				break;
			case 2:
				DSGXMtxMultiply(&gx->vecMatrix, &m, &gx->vecMatrix);
				// Fall through
			case 1:
				DSGXMtxMultiply(&gx->posMatrix, &m, &gx->posMatrix);
				break;
			case 3:
				DSGXMtxMultiply(&gx->texMatrix, &m, &gx->texMatrix);
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_MTX_MULT_4x3: {
			struct DSGXMatrix m;
			int i, j;
			for (j = 0; j < 4; ++j) {
				for (i = 0; i < 3; ++i) {
					m.m[i + j * 4] = gx->activeEntries[i + j * 3].params[0];
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[1] << 8;
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[2] << 16;
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[3] << 24;
				}
				m.m[j * 4 + 3] = 0;
			}
			m.m[15] = MTX_ONE;
			switch (gx->mtxMode) {
			case 0:
				DSGXMtxMultiply(&gx->projMatrix, &m, &gx->projMatrix);
				break;
			case 2:
				DSGXMtxMultiply(&gx->vecMatrix, &m, &gx->vecMatrix);
				// Fall through
			case 1:
				DSGXMtxMultiply(&gx->posMatrix, &m, &gx->posMatrix);
				break;
			case 3:
				DSGXMtxMultiply(&gx->texMatrix, &m, &gx->texMatrix);
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_MTX_MULT_3x3: {
			struct DSGXMatrix m;
			int i, j;
			for (j = 0; j < 3; ++j) {
				for (i = 0; i < 3; ++i) {
					m.m[i + j * 4] = gx->activeEntries[i + j * 3].params[0];
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[1] << 8;
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[2] << 16;
					m.m[i + j * 4] |= gx->activeEntries[i + j * 3].params[3] << 24;
				}
				m.m[j * 4 + 3] = 0;
			}
			m.m[12] = 0;
			m.m[13] = 0;
			m.m[14] = 0;
			m.m[15] = MTX_ONE;
			switch (gx->mtxMode) {
			case 0:
				DSGXMtxMultiply(&gx->projMatrix, &m, &gx->projMatrix);
				break;
			case 2:
				DSGXMtxMultiply(&gx->vecMatrix, &m, &gx->vecMatrix);
				// Fall through
			case 1:
				DSGXMtxMultiply(&gx->posMatrix, &m, &gx->posMatrix);
				break;
			case 3:
				DSGXMtxMultiply(&gx->texMatrix, &m, &gx->texMatrix);
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_MTX_TRANS: {
			int32_t m[3];
			m[0] = gx->activeEntries[0].params[0];
			m[0] |= gx->activeEntries[0].params[1] << 8;
			m[0] |= gx->activeEntries[0].params[2] << 16;
			m[0] |= gx->activeEntries[0].params[3] << 24;
			m[1] = gx->activeEntries[1].params[0];
			m[1] |= gx->activeEntries[1].params[1] << 8;
			m[1] |= gx->activeEntries[1].params[2] << 16;
			m[1] |= gx->activeEntries[1].params[3] << 24;
			m[2] = gx->activeEntries[2].params[0];
			m[2] |= gx->activeEntries[2].params[1] << 8;
			m[2] |= gx->activeEntries[2].params[2] << 16;
			m[2] |= gx->activeEntries[2].params[3] << 24;
			switch (gx->mtxMode) {
			case 0:
				DSGXMtxTranslate(&gx->projMatrix, m);
				break;
			case 2:
				DSGXMtxTranslate(&gx->vecMatrix, m);
				// Fall through
			case 1:
				DSGXMtxTranslate(&gx->posMatrix, m);
				break;
			case 3:
				DSGXMtxTranslate(&gx->texMatrix, m);
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_MTX_SCALE: {
			int32_t m[3];
			m[0] = gx->activeEntries[0].params[0];
			m[0] |= gx->activeEntries[0].params[1] << 8;
			m[0] |= gx->activeEntries[0].params[2] << 16;
			m[0] |= gx->activeEntries[0].params[3] << 24;
			m[1] = gx->activeEntries[1].params[0];
			m[1] |= gx->activeEntries[1].params[1] << 8;
			m[1] |= gx->activeEntries[1].params[2] << 16;
			m[1] |= gx->activeEntries[1].params[3] << 24;
			m[2] = gx->activeEntries[2].params[0];
			m[2] |= gx->activeEntries[2].params[1] << 8;
			m[2] |= gx->activeEntries[2].params[2] << 16;
			m[2] |= gx->activeEntries[2].params[3] << 24;
			switch (gx->mtxMode) {
			case 0:
				DSGXMtxScale(&gx->projMatrix, m);
				break;
			case 2:
				DSGXMtxScale(&gx->vecMatrix, m);
				// Fall through
			case 1:
				DSGXMtxScale(&gx->posMatrix, m);
				break;
			case 3:
				DSGXMtxScale(&gx->texMatrix, m);
				break;
			}
			_updateClipMatrix(gx);
			break;
		}
		case DS_GX_CMD_COLOR:
			gx->currentVertex.color = entry.params[0];
			gx->currentVertex.color |= entry.params[1] << 8;
			break;
		case DS_GX_CMD_TEXCOORD:
			gx->currentVertex.s = entry.params[0];
			gx->currentVertex.s |= entry.params[1] << 8;
			gx->currentVertex.t = entry.params[2];
			gx->currentVertex.t |= entry.params[3] << 8;
			break;
		case DS_GX_CMD_VTX_16: {
			int16_t x = gx->activeEntries[0].params[0];
			x |= gx->activeEntries[0].params[1] << 8;
			int16_t y = gx->activeEntries[0].params[2];
			y |= gx->activeEntries[0].params[3] << 8;
			int16_t z = gx->activeEntries[1].params[0];
			z |= gx->activeEntries[1].params[1] << 8;
			_emitVertex(gx, x, y, z);
			break;
		}
		case DS_GX_CMD_VTX_10: {
			int32_t xyz = entry.params[0];
			xyz |= entry.params[1] << 8;
			xyz |= entry.params[2] << 16;
			xyz |= entry.params[3] << 24;
			int16_t x = (xyz << 6) & 0xFFC0;
			int16_t y = (xyz >> 4) & 0xFFC0;
			int16_t z = (xyz >> 14) & 0xFFC0;
			_emitVertex(gx, x, y, z);
			break;
		}
		case DS_GX_CMD_VTX_XY: {
			int16_t x = entry.params[0];
			x |= entry.params[1] << 8;
			int16_t y = entry.params[2];
			y |= entry.params[3] << 8;
			_emitVertex(gx, x, y, gx->currentVertex.z);
			break;
		}
		case DS_GX_CMD_VTX_XZ: {
			int16_t x = entry.params[0];
			x |= entry.params[1] << 8;
			int16_t z = entry.params[2];
			z |= entry.params[3] << 8;
			_emitVertex(gx, x, gx->currentVertex.y, z);
			break;
		}
		case DS_GX_CMD_VTX_YZ: {
			int16_t y = entry.params[0];
			y |= entry.params[1] << 8;
			int16_t z = entry.params[2];
			z |= entry.params[3] << 8;
			_emitVertex(gx, gx->currentVertex.x, y, z);
			break;
		}
		case DS_GX_CMD_VTX_DIFF: {
			int32_t xyz = entry.params[0];
			xyz |= entry.params[1] << 8;
			xyz |= entry.params[2] << 16;
			xyz |= entry.params[3] << 24;
			int16_t x = (xyz << 6) & 0xFFC0;
			int16_t y = (xyz >> 4) & 0xFFC0;
			int16_t z = (xyz >> 14) & 0xFFC0;
			_emitVertex(gx, gx->currentVertex.x + (x >> 6), gx->currentVertex.y + (y >> 6), gx->currentVertex.z + (z >> 6));
		}
		case DS_GX_CMD_POLYGON_ATTR:
			gx->nextPoly.polyParams = entry.params[0];
			gx->nextPoly.polyParams |= entry.params[1] << 8;
			gx->nextPoly.polyParams |= entry.params[2] << 16;
			gx->nextPoly.polyParams |= entry.params[3] << 24;
			break;
		case DS_GX_CMD_TEXIMAGE_PARAM:
			gx->nextPoly.texParams = entry.params[0];
			gx->nextPoly.texParams |= entry.params[1] << 8;
			gx->nextPoly.texParams |= entry.params[2] << 16;
			gx->nextPoly.texParams |= entry.params[3] << 24;
			break;
		case DS_GX_CMD_PLTT_BASE:
			gx->nextPoly.palBase = entry.params[0];
			gx->nextPoly.palBase |= entry.params[1] << 8;
			gx->nextPoly.palBase |= entry.params[2] << 16;
			gx->nextPoly.palBase |= entry.params[3] << 24;
			break;
		case DS_GX_CMD_BEGIN_VTXS:
			gx->vertexMode = entry.params[0] & 3;
			gx->currentPoly = gx->nextPoly;
			break;
		case DS_GX_CMD_END_VTXS:
			gx->vertexMode = -1;
			break;
		case DS_GX_CMD_SWAP_BUFFERS:
			gx->swapBuffers = true;
			break;
		case DS_GX_CMD_VIEWPORT:
			gx->viewportX1 = (uint8_t) entry.params[0];
			gx->viewportY1 = (uint8_t) entry.params[1];
			gx->viewportX2 = (uint8_t) entry.params[2];
			gx->viewportY2 = (uint8_t) entry.params[3];
			gx->viewportWidth = gx->viewportX2 - gx->viewportX1;
			gx->viewportHeight = gx->viewportY2 - gx->viewportY1;
			break;
		default:
			mLOG(DS_GX, STUB, "Unimplemented GX command %02X:%02X %02X %02X %02X", entry.command, entry.params[0], entry.params[1], entry.params[2], entry.params[3]);
			break;
		}

		gxstat = DSRegGXSTATSetPVMatrixStackLevel(gxstat, gx->pvMatrixPointer);
		gxstat = DSRegGXSTATSetProjMatrixStackLevel(gxstat, projMatrixPointer);
		gxstat = DSRegGXSTATTestFillMatrixStackError(gxstat, projMatrixPointer || gx->pvMatrixPointer >= 0x1F);
		gx->p->memory.io9[DS9_REG_GXSTAT_LO >> 1] = gxstat;

		if (cyclesLate >= cycles) {
			cyclesLate -= cycles;
		} else {
			break;
		}
	}
	if (cycles && !gx->swapBuffers) {
		mTimingSchedule(timing, &gx->fifoEvent, cycles - cyclesLate);
	}
	_flushOutstanding(gx);
	DSGXUpdateGXSTAT(gx);
}

void DSGXInit(struct DSGX* gx) {
	gx->renderer = &dummyRenderer;
	CircleBufferInit(&gx->fifo, sizeof(struct DSGXEntry) * DS_GX_FIFO_SIZE);
	CircleBufferInit(&gx->pipe, sizeof(struct DSGXEntry) * DS_GX_PIPE_SIZE);
	gx->vertexBuffer[0] = malloc(sizeof(struct DSGXVertex) * DS_GX_VERTEX_BUFFER_SIZE);
	gx->vertexBuffer[1] = malloc(sizeof(struct DSGXVertex) * DS_GX_VERTEX_BUFFER_SIZE);
	gx->polygonBuffer[0] = malloc(sizeof(struct DSGXPolygon) * DS_GX_POLYGON_BUFFER_SIZE);
	gx->polygonBuffer[1] = malloc(sizeof(struct DSGXPolygon) * DS_GX_POLYGON_BUFFER_SIZE);
	gx->fifoEvent.name = "DS GX FIFO";
	gx->fifoEvent.priority = 0xC;
	gx->fifoEvent.context = gx;
	gx->fifoEvent.callback = _fifoRun;
}

void DSGXDeinit(struct DSGX* gx) {
	DSGXAssociateRenderer(gx, &dummyRenderer);
	CircleBufferDeinit(&gx->fifo);
	CircleBufferDeinit(&gx->pipe);
	free(gx->vertexBuffer[0]);
	free(gx->vertexBuffer[1]);
	free(gx->polygonBuffer[0]);
	free(gx->polygonBuffer[1]);
}

void DSGXReset(struct DSGX* gx) {
	CircleBufferClear(&gx->fifo);
	CircleBufferClear(&gx->pipe);
	DSGXMtxIdentity(&gx->projMatrix);
	DSGXMtxIdentity(&gx->texMatrix);
	DSGXMtxIdentity(&gx->posMatrix);
	DSGXMtxIdentity(&gx->vecMatrix);

	DSGXMtxIdentity(&gx->clipMatrix);
	DSGXMtxIdentity(&gx->projMatrixStack);
	DSGXMtxIdentity(&gx->texMatrixStack);
	int i;
	for (i = 0; i < 32; ++i) {
		DSGXMtxIdentity(&gx->posMatrixStack[i]);
		DSGXMtxIdentity(&gx->vecMatrixStack[i]);
	}
	gx->swapBuffers = false;
	gx->bufferIndex = 0;
	gx->vertexIndex = 0;
	gx->polygonIndex = 0;
	gx->mtxMode = 0;
	gx->pvMatrixPointer = 0;
	gx->vertexMode = -1;

	gx->viewportX1 = 0;
	gx->viewportY1 = 0;
	gx->viewportX2 = DS_VIDEO_HORIZONTAL_PIXELS - 1;
	gx->viewportY2 = DS_VIDEO_VERTICAL_PIXELS - 1;
	gx->viewportWidth = gx->viewportX2 - gx->viewportX1;
	gx->viewportHeight = gx->viewportY2 - gx->viewportY1;

	memset(gx->outstandingParams, 0, sizeof(gx->outstandingParams));
	memset(gx->outstandingCommand, 0, sizeof(gx->outstandingCommand));
	memset(&gx->outstandingEntry, 0, sizeof(gx->outstandingEntry));
	gx->activeParams = 0;
	memset(&gx->currentVertex, 0, sizeof(gx->currentVertex));
	memset(&gx->nextPoly, 0, sizeof(gx-> nextPoly));
}

void DSGXAssociateRenderer(struct DSGX* gx, struct DSGXRenderer* renderer) {
	gx->renderer->deinit(gx->renderer);
	gx->renderer = renderer;
	memcpy(gx->renderer->tex, gx->tex, sizeof(gx->renderer->tex));
	memcpy(gx->renderer->texPal, gx->texPal, sizeof(gx->renderer->texPal));
	gx->renderer->init(gx->renderer);
}

void DSGXUpdateGXSTAT(struct DSGX* gx) {
	uint32_t value = gx->p->memory.io9[DS9_REG_GXSTAT_HI >> 1] << 16;
	value = DSRegGXSTATIsDoIRQ(value);

	size_t entries = CircleBufferSize(&gx->fifo) / sizeof(struct DSGXEntry);
	// XXX
	if (gx->swapBuffers) {
		entries++;
	}
	value = DSRegGXSTATSetFIFOEntries(value, entries);
	value = DSRegGXSTATSetFIFOLtHalf(value, entries < (DS_GX_FIFO_SIZE / 2));
	value = DSRegGXSTATSetFIFOEmpty(value, entries == 0);

	if ((DSRegGXSTATGetDoIRQ(value) == 1 && entries < (DS_GX_FIFO_SIZE / 2)) ||
		(DSRegGXSTATGetDoIRQ(value) == 2 && entries == 0)) {
		DSRaiseIRQ(gx->p->ds9.cpu, gx->p->ds9.memory.io, DS_IRQ_GEOM_FIFO);
	}

	value = DSRegGXSTATSetBusy(value, mTimingIsScheduled(&gx->p->ds9.timing, &gx->fifoEvent) || gx->swapBuffers);

	gx->p->memory.io9[DS9_REG_GXSTAT_HI >> 1] = value >> 16;
}

static void DSGXUnpackCommand(struct DSGX* gx, uint32_t command) {
	gx->outstandingCommand[0] = command;
	gx->outstandingCommand[1] = command >> 8;
	gx->outstandingCommand[2] = command >> 16;
	gx->outstandingCommand[3] = command >> 24;
	if (gx->outstandingCommand[0] >= DS_GX_CMD_MAX) {
		gx->outstandingCommand[0] = 0;
	}
	if (gx->outstandingCommand[1] >= DS_GX_CMD_MAX) {
		gx->outstandingCommand[1] = 0;
	}
	if (gx->outstandingCommand[2] >= DS_GX_CMD_MAX) {
		gx->outstandingCommand[2] = 0;
	}
	if (gx->outstandingCommand[3] >= DS_GX_CMD_MAX) {
		gx->outstandingCommand[3] = 0;
	}
	gx->outstandingParams[0] = _gxCommandParams[gx->outstandingCommand[0]];
	gx->outstandingParams[1] = _gxCommandParams[gx->outstandingCommand[1]];
	gx->outstandingParams[2] = _gxCommandParams[gx->outstandingCommand[2]];
	gx->outstandingParams[3] = _gxCommandParams[gx->outstandingCommand[3]];
	_flushOutstanding(gx);
	DSGXUpdateGXSTAT(gx);
}

static void DSGXWriteFIFO(struct DSGX* gx, struct DSGXEntry entry) {
	if (CircleBufferSize(&gx->fifo) == (DS_GX_FIFO_SIZE * sizeof(entry))) {
		mLOG(DS_GX, INFO, "FIFO full");
		if (gx->p->cpuBlocked & DS_CPU_BLOCK_GX) {
			abort();
		}
		gx->p->cpuBlocked |= DS_CPU_BLOCK_GX;
		gx->outstandingEntry = entry;
		gx->p->ds9.cpu->nextEvent = 0;
		return;
	}
	if (gx->outstandingCommand[0]) {
		entry.command = gx->outstandingCommand[0];
		if (gx->outstandingParams[0]) {
			--gx->outstandingParams[0];
		}
		if (!gx->outstandingParams[0]) {
			// TODO: improve this
			memmove(&gx->outstandingParams[0], &gx->outstandingParams[1], sizeof(gx->outstandingParams[0]) * 3);
			memmove(&gx->outstandingCommand[0], &gx->outstandingCommand[1], sizeof(gx->outstandingCommand[0]) * 3);
			gx->outstandingParams[3] = 0;
			gx->outstandingCommand[3] = 0;
		}
	} else {
		gx->outstandingParams[0] = _gxCommandParams[entry.command];
		if (gx->outstandingParams[0]) {
			--gx->outstandingParams[0];
		}
		if (gx->outstandingParams[0]) {
			gx->outstandingCommand[0] = entry.command;
		}
	}
	uint32_t cycles = _gxCommandCycleBase[entry.command];
	if (!cycles) {
		return;
	}
	if (CircleBufferSize(&gx->fifo) == 0 && CircleBufferSize(&gx->pipe) < (DS_GX_PIPE_SIZE * sizeof(entry))) {
		CircleBufferWrite8(&gx->pipe, entry.command);
		CircleBufferWrite8(&gx->pipe, entry.params[0]);
		CircleBufferWrite8(&gx->pipe, entry.params[1]);
		CircleBufferWrite8(&gx->pipe, entry.params[2]);
		CircleBufferWrite8(&gx->pipe, entry.params[3]);
	} else if (CircleBufferSize(&gx->fifo) < (DS_GX_FIFO_SIZE * sizeof(entry))) {
		CircleBufferWrite8(&gx->fifo, entry.command);
		CircleBufferWrite8(&gx->fifo, entry.params[0]);
		CircleBufferWrite8(&gx->fifo, entry.params[1]);
		CircleBufferWrite8(&gx->fifo, entry.params[2]);
		CircleBufferWrite8(&gx->fifo, entry.params[3]);
	}
	if (!gx->swapBuffers && !mTimingIsScheduled(&gx->p->ds9.timing, &gx->fifoEvent)) {
		mTimingSchedule(&gx->p->ds9.timing, &gx->fifoEvent, cycles);
	}

	_flushOutstanding(gx);
}

uint16_t DSGXWriteRegister(struct DSGX* gx, uint32_t address, uint16_t value) {
	uint16_t oldValue = gx->p->memory.io9[address >> 1];
	switch (address) {
	case DS9_REG_DISP3DCNT:
		mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		break;
	case DS9_REG_GXSTAT_LO:
		value = DSRegGXSTATIsMatrixStackError(value);
		if (value) {
			oldValue = DSRegGXSTATClearMatrixStackError(oldValue);
			oldValue = DSRegGXSTATClearProjMatrixStackLevel(oldValue);
		}
		value = oldValue;
		break;
	case DS9_REG_GXSTAT_HI:
		value = DSRegGXSTATIsDoIRQ(value << 16) >> 16;
		gx->p->memory.io9[address >> 1] = value;
		DSGXUpdateGXSTAT(gx);
		value = gx->p->memory.io9[address >> 1];
		break;
	default:
		if (address < DS9_REG_GXFIFO_00) {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		} else if (address <= DS9_REG_GXFIFO_1F) {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		} else if (address < DS9_REG_GXSTAT_LO) {
			struct DSGXEntry entry = {
				.command = (address & 0x1FC) >> 2,
				.params = {
					value,
					value >> 8,
				}
			};
			if (entry.command < DS_GX_CMD_MAX) {
				DSGXWriteFIFO(gx, entry);
			}
		} else {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%04X", address, value);
		}
		break;
	}
	return value;
}

uint32_t DSGXWriteRegister32(struct DSGX* gx, uint32_t address, uint32_t value) {
	switch (address) {
	case DS9_REG_DISP3DCNT:
		mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%08X", address, value);
		break;
	case DS9_REG_GXSTAT_LO:
		value = (value & 0xFFFF0000) | DSGXWriteRegister(gx, DS9_REG_GXSTAT_LO, value);
		value = (value & 0x0000FFFF) | (DSGXWriteRegister(gx, DS9_REG_GXSTAT_HI, value >> 16) << 16);
		break;
	default:
		if (address < DS9_REG_GXFIFO_00) {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%08X", address, value);
		} else if (address <= DS9_REG_GXFIFO_1F) {
			if (gx->outstandingParams[0]) {
				struct DSGXEntry entry = {
					.command = gx->outstandingCommand[0],
					.params = {
						value,
						value >> 8,
						value >> 16,
						value >> 24
					}
				};
				DSGXWriteFIFO(gx, entry);
			} else {
				DSGXUnpackCommand(gx, value);
			}
		} else if (address < DS9_REG_GXSTAT_LO) {
			struct DSGXEntry entry = {
				.command = (address & 0x1FC) >> 2,
				.params = {
					value,
					value >> 8,
					value >> 16,
					value >> 24
				}
			};
			DSGXWriteFIFO(gx, entry);
		} else {
			mLOG(DS_GX, STUB, "Unimplemented GX write %03X:%08X", address, value);
		}
		break;
	}
	return value;
}

void DSGXFlush(struct DSGX* gx) {
	if (gx->swapBuffers) {
		gx->renderer->setRAM(gx->renderer, gx->vertexBuffer[gx->bufferIndex], gx->polygonBuffer[gx->bufferIndex], gx->polygonIndex);
		gx->swapBuffers = false;
		gx->bufferIndex ^= 1;
		gx->vertexIndex = 0;
		gx->polygonIndex = 0;
	}

	if (CircleBufferSize(&gx->fifo)) {
		mTimingSchedule(&gx->p->ds9.timing, &gx->fifoEvent, 0);
	}
	DSGXUpdateGXSTAT(gx);
}

static void DSGXDummyRendererInit(struct DSGXRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void DSGXDummyRendererReset(struct DSGXRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void DSGXDummyRendererDeinit(struct DSGXRenderer* renderer) {
	UNUSED(renderer);
	// Nothing to do
}

static void DSGXDummyRendererInvalidateTex(struct DSGXRenderer* renderer, int slot) {
	UNUSED(renderer);
	UNUSED(slot);
	// Nothing to do
}

static void DSGXDummyRendererSetRAM(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXPolygon* polys, unsigned polyCount) {
	UNUSED(renderer);
	UNUSED(verts);
	UNUSED(polys);
	UNUSED(polyCount);
	// Nothing to do
}

static void DSGXDummyRendererDrawScanline(struct DSGXRenderer* renderer, int y) {
	UNUSED(renderer);
	UNUSED(y);
	// Nothing to do
}

static void DSGXDummyRendererGetScanline(struct DSGXRenderer* renderer, int y, color_t** output) {
	UNUSED(renderer);
	UNUSED(y);
	*output = NULL;
	// Nothing to do
}
