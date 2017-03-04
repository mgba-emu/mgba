/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/gx/software.h>

#include <mgba-util/memory.h>
#include "gba/renderers/software-private.h"

#define SCREEN_SIZE (DS_VIDEO_VERTICAL_PIXELS << 12)

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

static color_t _lookupColor(struct DSGXSoftwareEndpoint* ep, struct DSGXSoftwarePolygon* poly) {
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
			s = poly->texW - s;
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
			t = poly->texH - t;
		}
		t &= poly->texH - 1;
	} else {
		t &= poly->texH - 1;
	}

	uint16_t texelCoord = s + t * poly->texW;
	uint8_t a = 0x1F;
	switch (poly->texFormat) {
	case 0:
	default:
		return _finishColor(ep->cr, ep->cg, ep->cb, 0x1F);
	case 1:
		texel = ((uint8_t*) poly->texBase)[texelCoord];
		a = (texel >> 5) & 0x7;
		a = (a << 2) + (a >> 1);
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
		return _finishColor(0x3F, 0, 0x3F, 0x1F);
	case 6:
		texel = ((uint8_t*) poly->texBase)[texelCoord];
		a = (texel >> 3) & 0x1F;
		texel &= 0x7;
		break;
	case 7:
		return _finishColor(0x3F, 0x3F, 0x3F, 0x1F);
	}
	if (DSGXTexParamsIs0Transparent(poly->poly->texParams) && !texel) {
		return 0;
	}
	uint8_t r, g, b;
	unsigned wr, wg, wb;
	texel = poly->palBase[texel];
	_expandColor(texel, &r, &g, &b);
	switch (poly->blendFormat) {
	case 1:
	default:
		// TODO: Alpha
		return _finishColor(r, g, b, a);
	case 0:
		wr = ((r + 1) * (ep->cr + 1) - 1) >> 6;
		wg = ((g + 1) * (ep->cg + 1) - 1) >> 6;
		wb = ((b + 1) * (ep->cb + 1) - 1) >> 6;
		return _finishColor(wr, wg, wb, a);
	}
}

static int _edgeSort(const void* a, const void* b) {
	const struct DSGXSoftwareEdge* ea = a;
	const struct DSGXSoftwareEdge* eb = b;

	// Sort upside down
	if (ea->y0 < eb->y0) {
		return 1;
	}
	if (ea->y0 > eb->y0) {
		return -1;
	}
	if (ea->y1 < eb->y1) {
		return 1;
	}
	if (ea->y1 > eb->y1) {
		return -1;
	}
	return 0;
}

static bool _edgeToSpan(struct DSGXSoftwareSpan* span, const struct DSGXSoftwareEdge* edge, int index, int32_t y) {
	int32_t height = edge->y1 - edge->y0;
	int64_t yw = (y << 12) - edge->y0;
	if (!height) {
		return false;
	}
	// Clamp to bounds
	if (yw < 0) {
		yw = 0;
	} else if (yw > height) {
		yw = height;
	}
	yw *= 0x100000000LL / height;

	span->ep[index].x = (((int64_t) (edge->x1 - edge->x0) * yw) >> 32) + edge->x0;

	if (index && span->ep[0].x > span->ep[index].x) {
		int32_t temp = span->ep[index].x;
		span->ep[index] = span->ep[0];
		span->ep[0].x = temp;
		index = 0;
	}
	int32_t w0 = edge->w0;
	int32_t w1 = edge->w1;
	int32_t w = (((int64_t) (edge->w1 - edge->w0) * yw) >> 32) + edge->w0;
	int64_t wRecip;// = 0x1000000000000LL / w;
	// XXX: Disable perspective correction until I figure out how to fix it
	wRecip = 0x100000000;
	w0 = 0x10000;
	w1 = 0x10000;
	span->ep[index].w = w;
	span->ep[index].z = (((edge->z1 - edge->z0) * yw) >> 32) + edge->z0;
	span->ep[index].cr = (((((edge->cr1 * (int64_t) w1 - edge->cr0 * (int64_t) w0) * yw) >> 32) + edge->cr0 * (int64_t) w0) * wRecip) >> 48;
	span->ep[index].cg = (((((edge->cg1 * (int64_t) w1 - edge->cg0 * (int64_t) w0) * yw) >> 32) + edge->cg0 * (int64_t) w0) * wRecip) >> 48;
	span->ep[index].cb = (((((edge->cb1 * (int64_t) w1 - edge->cb0 * (int64_t) w0) * yw) >> 32) + edge->cb0 * (int64_t) w0) * wRecip) >> 48;
	span->ep[index].s = (((((edge->s1 * (int64_t) w1 - edge->s0 * (int64_t) w0) * yw) >> 32) + edge->s0 * (int64_t) w0) * wRecip) >> 48;
	span->ep[index].t = (((((edge->t1 * (int64_t) w1 - edge->t0 * (int64_t) w0) * yw) >> 32) + edge->t0 * (int64_t) w0) * wRecip) >> 48;

	return true;
}

