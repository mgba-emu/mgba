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

struct DSVideoRenderer {
	void (*init)(struct DSVideoRenderer* renderer);
	void (*reset)(struct DSVideoRenderer* renderer);
	void (*deinit)(struct DSVideoRenderer* renderer);

	uint16_t (*writeVideoRegister)(struct DSVideoRenderer* renderer, uint32_t address, uint16_t value);
	void (*drawScanline)(struct DSVideoRenderer* renderer, int y);
	void (*finishFrame)(struct DSVideoRenderer* renderer);

	void (*getPixels)(struct DSVideoRenderer* renderer, size_t* stride, const void** pixels);
	void (*putPixels)(struct DSVideoRenderer* renderer, size_t stride, const void* pixels);

	uint16_t* vram;
};

struct DS;
struct DSVideo {
	struct DS* p;
	struct DSVideoRenderer* renderer;
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
void DSVideoAssociateRenderer(struct DSVideo* video, struct DSVideoRenderer* renderer);

struct DSCommon;
void DSVideoWriteDISPSTAT(struct DSCommon* dscore, uint16_t value);

struct DSMemory;
void DSVideoConfigureVRAM(struct DSMemory* memory, int index, uint8_t value);

CXX_GUARD_START

#endif
