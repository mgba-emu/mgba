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
	color_t rgba = (r << 2) & 0xF8;
	rgba |= (g << 10) & 0xF800;
	rgba |= (b << 18) & 0xF80000;
	rgba |= (a << 27) & 0xF8000000;
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
		// TODO: Slot 2 uses upper half
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
	switch (poly->blendFormat) {
	case 1:
	default:
		// TODO: Alpha
		return _finishColor(r, g, b, pa);
	case 0:
		wr = ((r + 1) * (ep->cr + 1) - 1) >> 6;
		wg = ((g + 1) * (ep->cg + 1) - 1) >> 6;
		wb = ((b + 1) * (ep->cb + 1) - 1) >> 6;
		wa = ((ta + 1) * (pa + 1) - 1) >> 5;
		return _finishColor(wr, wg, wb, wa);
	}
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

	span->ep[index].x = (((int64_t) (edge->x1 - edge->x0) * yw) / height) + edge->x0;

	if (index) {
		if (span->ep[0].x == span->ep[index].x) {
			return false;
		}
		if (span->ep[0].x > span->ep[index].x) {
			int32_t temp = span->ep[index].x;
			span->ep[index] = span->ep[0];
			span->ep[0].x = temp;
			index = 0;
		}
	}
	span->ep[index].w = (((int64_t) (edge->w1 - edge->w0) * yw) / height) + edge->w0;
	span->ep[index].z = (((int64_t) (edge->z1 - edge->z0) * yw) / height) + edge->z0;
	span->ep[index].cr = (((int64_t) (edge->cr1 - edge->cr0) * yw) / height) + edge->cr0;
	span->ep[index].cg = (((int64_t) (edge->cg1 - edge->cg0) * yw) / height) + edge->cg0;
	span->ep[index].cb = (((int64_t) (edge->cb1 - edge->cb0) * yw) / height) + edge->cb0;
	span->ep[index].s = (((int64_t) (edge->s1 - edge->s0) * yw) / height) + edge->s0;
	span->ep[index].t = (((int64_t) (edge->t1 - edge->t0) * yw) / height) + edge->t0;

	return true;
}

