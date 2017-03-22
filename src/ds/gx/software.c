/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/gx/software.h>

#include <mgba-util/memory.h>
#include "gba/renderers/software-private.h"

DEFINE_VECTOR(DSGXSoftwarePolygonList, struct DSGXSoftwarePolygon);
DEFINE_VECTOR(DSGXSoftwareEdgeList, struct DSGXSoftwareEdge);
DEFINE_VECTOR(DSGXSoftwareSpanList, struct DSGXSoftwareSpan);

static void DSGXSoftwareRendererInit(struct DSGXRenderer* renderer);
static void DSGXSoftwareRendererReset(struct DSGXRenderer* renderer);
static void DSGXSoftwareRendererDeinit(struct DSGXRenderer* renderer);
static void DSGXSoftwareRendererInvalidateTex(struct DSGXRenderer* renderer, int slot);
static void DSGXSoftwareRendererSetRAM(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXPolygon* polys, unsigned polyCount, bool wSort);
static void DSGXSoftwareRendererDrawScanline(struct DSGXRenderer* renderer, int y);
static void DSGXSoftwareRendererGetScanline(struct DSGXRenderer* renderer, int y, const color_t** output);

static void _expandColor(uint16_t c15, uint8_t* r, uint8_t* g, uint8_t* b) {
	*r = ((c15 << 1) & 0x3E) | 1;
	*g = ((c15 >> 4) & 0x3E) | 1;
	*b = ((c15 >> 9) & 0x3E) | 1;
}

static color_t _finishColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
#ifndef COLOR_16_BIT
	color_t rgba = (r << 2) & 0xFC;
	rgba |= (g << 10) & 0xFC00;
	rgba |= (b << 18) & 0xFC0000;
	rgba |= (a << 26) & 0xF8000000;
	return rgba;
#else
#error Unsupported color depth
#endif
}

static unsigned _mix32(int weightA, unsigned colorA, int weightB, unsigned colorB) {
	unsigned c = 0;
	unsigned a, b;
#ifdef COLOR_16_BIT
#error Unsupported color depth
#else
	a = colorA & 0xFF;
	b = colorB & 0xFF;
	c |= ((a * weightA + b * weightB) / 32) & 0x1FF;
	if (c & 0x00000100) {
		c = 0x000000FF;
	}

	a = colorA & 0xFF00;
	b = colorB & 0xFF00;
	c |= ((a * weightA + b * weightB) / 32) & 0x1FF00;
	if (c & 0x00010000) {
		c = (c & 0x000000FF) | 0x0000FF00;
	}

	a = colorA & 0xFF0000;
	b = colorB & 0xFF0000;
	c |= ((a * weightA + b * weightB) / 32) & 0x1FF0000;
	if (c & 0x01000000) {
		c = (c & 0x0000FFFF) | 0x00FF0000;
	}
#endif
	return c;
}

static unsigned _mixTexels(int weightA, unsigned colorA, int weightB, unsigned colorB) {
	unsigned c = 0;
	unsigned a, b;
	a = colorA & 0x7C1F;
	b = colorB & 0x7C1F;
	a |= (colorA & 0x3E0) << 16;
	b |= (colorB & 0x3E0) << 16;
	c = ((a * weightA + b * weightB) / 8);
	if (c & 0x04000000) {
		c = (c & ~0x07E00000) | 0x03E00000;
	}
	if (c & 0x0020) {
		c = (c & ~0x003F) | 0x001F;
	}
	if (c & 0x8000) {
		c = (c & ~0xF800) | 0x7C00;
	}
	c = (c & 0x7C1F) | ((c >> 16) & 0x03E0);
	return c;
}