static int _spanSort(const void* a, const void* b) {
	const struct DSGXSoftwareSpan* sa = a;
	const struct DSGXSoftwareSpan* sb = b;

	// Sort backwards
	if (sa->ep[0].x < sb->ep[0].x) {
		return 1;
	}
	if (sa->ep[0].x > sb->ep[0].x) {
		return -1;
	}
	if (sa->ep[0].w < sb->ep[0].w) {
		return 1;
	}
	if (sa->ep[0].w > sb->ep[0].w) {
		return -1;
	}
	return 0;
}

static void _lerpEndpoint(const struct DSGXSoftwareSpan* span, struct DSGXSoftwareEndpoint* ep, unsigned x) {
	int64_t width = span->ep[1].x - span->ep[0].x;
	int64_t xw = ((uint64_t) x << 12) - span->ep[0].x;
	if (!width) {
		return; // TODO?
	}
	// Clamp to bounds
	if (xw < 0) {
		xw = 0;
	} else if (xw > width) {
		xw = width;
	}
	xw *= 0x100000000LL / width;
	int32_t w0 = span->ep[0].w;
	int32_t w1 = span->ep[1].w;
	int64_t w = (((int64_t) (w1 - w0) * xw) >> 32) + w0;
	int64_t wRecip;// = 0x1000000000000LL / w;
	ep->w = w;
	// XXX: Disable perspective correction until I figure out how to fix it
	wRecip = 0x100000000;
	w0 = 0x10000;
	w1 = 0x10000;

	ep->z = (((span->ep[1].z - span->ep[0].z) * xw) >> 32) + span->ep[0].z;

	uint64_t r = (((span->ep[1].cr * (int64_t) w1 - span->ep[0].cr * (int64_t) w0) * xw) >> 32) + span->ep[0].cr * (int64_t) w0;
	uint64_t g = (((span->ep[1].cg * (int64_t) w1 - span->ep[0].cg * (int64_t) w0) * xw) >> 32) + span->ep[0].cg * (int64_t) w0;
	uint64_t b = (((span->ep[1].cb * (int64_t) w1 - span->ep[0].cb * (int64_t) w0) * xw) >> 32) + span->ep[0].cb * (int64_t) w0;
	ep->cr = (r * wRecip) >> 48;
	ep->cg = (g * wRecip) >> 48;
	ep->cb = (b * wRecip) >> 48;

	int32_t s = (((span->ep[1].s * (int64_t) w1 - span->ep[0].s * (int64_t) w0) * xw) >> 32) + span->ep[0].s * (int64_t) w0;
	int32_t t = (((span->ep[1].t * (int64_t) w1 - span->ep[0].t * (int64_t) w0) * xw) >> 32) + span->ep[0].t * (int64_t) w0;
	ep->s = (s * wRecip) >> 48;
	ep->t = (t * wRecip) >> 48;
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
			poly->palBase = &renderer->texPal[poly->poly->palBase >> 12][(poly->poly->palBase << 2) & 0x1FFF];
			break;
		default:
			poly->texBase = &renderer->tex[DSGXTexParamsGetVRAMBase(poly->poly->texParams) >> VRAM_BLOCK_OFFSET][(DSGXTexParamsGetVRAMBase(poly->poly->texParams) << 2) & 0xFFFF];
			poly->palBase = &renderer->texPal[poly->poly->palBase >> 11][(poly->poly->palBase << 3) & 0x1FFF];
			break;
		}
		edge->polyId = i;

		struct DSGXVertex* v0 = &verts[poly->poly->vertIds[0]];
		struct DSGXVertex* v1;

		int v;
		for (v = 1; v < poly->poly->verts; ++v) {
			v1 = &verts[poly->poly->vertIds[v]];
			if (v0->vy >= v1->vy) {
				edge->y0 = SCREEN_SIZE - v0->vy;
				edge->x0 = v0->vx;
				edge->z0 = v0->vz;
				edge->w0 = v0->vw;
				_expandColor(v0->color, &edge->cr0, &edge->cg0, &edge->cb0);
				edge->s0 = v0->vs;
				edge->t0 = v0->vt;

				edge->y1 = SCREEN_SIZE - v1->vy;
				edge->x1 = v1->vx;
				edge->z1 = v1->vz;
				edge->w1 = v1->vw;
				_expandColor(v1->color, &edge->cr1, &edge->cg1, &edge->cb1);
				edge->s1 = v1->vs;
				edge->t1 = v1->vt;
			} else {
				edge->y0 = SCREEN_SIZE - v1->vy;
				edge->x0 = v1->vx;
				edge->z0 = v1->vz;
				edge->w0 = v1->vw;
				_expandColor(v1->color, &edge->cr0, &edge->cg0, &edge->cb0);
				edge->s0 = v1->vs;
				edge->t0 = v1->vt;

				edge->y1 = SCREEN_SIZE - v0->vy;
				edge->x1 = v0->vx;
				edge->z1 = v0->vz;
				edge->w1 = v0->vw;
				_expandColor(v0->color, &edge->cr1, &edge->cg1, &edge->cb1);
				edge->s1 = v0->vs;
				edge->t1 = v0->vt;
			}

			edge = DSGXSoftwareEdgeListAppend(&softwareRenderer->activeEdges);
			edge->polyId = i;
			v0 = v1;
		}

		v1 = &verts[poly->poly->vertIds[0]];
		if (v0->vy >= v1->vy) {
			edge->y0 = SCREEN_SIZE - v0->vy;
			edge->x0 = v0->vx;
			edge->z0 = v0->vz;
			edge->w0 = v0->vw;
			_expandColor(v0->color, &edge->cr0, &edge->cg0, &edge->cb0);
			edge->s0 = v0->vs;
			edge->t0 = v0->vt;

			edge->y1 = SCREEN_SIZE - v1->vy;
			edge->x1 = v1->vx;
			edge->z1 = v1->vz;
			edge->w1 = v1->vw;
			_expandColor(v1->color, &edge->cr1, &edge->cg1, &edge->cb1);
			edge->s1 = v1->vs;
			edge->t1 = v1->vt;
		} else {
			edge->y0 = SCREEN_SIZE - v1->vy;
			edge->x0 = v1->vx;
			edge->w0 = v1->vw;
			edge->z0 = v1->vz;
			_expandColor(v1->color, &edge->cr0, &edge->cg0, &edge->cb0);
			edge->s0 = v1->vs;
			edge->t0 = v1->vt;

			edge->y1 = SCREEN_SIZE - v0->vy;
			edge->x1 = v0->vx;
			edge->z1 = v0->vz;
			edge->w1 = v0->vw;
			_expandColor(v0->color, &edge->cr1, &edge->cg1, &edge->cb1);
			edge->s1 = v0->vs;
			edge->t1 = v0->vt;
		}
	}
	qsort(DSGXSoftwareEdgeListGetPointer(&softwareRenderer->activeEdges, 0), DSGXSoftwareEdgeListSize(&softwareRenderer->activeEdges), sizeof(struct DSGXSoftwareEdge), _edgeSort);
}

