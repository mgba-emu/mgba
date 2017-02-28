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
#include <mgba-util/vector.h>

struct DSGXSoftwarePolygon {
	struct DSGXPolygon* poly;
	int32_t topY;
	int32_t bottomY;
	int32_t topZ;
};

struct DSGXSoftwareEdge {
	struct DSGXPolygon* poly;
	int32_t y0;
	int32_t x0;
	int32_t w0;
	int32_t c0; // 6.6.6.6 ARGB
	int16_t s0;
	int16_t t0;

	int32_t y1;
	int32_t x1;
	int32_t w1;
	int32_t c1; // 6.6.6.6 ARGB
	int16_t s1;
	int16_t t1;
};

DECLARE_VECTOR(DSGXSoftwarePolygonList, struct DSGXSoftwarePolygon);
DECLARE_VECTOR(DSGXSoftwareEdgeList, struct DSGXSoftwareEdge);

struct DSGXSoftwareRenderer {
	struct DSGXRenderer d;

	struct DSGXSoftwarePolygonList activePolys;
	struct DSGXSoftwareEdgeList activeEdges;

	uint16_t depthBuffer[DS_VIDEO_HORIZONTAL_PIXELS];
	color_t* scanlineCache;

	struct DSGXVertex* verts;
};

CXX_GUARD_END

#endif