static color_t _lookupColor(struct DSGXSoftwareRenderer* renderer, struct DSGXSoftwareEndpoint* ep, struct DSGXSoftwarePolygon* poly) {
	if (!poly->texBase && poly->texFormat) {
		return 0;
	}
	// TODO: Optimize
	uint16_t texel;

	int16_t s = ep->s >> 4;
	int16_t t = ep->t >> 4;
	if (!DSGXTexParamsIsSRepeat(poly->poly->texParams)) {
		if (s < 0) {
			s = 0;
		} else if (s >= poly->texW) {
			s = poly->texW - 1;
		}
	} else if (DSGXTexParamsIsSMirror(poly->poly->texParams)) {
		if (s & poly->texW) {
			s = poly->texW - s - 1;
		}
		s &= poly->texW - 1;
	} else {
		s &= poly->texW - 1;
	}
	if (!DSGXTexParamsIsTRepeat(poly->poly->texParams)) {
		if (t < 0) {
			t = 0;
		} else if (t >= poly->texH) {
			t = poly->texW - 1;
		}
	} else if (DSGXTexParamsIsTMirror(poly->poly->texParams)) {
		if (t & poly->texH) {
			t = poly->texH - t - 1;
		}
		t &= poly->texH - 1;
	} else {
		t &= poly->texH - 1;
	}

	uint16_t texelCoord = s + t * poly->texW;
	uint8_t ta = 0x1F;
	uint8_t pa = DSGXPolygonAttrsGetAlpha(poly->poly->polyParams);
	if (pa) {
		pa = (pa << 1) + 1;
	}
	switch (poly->texFormat) {
	case 0:
	default:
		return _finishColor(ep->cr, ep->cg, ep->cb, pa);
	case 1:
		texel = ((uint8_t*) poly->texBase)[texelCoord];
		ta = (texel >> 5) & 0x7;
		ta = (ta << 2) + (ta >> 1);
		texel &= 0x1F;
		break;
	case 2:
		texel = ((uint8_t*) poly->texBase)[texelCoord >> 2];
		if (texelCoord & 0x3) {
			texel >>= 2 * texel & 3;
		}
		texel &= 0x3;
		break;
	case 3:
		texel = ((uint8_t*) poly->texBase)[texelCoord >> 1];
		if (texelCoord & 0x1) {
			texel >>= 4;
		}
		texel &= 0xF;
		break;
	case 4:
		texel = ((uint8_t*) poly->texBase)[texelCoord];
		break;
	case 5:
		texelCoord = (s & ~3) + (t & 3) + (t >> 2) * poly->texW;
		texel = ((uint8_t*) poly->texBase)[texelCoord];
		texel >>= (s & 3) * 2;
		texel &= 3;
		break;
	case 6:
		texel = ((uint8_t*) poly->texBase)[texelCoord];
		ta = (texel >> 3) & 0x1F;
		texel &= 0x7;
		break;
	case 7:
		return _finishColor(0x3F, 0x3F, 0x3F, pa);
	}
	uint8_t r, g, b;
	unsigned wr, wg, wb, wa;
	if (poly->texFormat == 5) {
		if (!renderer->d.tex[1]) {
			return 0;
		}
		uint16_t half = DSGXTexParamsGetVRAMBase(poly->poly->texParams) & 0x8000;
		uint32_t slot1Base = (DSGXTexParamsGetVRAMBase(poly->poly->texParams) << 1) + (texelCoord >> 2) + half;
		uint16_t texel2 = renderer->d.tex[1][slot1Base];
		uint16_t texel2Base = (texel2 & 0x3FFF) << 1;
		int a = 0x8;
		int b = 0;
		switch (texel2 >> 14) {
		case 0:
			if (texel == 3) {
				ta = 0;
			}
			texel = poly->palBase[texel + texel2Base];
			break;
		case 1:
			if (texel == 3) {
				ta = 0;
			}
			if (texel != 2) {
				texel = poly->palBase[texel + texel2Base];
			} else {
				texel = poly->palBase[texel2Base];
				texel2 = poly->palBase[texel2Base + 1];
				a = 4;
				b = 4;
			}
			break;
		case 2:
			texel = poly->palBase[texel + texel2Base];
			break;
		case 3:
			switch (texel) {
			case 0:
			case 1:
				texel = poly->palBase[texel + texel2Base];
				break;
			case 2:
				texel = poly->palBase[texel2Base];
				texel2 = poly->palBase[texel2Base + 1];
				a = 5;
				b = 3;
				break;
			case 3:
				texel = poly->palBase[texel2Base];
				texel2 = poly->palBase[texel2Base + 1];
				a = 3;
				b = 5;
				break;
			}
			break;
		}
		if (b) {
			texel = _mixTexels(a, texel, b, texel2);
		}
	} else {
		if (poly->texFormat < 5 && poly->texFormat > 1 && DSGXTexParamsIs0Transparent(poly->poly->texParams) && !texel) {
			return 0;
		}
		texel = poly->palBase[texel];
	}
	_expandColor(texel, &r, &g, &b);
	if (ta) {
		ta = (ta << 1) + 1;
	}
	switch (poly->blendFormat) {
	default:
		return _finishColor(r, g, b, pa);
	case 0:
		wr = ((r + 1) * (ep->cr + 1) - 1) >> 6;
		wg = ((g + 1) * (ep->cg + 1) - 1) >> 6;
		wb = ((b + 1) * (ep->cb + 1) - 1) >> 6;
		wa = ((ta + 1) * (pa + 1) - 1) >> 6;
		return _finishColor(wr, wg, wb, wa);
	case 1:
		wr = (r * ta + ep->cr * (63 - ta)) >> 6;
		wg = (g * ta + ep->cg * (63 - ta)) >> 6;
		wb = (b * ta + ep->cb * (63 - ta)) >> 6;
		return _finishColor(wr, wg, wb, pa);
	case 3:
		return _finishColor(r, g, b, pa);
	}
}