static void DSGXSoftwareRendererDrawScanline(struct DSGXRenderer* renderer, int y) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	if (!softwareRenderer->flushPending) {
		return;
	}
	DSGXSoftwareSpanListClear(&softwareRenderer->activeSpans);
	memset(softwareRenderer->bucket, 0, sizeof(*softwareRenderer->bucket) * DS_GX_POLYGON_BUFFER_SIZE);
	int i;
	for (i = DSGXSoftwareEdgeListSize(&softwareRenderer->activeEdges); i; --i) {
		size_t idx = i - 1;
		struct DSGXSoftwareEdge* edge = DSGXSoftwareEdgeListGetPointer(&softwareRenderer->activeEdges, idx);
		if (edge->y1 >> 12 < y) {
			DSGXSoftwareEdgeListShift(&softwareRenderer->activeEdges, idx, 1);
			continue;
		} else if (edge->y0 >> 12 > y) {
			continue;
		}

		unsigned poly = edge->polyId;
		struct DSGXSoftwareSpan* span = softwareRenderer->bucket[poly];
		if (span && !span->ep[1].w) {
			_edgeToSpan(span, edge, 1, y);
			softwareRenderer->bucket[poly] = NULL;
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
	qsort(DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, 0), DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans), sizeof(struct DSGXSoftwareSpan), _spanSort);

	color_t* scanline = &softwareRenderer->scanlineCache[DS_VIDEO_HORIZONTAL_PIXELS * y];

	int nextSpanX = DS_VIDEO_HORIZONTAL_PIXELS;
	if (DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans)) {
		nextSpanX = DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans) - 1)->ep[0].x;
		nextSpanX >>= 12;
	}
	for (i = 0; i < DS_VIDEO_HORIZONTAL_PIXELS; ++i) {
		struct DSGXSoftwareSpan* span = NULL;
		struct DSGXSoftwareEndpoint ep;
		int32_t depth = INT32_MAX;
		scanline[i] = 0;
		if (i >= nextSpanX) {
			size_t nextSpanId = DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans);
			span = DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, nextSpanId - 1);
			while (i > (span->ep[1].x >> 12) || !span->ep[1].x) {
				DSGXSoftwareSpanListShift(&softwareRenderer->activeSpans, nextSpanId - 1, 1);
				--nextSpanId;
				if (!nextSpanId) {
					nextSpanX = DS_VIDEO_HORIZONTAL_PIXELS;
					span = NULL;
					break;
				}
				span = DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, nextSpanId - 1);
				nextSpanX = span->ep[0].x >> 12;
			}
			while (span && i > (span->ep[0].x >> 12)) {
				if (i <= (span->ep[1].x >> 12)) {
					_lerpEndpoint(span, &ep, i);
					color_t color = _lookupColor(&ep, span->poly);
					if (color & 0xF8000000) {
						// TODO: Alpha
						if (softwareRenderer->wSort) {
							if (ep.w < depth) {
								depth = ep.w;
								scanline[i] = color;
							}
						} else {
							if (ep.z < depth) {
								depth = ep.z;
								scanline[i] = color;
							}
						}
					}
				}
				--nextSpanId;
				if (!nextSpanId) {
					break;
				}
				span = DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, nextSpanId - 1);
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