static void _lerpEndpoint(const struct DSGXSoftwareSpan* span, struct DSGXSoftwareEndpoint* ep, unsigned x) {
	int64_t width = span->ep[1].x - span->ep[0].x;
	int64_t xw = x - span->ep[0].x;
	if (!width) {
		return; // TODO?
	}
	// Clamp to bounds
	if (xw < 0) {
		xw = 0;
	} else if (xw > width) {
		xw = width;
	}
	ep->w = (((int64_t) (span->ep[1].w - span->ep[0].w) * xw) / width) + span->ep[0].w;
	ep->z = (((int64_t) (span->ep[1].z - span->ep[0].z) * xw) / width) + span->ep[0].z;
	ep->cr = (((int64_t) (span->ep[1].cr - span->ep[0].cr) * xw) / width) + span->ep[0].cr;
	ep->cg = (((int64_t) (span->ep[1].cg - span->ep[0].cg) * xw) / width) + span->ep[0].cg;
	ep->cb = (((int64_t) (span->ep[1].cb - span->ep[0].cb) * xw) / width) + span->ep[0].cb;
	ep->s = (((int64_t) (span->ep[1].s - span->ep[0].s) * xw) / width) + span->ep[0].s;
	ep->t = (((int64_t) (span->ep[1].t - span->ep[0].t) * xw) / width) + span->ep[0].t;
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

static void DSGXSoftwareRendererSetRAM(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXPolygon* polys, unsigned polyCount, bool wSort) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;

	softwareRenderer->flushPending = true;
	softwareRenderer->wSort = wSort;
	softwareRenderer->verts = verts;
	DSGXSoftwarePolygonListClear(&softwareRenderer->activePolys);
	DSGXSoftwareEdgeListClear(&softwareRenderer->activeEdges);
	unsigned i;
	for (i = 0; i < polyCount; ++i) {
		struct DSGXSoftwarePolygon* poly = DSGXSoftwarePolygonListAppend(&softwareRenderer->activePolys);
		struct DSGXSoftwareEdge* edge = DSGXSoftwareEdgeListAppend(&softwareRenderer->activeEdges);
		poly->poly = &polys[i];
		poly->texFormat = DSGXTexParamsGetFormat(poly->poly->texParams);
		poly->blendFormat = DSGXPolygonAttrsGetMode(poly->poly->polyParams);
		poly->texW = 8 << DSGXTexParamsGetSSize(poly->poly->texParams);
		poly->texH = 8 << DSGXTexParamsGetTSize(poly->poly->texParams);
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
		edge->polyId = i;

		struct DSGXVertex* v0 = &verts[poly->poly->vertIds[0]];
		struct DSGXVertex* v1;

		int32_t v0x = (v0->vx + v0->vw) * (int64_t) renderer->viewportWidth / (v0->vw * 2) + renderer->viewportX;
		int32_t v0y = (-v0->vy + v0->vw) * (int64_t) renderer->viewportHeight / (v0->vw * 2) + renderer->viewportY;

		int v;
		for (v = 1; v < poly->poly->verts; ++v) {
			v1 = &verts[poly->poly->vertIds[v]];
			int32_t v1x = (v1->vx + v1->vw) * (int64_t) renderer->viewportWidth / (v1->vw * 2) + renderer->viewportX;
			int32_t v1y = (-v1->vy + v1->vw) * (int64_t) renderer->viewportHeight / (v1->vw * 2) + renderer->viewportY;

			if (v0y <= v1y) {
				edge->y0 = v0y;
				edge->x0 = v0x;
				edge->z0 = v0->vz;
				edge->w0 = v0->vw;
				_expandColor(v0->color, &edge->cr0, &edge->cg0, &edge->cb0);
				edge->s0 = v0->vs;
				edge->t0 = v0->vt;

				edge->y1 = v1y;
				edge->x1 = v1x;
				edge->z1 = v1->vz;
				edge->w1 = v1->vw;
				_expandColor(v1->color, &edge->cr1, &edge->cg1, &edge->cb1);
				edge->s1 = v1->vs;
				edge->t1 = v1->vt;
			} else {
				edge->y0 = v1y;
				edge->x0 = v1x;
				edge->z0 = v1->vz;
				edge->w0 = v1->vw;
				_expandColor(v1->color, &edge->cr0, &edge->cg0, &edge->cb0);
				edge->s0 = v1->vs;
				edge->t0 = v1->vt;

				edge->y1 = v0y;
				edge->x1 = v0x;
				edge->z1 = v0->vz;
				edge->w1 = v0->vw;
				_expandColor(v0->color, &edge->cr1, &edge->cg1, &edge->cb1);
				edge->s1 = v0->vs;
				edge->t1 = v0->vt;
			}

			edge = DSGXSoftwareEdgeListAppend(&softwareRenderer->activeEdges);
			edge->polyId = i;
			v0 = v1;
			v0x = v1x;
			v0y = v1y;
		}

		v1 = &verts[poly->poly->vertIds[0]];
		int32_t v1x = (v1->vx + v1->vw) * (int64_t) renderer->viewportWidth / (v1->vw * 2) + renderer->viewportX;
		int32_t v1y = (-v1->vy + v1->vw) * (int64_t) renderer->viewportHeight / (v1->vw * 2) + renderer->viewportY;

		if (v0y <= v1y) {
			edge->y0 = v0y;
			edge->x0 = v0x;
			edge->z0 = v0->vz;
			edge->w0 = v0->vw;
			_expandColor(v0->color, &edge->cr0, &edge->cg0, &edge->cb0);
			edge->s0 = v0->vs;
			edge->t0 = v0->vt;

			edge->y1 = v1y;
			edge->x1 = v1x;
			edge->z1 = v1->vz;
			edge->w1 = v1->vw;
			_expandColor(v1->color, &edge->cr1, &edge->cg1, &edge->cb1);
			edge->s1 = v1->vs;
			edge->t1 = v1->vt;
		} else {
			edge->y0 = v1y;
			edge->x0 = v1x;
			edge->z0 = v1->vz;
			edge->w0 = v1->vw;
			_expandColor(v1->color, &edge->cr0, &edge->cg0, &edge->cb0);
			edge->s0 = v1->vs;
			edge->t0 = v1->vt;

			edge->y1 = v0y;
			edge->x1 = v0x;
			edge->z1 = v0->vz;
			edge->w1 = v0->vw;
			_expandColor(v0->color, &edge->cr1, &edge->cg1, &edge->cb1);
			edge->s1 = v0->vs;
			edge->t1 = v0->vt;
		}
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
		if (edge->y1 < y) {
			continue;
		} else if (edge->y0 > y) {
			continue;
		}

		unsigned poly = edge->polyId;
		struct DSGXSoftwareSpan* span = softwareRenderer->bucket[poly];
		if (span && !span->ep[1].w) {
			if (_edgeToSpan(span, edge, 1, y)) {
				softwareRenderer->bucket[poly] = NULL;
			}
		} else if (!span) {
			span = DSGXSoftwareSpanListAppend(&softwareRenderer->activeSpans);
			memset(&span->ep[1], 0, sizeof(span->ep[1]));
			span->poly = DSGXSoftwarePolygonListGetPointer(&softwareRenderer->activePolys, poly);
			if (!_edgeToSpan(span, edge, 0, y)) {
				// Horizontal line
				DSGXSoftwareSpanListShift(&softwareRenderer->activeSpans, DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans) - 1, 1);
			} else {
				softwareRenderer->bucket[poly] = span;
			}
		}
	}

	color_t* scanline = &softwareRenderer->scanlineCache[DS_VIDEO_HORIZONTAL_PIXELS * y];
	memset(scanline, 0, sizeof(color_t) * DS_VIDEO_HORIZONTAL_PIXELS);
	for (i = 0; i < DS_VIDEO_HORIZONTAL_PIXELS; i += 4) {
		softwareRenderer->depthBuffer[i] = INT32_MAX;
		softwareRenderer->depthBuffer[i + 1] = INT32_MAX;
		softwareRenderer->depthBuffer[i + 2] = INT32_MAX;
		softwareRenderer->depthBuffer[i + 3] = INT32_MAX;
	}

	for (i = 0; i < DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans); ++i) {
		struct DSGXSoftwareSpan* span = DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, i);

		int32_t x = span->ep[0].x;
		if (x < 0) {
			x = 0;
		}
		for (; x < span->ep[1].x && x < DS_VIDEO_HORIZONTAL_PIXELS; ++x) {
			struct DSGXSoftwareEndpoint ep;
			_lerpEndpoint(span, &ep, x);
			color_t color = _lookupColor(softwareRenderer, &ep, span->poly);
			unsigned a = color >> 27;
			unsigned current = scanline[x];
			unsigned b = current >> 27;
			unsigned ab = a;
			if (b > ab) {
				ab = b;
			}
			if (a == 0x1F) {
				if (softwareRenderer->wSort) {
					if (ep.w < softwareRenderer->depthBuffer[x]) {
						softwareRenderer->depthBuffer[x] = ep.w;
						scanline[x] = color;
					} else if (b < 0x1F) {
						scanline[x] = _mix32(b, current, 0x1F - b, color) | a << 27;
					}
				} else {
					if (ep.z < softwareRenderer->depthBuffer[x]) {
						softwareRenderer->depthBuffer[x] = ep.z;
						scanline[x] = color;
					} else if (b < 0x1F) {
						scanline[x] = _mix32(b, current, 0x1F - b, color) | a << 27;
					}
				}
			} else if (a) {
				// TODO: Disable alpha?
				color = _mix32(a, color, 0x1F - a, current);
				color |= ab << 27;
				if (softwareRenderer->wSort) {
					if (ep.w < softwareRenderer->depthBuffer[x]) {
						softwareRenderer->depthBuffer[x] = ep.w;
						scanline[x] = color;
					} else if (b < 0x1F) {
						scanline[x] = _mix32(b, current, 0x1F - b, color) | ab << 27;
					}
				} else {
					if (ep.z < softwareRenderer->depthBuffer[x]) {
						softwareRenderer->depthBuffer[x] = ep.z;
						scanline[x] = color;
					} else if (b < 0x1F) {
						scanline[x] = _mix32(b, current, 0x1F - b, color) | ab << 27;
					}
				}
			}
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