static inline int32_t _interpolate(int32_t x0, int32_t x1, int64_t w0, int64_t w1, int64_t w, int32_t qr) {
	// 32-bit -> 96-bit
	int64_t x0b = (w0 & 0xFFFFFFFF) * x0;
	int64_t x0t = (w0 >> 32) * x0;
	int64_t x1b = (w1 & 0xFFFFFFFF) * x1;
	int64_t x1t = (w1 >> 32) * x1;
	// 96-bit -> 64-bit
	int64_t xx0 = (x0t << 32) + x0b;
	int64_t xx1 = (x1t << 32) + x1b;
	xx1 -= xx0;
	xx1 >>= 12;

	int64_t qrb = xx1 * qr;
	qrb += xx0;

	return qrb / w;
}

static inline int32_t _divideBy(int64_t x, int32_t recip) {
	int64_t x0 = (x & 0xFFFFFFFF) * recip;
	int64_t x1 = (x >> 32) * recip;
	x1 += x0 >> 32;
	return x1 >> 31;
}

static bool _edgeToSpan(struct DSGXSoftwareSpan* span, const struct DSGXSoftwareEdge* edge, int index, int32_t y) {
	int32_t height = edge->y1 - edge->y0;
	int64_t yw = y - edge->y0;
	if (!height) {
		return false;
	}
	// Clamp to bounds
	if (yw < 0) {
		return false;
	} else if (yw > height) {
		return false;
	}

	span->ep[index].coord[0] = (((int64_t) (edge->x1 - edge->x0) * yw) / height) + edge->x0;

	if (index) {
		if (span->ep[0].coord[0] == span->ep[index].coord[0]) {
			return false;
		}
		if (span->ep[0].coord[0] > span->ep[index].coord[0]) {
			int32_t temp = span->ep[index].coord[0];
			span->ep[index] = span->ep[0];
			span->ep[0].coord[0] = temp;
			index = 0;
		}
	}

	int64_t w0 = 0x7FFFFFFFFFFFFFFF / edge->w0;
	int64_t w1 = 0x7FFFFFFFFFFFFFFF / edge->w1;
	int64_t w = w1 - w0;

	// Losslessly interpolate two 64-bit values
	int64_t wb = (w & 0xFFFFFFFF) * yw;
	int64_t wt = (w >> 32) * yw;
	int64_t div = wt / height;
	int64_t rem = wt % height;
	w = div << 32;
	wb += rem << 32;
	div = wb / height;
	w += div;
	w += w0;

	span->ep[index].coord[3] = (0x7FFFFFFFFFFFFFFF / w) + 1;
	span->ep[index].wRecip = w;
	int32_t qr = (yw << 12) / height;

	span->ep[index].coord[2]  = _interpolate(edge->z0, edge->z1, w0, w1, w, qr);
	span->ep[index].cr = _interpolate(edge->cr0, edge->cr1, w0, w1, w, qr);
	span->ep[index].cg = _interpolate(edge->cg0, edge->cg1, w0, w1, w, qr);
	span->ep[index].cb = _interpolate(edge->cb0, edge->cb1, w0, w1, w, qr);
	span->ep[index].s  = _interpolate(edge->s0, edge->s1, w0, w1, w, qr);
	span->ep[index].t  = _interpolate(edge->t0, edge->t1, w0, w1, w, qr);

	return true;
}

