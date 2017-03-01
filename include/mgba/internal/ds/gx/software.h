/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_GX_SOFTWARE_H
#define DS_GX_SOFTWARE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/ds/gx.h>
#include <mgba/internal/ds/video.h>
#include <mgba-util/table.h>
#include <mgba-util/vector.h>

struct DSGXSoftwarePolygon {
	struct DSGXPolygon* poly;
	int32_t topY;
	int32_t bottomY;
	int32_t topZ;
};

struct DSGXSoftwareEdge {
	unsigned polyId;
	int32_t y0; // 20.12
	int32_t x0; // 20.12
	int32_t w0; // 20.12
	int8_t cr0;
	int8_t cg0;
	int8_t cb0;
	int16_t s0;
	int16_t t0;

	int32_t y1; // 20.12
	int32_t x1; // 20.12
	int32_t w1; // 20.12
	int8_t cr1;
	int8_t cg1;
	int8_t cb1;
	int16_t s1;
	int16_t t1;
};

struct DSGXSoftwareSpan {
	int32_t x0; // 20.12
	int32_t w0; // 20.12
	int8_t cr0;
	int8_t cg0;
	int8_t cb0;
	int16_t s0;
	int16_t t0;

	int32_t x1; // 20.12
	int32_t w1; // 20.12
	int8_t cr1;
	int8_t cg1;
	int8_t cb1;
	int16_t s1;
	int16_t t1;
};

DECLARE_VECTOR(DSGXSoftwarePolygonList, struct DSGXSoftwarePolygon);
DECLARE_VECTOR(DSGXSoftwareEdgeList, struct DSGXSoftwareEdge);
DECLARE_VECTOR(DSGXSoftwareSpanList, struct DSGXSoftwareSpan);

struct DSGXSoftwareRenderer {
	struct DSGXRenderer d;

	struct DSGXSoftwarePolygonList activePolys;
	struct DSGXSoftwareEdgeList activeEdges;
	struct DSGXSoftwareSpanList activeSpans;
	struct DSGXSoftwareSpan** bucket;

	uint16_t depthBuffer[DS_VIDEO_HORIZONTAL_PIXELS];
	color_t* scanlineCache;

	struct DSGXVertex* verts;
};

CXX_GUARD_END

#endif
