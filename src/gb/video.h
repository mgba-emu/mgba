/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_VIDEO_H
#define GB_VIDEO_H

#include "util/common.h"

#include "gb/memory.h"

enum {
	GB_VIDEO_HORIZONTAL_PIXELS = 160,
	GB_VIDEO_VERTICAL_PIXELS = 144,
	GB_VIDEO_VBLANK_PIXELS = 10,
	GB_VIDEO_VERTICAL_TOTAL_PIXELS = GB_VIDEO_VERTICAL_PIXELS + GB_VIDEO_VBLANK_PIXELS,

	GB_VIDEO_MODE_0_LENGTH = 203, // Estimates, figure out with more precision
	GB_VIDEO_MODE_2_LENGTH = 81,
	GB_VIDEO_MODE_3_LENGTH = 172,

	GB_VIDEO_HORIZONTAL_LENGTH = GB_VIDEO_MODE_0_LENGTH + GB_VIDEO_MODE_2_LENGTH + GB_VIDEO_MODE_3_LENGTH,

	GB_VIDEO_MODE_1_LENGTH = GB_VIDEO_HORIZONTAL_LENGTH * GB_VIDEO_VBLANK_PIXELS,
	GB_VIDEO_TOTAL_LENGTH = GB_VIDEO_HORIZONTAL_LENGTH * GB_VIDEO_VERTICAL_TOTAL_PIXELS,
};

struct GBVideoRenderer {
	void (*init)(struct GBVideoRenderer* renderer);
	void (*reset)(struct GBVideoRenderer* renderer);
	void (*deinit)(struct GBVideoRenderer* renderer);

	uint8_t (*writeVideoRegister)(struct GBVideoRenderer* renderer, uint16_t address, uint8_t value);
	void (*writeVRAM)(struct GBVideoRenderer* renderer, uint16_t address);
	void (*drawScanline)(struct GBVideoRenderer* renderer, int y);
	void (*finishFrame)(struct GBVideoRenderer* renderer);

	void (*getPixels)(struct GBVideoRenderer* renderer, unsigned* stride, const void** pixels);
	void (*putPixels)(struct GBVideoRenderer* renderer, unsigned stride, void* pixels);

	uint8_t* vram;
};

DECL_BITFIELD(GBRegisterLCDC, uint8_t);
DECL_BIT(GBRegisterLCDC, Enable, 7);

DECL_BITFIELD(GBRegisterSTAT, uint8_t);

struct GBVideo {
	struct GB* p;
	struct GBVideoRenderer* renderer;

	int ly;
	int mode;

	int32_t nextEvent;
	int32_t eventDiff;

	int32_t nextMode;

	uint8_t* vram;

	int32_t frameCounter;
	int frameskip;
	int frameskipCounter;
};

void GBVideoInit(struct GBVideo* video);
void GBVideoReset(struct GBVideo* video);
void GBVideoDeinit(struct GBVideo* video);
void GBVideoAssociateRenderer(struct GBVideo* video, struct GBVideoRenderer* renderer);
int32_t GBVideoProcessEvents(struct GBVideo* video, int32_t cycles);

void GBVideoWriteLCDC(struct GBVideo* video, GBRegisterLCDC value);
void GBVideoWriteSTAT(struct GBVideo* video, GBRegisterSTAT value);

#endif