static void _createStep(struct DSGXSoftwareSpan* span) {
	int32_t width = (span->ep[1].coord[0] - span->ep[0].coord[0]) >> 7;

	span->ep[0].stepW = span->ep[0].wRecip;
	span->ep[0].stepZ = span->ep[0].coord[2] * span->ep[0].wRecip;
	span->ep[0].stepR = span->ep[0].cr * span->ep[0].wRecip;
	span->ep[0].stepG = span->ep[0].cg * span->ep[0].wRecip;
	span->ep[0].stepB = span->ep[0].cb * span->ep[0].wRecip;
	span->ep[0].stepS = span->ep[0].s  * span->ep[0].wRecip;
	span->ep[0].stepT = span->ep[0].t  * span->ep[0].wRecip;

	span->ep[1].stepW = span->ep[1].wRecip;
	span->ep[1].stepZ = span->ep[1].coord[2] * span->ep[1].wRecip;
	span->ep[1].stepR = span->ep[1].cr * span->ep[1].wRecip;
	span->ep[1].stepG = span->ep[1].cg * span->ep[1].wRecip;
	span->ep[1].stepB = span->ep[1].cb * span->ep[1].wRecip;
	span->ep[1].stepS = span->ep[1].s  * span->ep[1].wRecip;
	span->ep[1].stepT = span->ep[1].t  * span->ep[1].wRecip;

	if (!width) {
		return;
	}
	span->step.coord[0] = span->ep[1].coord[0] - span->ep[0].coord[0];
	span->step.stepW = (span->ep[1].stepW - span->ep[0].stepW) / width;
	span->step.stepZ = (span->ep[1].stepZ - span->ep[0].stepZ) / width;
	span->step.stepR = (span->ep[1].stepR - span->ep[0].stepR) / width;
	span->step.stepG = (span->ep[1].stepG - span->ep[0].stepG) / width;
	span->step.stepB = (span->ep[1].stepB - span->ep[0].stepB) / width;
	span->step.stepS = (span->ep[1].stepS - span->ep[0].stepS) / width;
	span->step.stepT = (span->ep[1].stepT - span->ep[0].stepT) / width;
}

static void _stepEndpoint(struct DSGXSoftwareSpan* span) {
	int i = 28;
	int32_t nextX = (span->ep[0].coord[0] & ~0xFFF) + 0x1000;
	i = (nextX - span->ep[0].coord[0]) >> 7;
	span->ep[0].coord[0] = nextX;

	span->ep[0].wRecip += span->step.stepW * i;
	span->ep[0].coord[3] = (0x7FFFFFFFFFFFFFFF / span->ep[0].wRecip) + 1;

	span->ep[0].stepZ += span->step.stepZ * i;
	span->ep[0].coord[2] = _divideBy(span->ep[0].stepZ, span->ep[0].coord[3]);

	span->ep[0].stepR += span->step.stepR * i;
	span->ep[0].stepG += span->step.stepG * i;
	span->ep[0].stepB += span->step.stepB * i;
	span->ep[0].stepS += span->step.stepS * i;
	span->ep[0].stepT += span->step.stepT * i;
}

