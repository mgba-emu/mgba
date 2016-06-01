/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_VIDEO_H
#define DS_VIDEO_H

#include "util/common.h"

#include "core/log.h"

mLOG_DECLARE_CATEGORY(DS_VIDEO);

enum {
	DS_VIDEO_HORIZONTAL_PIXELS = 256,
	DS_VIDEO_HBLANK_PIXELS = 99,
	DS_VIDEO_HORIZONTAL_LENGTH = (DS_VIDEO_HORIZONTAL_PIXELS + DS_VIDEO_HBLANK_PIXELS) * 6,

	DS_VIDEO_VERTICAL_PIXELS = 192,
	DS_VIDEO_VBLANK_PIXELS = 71,
	DS_VIDEO_VERTICAL_TOTAL_PIXELS = DS_VIDEO_VERTICAL_PIXELS + DS_VIDEO_VBLANK_PIXELS,

	DS_VIDEO_TOTAL_LENGTH = DS_VIDEO_HORIZONTAL_LENGTH * DS_VIDEO_VERTICAL_TOTAL_PIXELS,
};

struct DS;
struct DSVideo {
	struct DS* p;

	// VCOUNT
	int vcount;

	int32_t nextHblank;
	int32_t nextEvent;
	int32_t eventDiff;

	int32_t nextHblankIRQ;
	int32_t nextVblankIRQ;
	int32_t nextVcounterIRQ;

	int32_t frameCounter;
	int frameskip;
	int frameskipCounter;
};

void DSVideoInit(struct DSVideo* video);
void DSVideoReset(struct DSVideo* video);
void DSVideoDeinit(struct DSVideo* video);

#endif
