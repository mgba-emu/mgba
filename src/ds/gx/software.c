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
static void DSGXSoftwareRendererSetRAM(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXPolygon* polys, unsigned polyCount);
static void DSGXSoftwareRendererDrawScanline(struct DSGXRenderer* renderer, int y);
static void DSGXSoftwareRendererGetScanline(struct DSGXRenderer* renderer, int y, color_t** output);

static void _expandColor(uint16_t c15, int8_t* r, int8_t* g, int8_t* b) {
	*r = ((c15 << 1) & 0x3E) | 1;
	*g = ((c15 >> 4) & 0x3E) | 1;
	*b = ((c15 >> 9) & 0x3E) | 1;
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
	// TODO: Perspective correction
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
	span->ep[index].x = ((int64_t) (edge->x1 - edge->x0) * yw) / height + edge->x0;
	if (index && span->ep[0].x > span->ep[index].x) {
		int32_t temp = span->ep[index].x;
		span->ep[index] = span->ep[0];
		span->ep[0].x = temp;
		index = 0;
	}
	span->ep[index].w = ((int64_t) (edge->w1 - edge->w0) * yw) / height + edge->w0;
	span->ep[index].cr = ((int32_t) (edge->cr1 - edge->cr0) * yw) / height + edge->cr0;
	span->ep[index].cg = ((int32_t) (edge->cg1 - edge->cg0) * yw) / height + edge->cg0;
	span->ep[index].cb = ((int32_t) (edge->cb1 - edge->cb0) * yw) / height + edge->cb0;
	span->ep[index].s = ((int32_t) (edge->s1 - edge->s0) * yw) / height + edge->s0;
	span->ep[index].t = ((int32_t) (edge->t1 - edge->t0) * yw) / height + edge->t0;

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
	if (sa->ep[1].x < sb->ep[1].x) {
		return 1;
	}
	if (sa->ep[1].x > sb->ep[1].x) {
		return -1;
	}
	return 0;
}

static color_t _lerpColor(const struct DSGXSoftwareSpan* span, unsigned x) {
	int64_t width = span->ep[1].x - span->ep[0].x;
	int64_t xw = ((uint64_t) x << 12) - span->ep[0].x;
	if (!width) {
		return 0; // TODO?
	}
	// Clamp to bounds
	if (xw < 0) {
		xw = 0;
	} else if (xw > width) {
		xw = width;
	}
	uint32_t w = ((int64_t) (span->ep[0].w - span->ep[1].w) * xw) / width + span->ep[1].w;

	uint64_t r = ((span->ep[1].cr * (int64_t) span->ep[0].w - span->ep[0].cr * (int64_t) span->ep[1].w) * xw) / width + span->ep[0].cr * (int64_t) span->ep[1].w;
	uint64_t g = ((span->ep[1].cg * (int64_t) span->ep[0].w - span->ep[0].cg * (int64_t) span->ep[1].w) * xw) / width + span->ep[0].cg * (int64_t) span->ep[1].w;
	uint64_t b = ((span->ep[1].cb * (int64_t) span->ep[0].w - span->ep[0].cb * (int64_t) span->ep[1].w) * xw) / width + span->ep[0].cb * (int64_t) span->ep[1].w;
	r /= w;
	g /= w;
	b /= w;
#ifndef COLOR_16_BIT
	color_t rgb = (r << 2) & 0xF8;
	rgb |= (g << 10) & 0xF800;
	rgb |= (b << 18) & 0xF80000;
	return rgb;
#else
#error Unsupported color depth
#endif
}

void DSGXSoftwareRendererCreate(struct DSGXSoftwareRenderer* renderer) {
	renderer->d.init = DSGXSoftwareRendererInit;
	renderer->d.reset = DSGXSoftwareRendererReset;
	renderer->d.deinit = DSGXSoftwareRendererDeinit;
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
	softwareRenderer->scanlineCache = anonymousMemoryMap(sizeof(color_t) * DS_VIDEO_HORIZONTAL_PIXELS * 48);
}

static void DSGXSoftwareRendererReset(struct DSGXRenderer* renderer) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	// TODO
}

static void DSGXSoftwareRendererDeinit(struct DSGXRenderer* renderer) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	DSGXSoftwarePolygonListDeinit(&softwareRenderer->activePolys);
	DSGXSoftwareEdgeListDeinit(&softwareRenderer->activeEdges);	
	DSGXSoftwareSpanListDeinit(&softwareRenderer->activeSpans);
	mappedMemoryFree(softwareRenderer->bucket, sizeof(*softwareRenderer->bucket) * DS_GX_POLYGON_BUFFER_SIZE);
	mappedMemoryFree(softwareRenderer->scanlineCache, sizeof(color_t) * DS_VIDEO_HORIZONTAL_PIXELS * 48);
}