static void _resolveEndpoint(struct DSGXSoftwareSpan* span) {
	span->ep[0].cr = _divideBy(span->ep[0].stepR, span->ep[0].coord[3]);
	span->ep[0].cg = _divideBy(span->ep[0].stepG, span->ep[0].coord[3]);
	span->ep[0].cb = _divideBy(span->ep[0].stepB, span->ep[0].coord[3]);
	span->ep[0].s = _divideBy(span->ep[0].stepS, span->ep[0].coord[3]);
	span->ep[0].t = _divideBy(span->ep[0].stepT, span->ep[0].coord[3]);
}

void DSGXSoftwareRendererCreate(struct DSGXSoftwareRenderer* renderer) {
	renderer->d.init = DSGXSoftwareRendererInit;
	renderer->d.reset = DSGXSoftwareRendererReset;
	renderer->d.deinit = DSGXSoftwareRendererDeinit;
	renderer->d.invalidateTex = DSGXSoftwareRendererInvalidateTex;
	renderer->d.setRAM = DSGXSoftwareRendererSetRAM;
	renderer->d.drawScanline = DSGXSoftwareRendererDrawScanline;
	renderer->d.getScanline = DSGXSoftwareRendererGetScanline;
}

static void DSGXSoftwareRendererInit(struct DSGXRenderer* renderer) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	DSGXSoftwarePolygonListInit(&softwareRenderer->activePolys, DS_GX_POLYGON_BUFFER_SIZE / 4);
	DSGXSoftwareEdgeListInit(&softwareRenderer->activeEdges, DS_GX_POLYGON_BUFFER_SIZE);
	DSGXSoftwareSpanListInit(&softwareRenderer->activeSpans, DS_GX_POLYGON_BUFFER_SIZE / 2);
	softwareRenderer->bucket = anonymousMemoryMap(sizeof(*softwareRenderer->bucket) * DS_GX_POLYGON_BUFFER_SIZE);
	softwareRenderer->scanlineCache = anonymousMemoryMap(sizeof(color_t) * DS_VIDEO_VERTICAL_PIXELS * DS_VIDEO_HORIZONTAL_PIXELS);
}

static void DSGXSoftwareRendererReset(struct DSGXRenderer* renderer) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	softwareRenderer->flushPending = false;
}

static void DSGXSoftwareRendererDeinit(struct DSGXRenderer* renderer) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	DSGXSoftwarePolygonListDeinit(&softwareRenderer->activePolys);
	DSGXSoftwareEdgeListDeinit(&softwareRenderer->activeEdges);	
	DSGXSoftwareSpanListDeinit(&softwareRenderer->activeSpans);
	mappedMemoryFree(softwareRenderer->bucket, sizeof(*softwareRenderer->bucket) * DS_GX_POLYGON_BUFFER_SIZE);
	mappedMemoryFree(softwareRenderer->scanlineCache, sizeof(color_t) * DS_VIDEO_VERTICAL_PIXELS * DS_VIDEO_HORIZONTAL_PIXELS);
}

static void DSGXSoftwareRendererInvalidateTex(struct DSGXRenderer* renderer, int slot) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	// TODO
}

