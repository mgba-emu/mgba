/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_VIDEO_H
#define DS_VIDEO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>

mLOG_DECLARE_CATEGORY(DS_VIDEO);

enum {
	DS_VIDEO_HORIZONTAL_PIXELS = 256,
	DS_VIDEO_HBLANK_PIXELS = 99,
	DS7_VIDEO_HBLANK_LENGTH = 1613,
	DS9_VIDEO_HBLANK_LENGTH = 1606,
	DS_VIDEO_HORIZONTAL_LENGTH = (DS_VIDEO_HORIZONTAL_PIXELS + DS_VIDEO_HBLANK_PIXELS) * 6,

	DS_VIDEO_VERTICAL_PIXELS = 192,
	DS_VIDEO_VBLANK_PIXELS = 71,
	DS_VIDEO_VERTICAL_TOTAL_PIXELS = DS_VIDEO_VERTICAL_PIXELS + DS_VIDEO_VBLANK_PIXELS,

	DS_VIDEO_TOTAL_LENGTH = DS_VIDEO_HORIZONTAL_LENGTH * DS_VIDEO_VERTICAL_TOTAL_PIXELS,
};

struct DS;
struct DSVideo {
	struct DS* p;
	struct mTimingEvent event7;
	struct mTimingEvent event9;

	// VCOUNT
	int vcount;

	uint16_t* vram;

	int32_t frameCounter;
	int frameskip;
	int frameskipCounter;
};

void DSVideoInit(struct DSVideo* video);
void DSVideoReset(struct DSVideo* video);
void DSVideoDeinit(struct DSVideo* video);

struct DSCommon;
void DSVideoWriteDISPSTAT(struct DSCommon* dscore, uint16_t value);

CXX_GUARD_START

#endif