static void DSGXSoftwareRendererSetRAM(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXPolygon* polys, unsigned polyCount) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;

	softwareRenderer->verts = verts;
	DSGXSoftwarePolygonListClear(&softwareRenderer->activePolys);
	DSGXSoftwareEdgeListClear(&softwareRenderer->activeEdges);
	unsigned i;
	for (i = 0; i < polyCount; ++i) {
		struct DSGXSoftwarePolygon* poly = DSGXSoftwarePolygonListAppend(&softwareRenderer->activePolys);
		struct DSGXSoftwareEdge* edge = DSGXSoftwareEdgeListAppend(&softwareRenderer->activeEdges);
		poly->poly = &polys[i];
		edge->polyId = i;

		struct DSGXVertex* v0 = &verts[poly->poly->vertIds[0]];
		struct DSGXVertex* v1;

		int v;
		for (v = 1; v < poly->poly->verts; ++v) {
			v1 = &verts[poly->poly->vertIds[v]];
			if (v0->vy >= v1->vy) {
				edge->y0 = SCREEN_SIZE - v0->vy;
				edge->x0 = v0->vx;
				edge->w0 = v0->vw;
				_expandColor(v0->color, &edge->cr0, &edge->cg0, &edge->cb0);
				edge->s0 = v0->s;
				edge->t0 = v0->t;

				edge->y1 = SCREEN_SIZE - v1->vy;
				edge->x1 = v1->vx;
				edge->w1 = v1->vw;
				_expandColor(v1->color, &edge->cr1, &edge->cg1, &edge->cb1);
				edge->s1 = v1->s;
				edge->t1 = v1->t;
			} else {
				edge->y0 = SCREEN_SIZE - v1->vy;
				edge->x0 = v1->vx;
				edge->w0 = v1->vw;
				_expandColor(v1->color, &edge->cr0, &edge->cg0, &edge->cb0);
				edge->s0 = v1->s;
				edge->t0 = v1->t;

				edge->y1 = SCREEN_SIZE - v0->vy;
				edge->x1 = v0->vx;
				edge->w1 = v0->vw;
				_expandColor(v0->color, &edge->cr1, &edge->cg1, &edge->cb1);
				edge->s1 = v0->s;
				edge->t1 = v0->t;
			}

			edge = DSGXSoftwareEdgeListAppend(&softwareRenderer->activeEdges);
			edge->polyId = i;
			v0 = v1;
		}

		v1 = &verts[poly->poly->vertIds[0]];
		if (v0->vy >= v1->vy) {
			edge->y0 = SCREEN_SIZE - v0->vy;
			edge->x0 = v0->vx;
			edge->w0 = v0->vw;
			_expandColor(v0->color, &edge->cr0, &edge->cg0, &edge->cb0);
			edge->s0 = v0->s;
			edge->t0 = v0->t;

			edge->y1 = SCREEN_SIZE - v1->vy;
			edge->x1 = v1->vx;
			edge->w1 = v1->vw;
			_expandColor(v1->color, &edge->cr1, &edge->cg1, &edge->cb1);
			edge->s1 = v1->s;
			edge->t1 = v1->t;
		} else {
			edge->y0 = SCREEN_SIZE - v1->vy;
			edge->x0 = v1->vx;
			edge->w0 = v1->vw;
			_expandColor(v1->color, &edge->cr0, &edge->cg0, &edge->cb0);
			edge->s0 = v1->s;
			edge->t0 = v1->t;

			edge->y1 = SCREEN_SIZE - v0->vy;
			edge->x1 = v0->vx;
			edge->w1 = v0->vw;
			_expandColor(v0->color, &edge->cr1, &edge->cg1, &edge->cb1);
			edge->s1 = v0->s;
			edge->t1 = v0->t;
		}
	}
	qsort(DSGXSoftwareEdgeListGetPointer(&softwareRenderer->activeEdges, 0), DSGXSoftwareEdgeListSize(&softwareRenderer->activeEdges), sizeof(struct DSGXSoftwareEdge), _edgeSort);
}

static void DSGXSoftwareRendererDrawScanline(struct DSGXRenderer* renderer, int y) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	DSGXSoftwareSpanListClear(&softwareRenderer->activeSpans);
	memset(softwareRenderer->bucket, 0, sizeof(*softwareRenderer->bucket) * DS_GX_POLYGON_BUFFER_SIZE);
	size_t i;
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
			memset(span, 0, sizeof(*span));
			if (!_edgeToSpan(span, edge, 0, y)) {
				// Horizontal line
				DSGXSoftwareSpanListShift(&softwareRenderer->activeSpans, DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans) - 1, 1);
			} else {
				softwareRenderer->bucket[poly] = span;
			}
		}
	}
	qsort(DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, 0), DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans), sizeof(struct DSGXSoftwareSpan), _spanSort);

	y %= 48;
	color_t* scanline = &softwareRenderer->scanlineCache[DS_VIDEO_HORIZONTAL_PIXELS * y];

	size_t nextSpanX = DS_VIDEO_HORIZONTAL_PIXELS;
	if (DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans)) {
		nextSpanX = DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans) - 1)->ep[0].x;
		nextSpanX >>= 12;
	}
	for (i = 0; i < DS_VIDEO_HORIZONTAL_PIXELS; ++i) {
		struct DSGXSoftwareSpan* span = NULL;
		if (i >= nextSpanX) {
			size_t nextSpanId = DSGXSoftwareSpanListSize(&softwareRenderer->activeSpans);
			span = DSGXSoftwareSpanListGetPointer(&softwareRenderer->activeSpans, nextSpanId - 1);
			while (i > (uint32_t) (span->ep[1].x >> 12)) {
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
			if (i < nextSpanX) {
				span = NULL;
			}
		}
		if (span) {
			scanline[i] = _lerpColor(span, i);
		} else {
			scanline[i] = FLAG_UNWRITTEN; // TODO
		}
	}
}

static void DSGXSoftwareRendererGetScanline(struct DSGXRenderer* renderer, int y, color_t** output) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	y %= 48;
	*output = &softwareRenderer->scanlineCache[DS_VIDEO_HORIZONTAL_PIXELS * y];
}