static void _preparePoly(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXSoftwarePolygon* poly, int polyId) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	struct DSGXSoftwareEdge* edge = DSGXSoftwareEdgeListAppend(&softwareRenderer->activeEdges);
	poly->texFormat = DSGXTexParamsGetFormat(poly->poly->texParams);
	poly->blendFormat = DSGXPolygonAttrsGetMode(poly->poly->polyParams);
	poly->texW = 8 << DSGXTexParamsGetSSize(poly->poly->texParams);
	poly->texH = 8 << DSGXTexParamsGetTSize(poly->poly->texParams);
	if (!renderer->tex[DSGXTexParamsGetVRAMBase(poly->poly->texParams) >> VRAM_BLOCK_OFFSET]) {
		poly->texBase = NULL;
		poly->palBase = NULL;
	} else {
		switch (poly->texFormat) {
		case 0:
		case 7:
			poly->texBase = NULL;
			poly->palBase = NULL;
			break;
		case 2:
			poly->texBase = &renderer->tex[DSGXTexParamsGetVRAMBase(poly->poly->texParams) >> VRAM_BLOCK_OFFSET][(DSGXTexParamsGetVRAMBase(poly->poly->texParams) << 2) & 0xFFFF];
			poly->palBase = &renderer->texPal[poly->poly->palBase >> 11][(poly->poly->palBase << 2) & 0x1FFF];
			break;
		default:
			poly->texBase = &renderer->tex[DSGXTexParamsGetVRAMBase(poly->poly->texParams) >> VRAM_BLOCK_OFFSET][(DSGXTexParamsGetVRAMBase(poly->poly->texParams) << 2) & 0xFFFF];
			poly->palBase = &renderer->texPal[poly->poly->palBase >> 10][(poly->poly->palBase << 3) & 0x1FFF];
			break;
		}
	}
	edge->polyId = polyId;

	struct DSGXVertex* v0 = &verts[poly->poly->vertIds[0]];
	struct DSGXVertex* v1;

	int32_t v0x = (v0->viewCoord[0] + v0->viewCoord[3]) * (int64_t) (renderer->viewportWidth << 12) / (v0->viewCoord[3] * 2) + (renderer->viewportX << 12);
	int32_t v0y = (-v0->viewCoord[1] + v0->viewCoord[3]) * (int64_t) (renderer->viewportHeight << 12) / (v0->viewCoord[3] * 2) + (renderer->viewportY << 12);

	int v;
	for (v = 1; v < poly->poly->verts; ++v) {
		v1 = &verts[poly->poly->vertIds[v]];
		int32_t v1x = (v1->viewCoord[0] + v1->viewCoord[3]) * (int64_t) (renderer->viewportWidth << 12) / (v1->viewCoord[3] * 2) + (renderer->viewportX << 12);
		int32_t v1y = (-v1->viewCoord[1] + v1->viewCoord[3]) * (int64_t) (renderer->viewportHeight << 12) / (v1->viewCoord[3] * 2) + (renderer->viewportY << 12);

		if (v0y <= v1y) {
			edge->y0 = v0y;
			edge->x0 = v0x;
			edge->z0 = v0->viewCoord[2];
			edge->w0 = v0->viewCoord[3];
			_expandColor(v0->color, &edge->cr0, &edge->cg0, &edge->cb0);
			edge->s0 = v0->vs;
			edge->t0 = v0->vt;

			edge->y1 = v1y;
			edge->x1 = v1x;
			edge->z1 = v1->viewCoord[2];
			edge->w1 = v1->viewCoord[3];
			_expandColor(v1->color, &edge->cr1, &edge->cg1, &edge->cb1);
			edge->s1 = v1->vs;
			edge->t1 = v1->vt;
		} else {
			edge->y0 = v1y;
			edge->x0 = v1x;
			edge->z0 = v1->viewCoord[2];
			edge->w0 = v1->viewCoord[3];
			_expandColor(v1->color, &edge->cr0, &edge->cg0, &edge->cb0);
			edge->s0 = v1->vs;
			edge->t0 = v1->vt;

			edge->y1 = v0y;
			edge->x1 = v0x;
			edge->z1 = v0->viewCoord[2];
			edge->w1 = v0->viewCoord[3];
			_expandColor(v0->color, &edge->cr1, &edge->cg1, &edge->cb1);
			edge->s1 = v0->vs;
			edge->t1 = v0->vt;
		}

		edge = DSGXSoftwareEdgeListAppend(&softwareRenderer->activeEdges);
		edge->polyId = polyId;
		v0 = v1;
		v0x = v1x;
		v0y = v1y;
	}

	v1 = &verts[poly->poly->vertIds[0]];
	int32_t v1x = (v1->viewCoord[0] + v1->viewCoord[3]) * (int64_t) (renderer->viewportWidth << 12) / (v1->viewCoord[3] * 2) + (renderer->viewportX << 12);
	int32_t v1y = (-v1->viewCoord[1] + v1->viewCoord[3]) * (int64_t) (renderer->viewportHeight << 12) / (v1->viewCoord[3] * 2) + (renderer->viewportY << 12);

	if (v0y <= v1y) {
		edge->y0 = v0y;
		edge->x0 = v0x;
		edge->z0 = v0->viewCoord[2];
		edge->w0 = v0->viewCoord[3];
		_expandColor(v0->color, &edge->cr0, &edge->cg0, &edge->cb0);
		edge->s0 = v0->vs;
		edge->t0 = v0->vt;

		edge->y1 = v1y;
		edge->x1 = v1x;
		edge->z1 = v1->viewCoord[2];
		edge->w1 = v1->viewCoord[3];
		_expandColor(v1->color, &edge->cr1, &edge->cg1, &edge->cb1);
		edge->s1 = v1->vs;
		edge->t1 = v1->vt;
	} else {
		edge->y0 = v1y;
		edge->x0 = v1x;
		edge->z0 = v1->viewCoord[2];
		edge->w0 = v1->viewCoord[3];
		_expandColor(v1->color, &edge->cr0, &edge->cg0, &edge->cb0);
		edge->s0 = v1->vs;
		edge->t0 = v1->vt;

		edge->y1 = v0y;
		edge->x1 = v0x;
		edge->z1 = v0->viewCoord[2];
		edge->w1 = v0->viewCoord[3];
		_expandColor(v0->color, &edge->cr1, &edge->cg1, &edge->cb1);
		edge->s1 = v0->vs;
		edge->t1 = v0->vt;
	}
}

