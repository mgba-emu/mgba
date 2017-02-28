/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/gx/software.h>

#include <mgba-util/memory.h>

DEFINE_VECTOR(DSGXSoftwarePolygonList, struct DSGXSoftwarePolygon);
DEFINE_VECTOR(DSGXSoftwareEdgeList, struct DSGXSoftwareEdge);

static void DSGXSoftwareRendererInit(struct DSGXRenderer* renderer);
static void DSGXSoftwareRendererReset(struct DSGXRenderer* renderer);
static void DSGXSoftwareRendererDeinit(struct DSGXRenderer* renderer);
static void DSGXSoftwareRendererSetRAM(struct DSGXRenderer* renderer, struct DSGXVertex* verts, struct DSGXPolygon* polys, unsigned polyCount);
static void DSGXSoftwareRendererDrawScanline(struct DSGXRenderer* renderer, int y);
static void DSGXSoftwareRendererGetScanline(struct DSGXRenderer* renderer, int y, color_t** output);

static int _edgeSort(const void* a, const void* b) {
	const struct DSGXSoftwareEdge* ea = a;
	const struct DSGXSoftwareEdge* eb = b;

	if (ea->y0 < eb->y0) {
		return -1;
	}
	if (ea->y0 > eb->y0) {
		return 1;
	}
	if (ea->y1 < eb->y1) {
		return -1;
	}
	if (ea->y1 > eb->y1) {
		return 1;
	}
	return 0;
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

		struct DSGXVertex* v0 = &verts[poly->poly->vertIds[0]];
		struct DSGXVertex* v1;

		int v;
		for (v = 1; v < poly->poly->verts; ++v) {
			v1 = &verts[poly->poly->vertIds[v]];
			if (v0->vy <= v1->vy) {
				edge->y0 = v0->vy;
				edge->x0 = v0->vx;
				edge->w0 = v0->vw;
				edge->c0 = v0->color;
				edge->s0 = v0->s;
				edge->t0 = v0->t;

				edge->y1 = v1->vy;
				edge->x1 = v1->vx;
				edge->w1 = v1->vw;
				edge->c1 = v1->color;
				edge->s1 = v1->s;
				edge->t1 = v1->t;
			} else {
				edge->y0 = v1->vy;
				edge->x0 = v1->vx;
				edge->w0 = v1->vw;
				edge->c0 = v1->color;
				edge->s0 = v1->s;
				edge->t0 = v1->t;

				edge->y1 = v0->vy;
				edge->x1 = v0->vx;
				edge->w1 = v0->vw;
				edge->c1 = v0->color;
				edge->s1 = v0->s;
				edge->t1 = v0->t;
			}

			edge = DSGXSoftwareEdgeListAppend(&softwareRenderer->activeEdges);
			v0 = v1;
		}

		v1 = &verts[poly->poly->vertIds[0]];
		if (v0->vy <= v1->vy) {
			edge->y0 = v0->vy;
			edge->x0 = v0->vx;
			edge->w0 = v0->vw;
			edge->c0 = v0->color;
			edge->s0 = v0->s;
			edge->t0 = v0->t;

			edge->y1 = v1->vy;
			edge->x1 = v1->vx;
			edge->w1 = v1->vw;
			edge->c1 = v1->color;
			edge->s1 = v1->s;
			edge->t1 = v1->t;
		} else {
			edge->y0 = v1->vy;
			edge->x0 = v1->vx;
			edge->w0 = v1->vw;
			edge->c0 = v1->color;
			edge->s0 = v1->s;
			edge->t0 = v1->t;

			edge->y1 = v0->vy;
			edge->x1 = v0->vx;
			edge->w1 = v0->vw;
			edge->c1 = v0->color;
			edge->s1 = v0->s;
			edge->t1 = v0->t;
		}
	}
	qsort(DSGXSoftwareEdgeListGetPointer(&softwareRenderer->activeEdges, 0), DSGXSoftwareEdgeListSize(&softwareRenderer->activeEdges), sizeof(struct DSGXSoftwareEdge), _edgeSort);
}

static void DSGXSoftwareRendererDrawScanline(struct DSGXRenderer* renderer, int y) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	// TODO
}

static void DSGXSoftwareRendererGetScanline(struct DSGXRenderer* renderer, int y, color_t** output) {
	struct DSGXSoftwareRenderer* softwareRenderer = (struct DSGXSoftwareRenderer*) renderer;
	y %= 48;
	*output = &softwareRenderer->scanlineCache[DS_VIDEO_HORIZONTAL_PIXELS * y];
}