static void DSGXSoftwareRendererSetRAM(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXPolygon* polys, unsigned polyCount, bool wSort) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;

	softwareRenderer->flushPending = true;
	softwareRenderer->sort = wSort ? 3 : 2;
	softwareRenderer->verts = verts;
	DSGXSoftwarePolygonListClear(&softwareRenderer->activePolys);
	DSGXSoftwareEdgeListClear(&softwareRenderer->activeEdges);
	unsigned i;
	// Pass 1: Opaque
	for (i = 0; i < polyCount; ++i) {
		struct DSGXSoftwarePolygon* poly = NULL;
		switch (DSGXTexParamsGetFormat(polys[i].texParams)) {
		default:
			if (DSGXPolygonAttrsGetAlpha(polys[i].polyParams) == 0x1F) {
				poly = DSGXSoftwarePolygonListAppend(&softwareRenderer->activePolys);
				break;
			}
		case 1:
		case 6:
			break;
		}
		if (!poly) {
			continue;
		}
		poly->poly = &polys[i];
		_preparePoly(renderer, verts, poly, DSGXSoftwarePolygonListSize(&softwareRenderer->activePolys) - 1);
	}
	// Pass 2: Translucent
	for (i = 0; i < polyCount; ++i) {
		struct DSGXSoftwarePolygon* poly = NULL;
		switch (DSGXTexParamsGetFormat(polys[i].texParams)) {
		default:
			if (DSGXPolygonAttrsGetAlpha(polys[i].polyParams) == 0x1F) {
				break;
			}
		case 1:
		case 6:
			poly = DSGXSoftwarePolygonListAppend(&softwareRenderer->activePolys);
			break;
		}
		if (!poly) {
			continue;
		}
		poly->poly = &polys[i];
		_preparePoly(renderer, verts, poly, DSGXSoftwarePolygonListSize(&softwareRenderer->activePolys) - 1);
	}
}

static void DSGXSoftwareRendererDrawScanline(struct DSGXRenderer* renderer, int y) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	if (!softwareRenderer->flushPending) {
		return;
	}
	DSGXSoftwareSpanListClear(&softwareRenderer->activeSpans);
	memset(softwareRenderer->bucket, 0, sizeof(*softwareRenderer->bucket) * DS_GX_POLYGON_BUFFER_SIZE);
	size_t i;
	for (i = 0; i < DSGXSoftwareEdgeListSize(&softwareRenderer->activeEdges); ++i) {
		struct DSGXSoftwareEdge* edge = DSGXSoftwareEdgeListGetPointer(&softwareRenderer->activeEdges, i);
		if (edge->y1 < (y << 12)) {
			continue;
		} else if (edge->y0 > (y << 12)) {
			continue;
		}

		unsigned poly = edge->polyId;
		struct DSGXSoftwareSpan* span = softwareRenderer->bucket[poly];
		if (span && !span->ep[1].coord[3]) {
			if (_edgeToSpan(span, edge, 1, y << 12)) {
				_createStep(span);
				softwareRenderer->bucket[poly] = NULL;
			}
		} else if (!span) {
			span = DSGXSoftwareSpanListAppend(&softwareRenderer->activeSpans);
			memset(&span->ep[1], 0, sizeof(span->ep[1]));
			span->poly = DSGXSoftwarePolygonListGetPointer(&softwareRenderer->activePolys, poly);
			span->polyId = DSGXPolygonAttrsGetId(span->poly->poly->polyParams);
			if (!_edgeToSpan(span, edge, 0, y << 12)) {
				// Horizontal line
				DSGXSoftwareSpanListShift(&softwareRenderer->activeSpans, DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans) - 1, 1);
			} else {
				softwareRenderer->bucket[poly] = span;
			}
		}
	}

	color_t* scanline = &softwareRenderer->scanlineCache[DS_VIDEO_HORIZONTAL_PIXELS * y];
	memset(scanline, 0, sizeof(color_t) * DS_VIDEO_HORIZONTAL_PIXELS);
	memset(softwareRenderer->stencilBuffer, 0, sizeof(softwareRenderer->stencilBuffer[0]) * DS_VIDEO_HORIZONTAL_PIXELS);
	for (i = 0; i < DS_VIDEO_HORIZONTAL_PIXELS; i += 4) {
		softwareRenderer->depthBuffer[i] = INT32_MAX;
		softwareRenderer->depthBuffer[i + 1] = INT32_MAX;
		softwareRenderer->depthBuffer[i + 2] = INT32_MAX;
		softwareRenderer->depthBuffer[i + 3] = INT32_MAX;
	}

	for (i = 0; i < DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans); ++i) {
		struct DSGXSoftwareSpan* span = DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, i);

		int32_t x = span->ep[0].coord[0] >> 12;
		if (x < 0) {
			x = 0;
		}
		unsigned stencilValue = span->polyId;
		if (span->poly->blendFormat == 3) {
			stencilValue |= 0x40;
		}
		for (; x < (span->ep[1].coord[0] >> 12) && x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			if (span->ep[0].coord[softwareRenderer->sort] < softwareRenderer->depthBuffer[x]) {
				_resolveEndpoint(span);
				color_t color = _lookupColor(softwareRenderer, &span->ep[0], span->poly);
				unsigned a = color >> 27;
				unsigned current = scanline[x];
				unsigned b = current >> 27;
				unsigned ab = a;
				unsigned s = stencilValue;
				if (b > ab) {
					ab = b;
				}
				if (a == 0x1F) {
					if (!(s == 0x40 || (softwareRenderer->stencilBuffer[x] & 0x40))) {
						softwareRenderer->depthBuffer[x] = span->ep[0].coord[softwareRenderer->sort];
						scanline[x] = color;
						s &= ~0x40;
					}
					softwareRenderer->stencilBuffer[x] = s;
				} else if (a) {
					// TODO: Disable alpha?
					if (b) {
						color = _mix32(a, color, 0x1F - a, current);
						color |= ab << 27;
					}
					if (softwareRenderer->stencilBuffer[x] != s) {
						if (!(s == 0x40 || (softwareRenderer->stencilBuffer[x] & 0x40))) {
							if (DSGXPolygonAttrsIsUpdateDepth(span->poly->poly->polyParams)) {
								softwareRenderer->depthBuffer[x] = span->ep[0].coord[softwareRenderer->sort];
							}
							scanline[x] = color;
							s &= ~0x40;
						}
						softwareRenderer->stencilBuffer[x] = s;
					}
				}
			}
			_stepEndpoint(span);
		}
	}

	if (y == DS_VIDEO_VERTICAL_PIXELS - 1) {
		softwareRenderer->flushPending = false;
	}
}

static void DSGXSoftwareRendererGetScanline(struct DSGXRenderer* renderer, int y, const color_t** output) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	*output = &softwareRenderer->scanlineCache[DS_VIDEO_HORIZONTAL_PIXELS * y];
}
