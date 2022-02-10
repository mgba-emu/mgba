/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_RENDERER_SOFTWARE_H
#define GB_RENDERER_SOFTWARE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/video.h>

struct GBVideoRendererSprite {
	struct GBObj obj;
	int8_t index;
};

struct GBVideoSoftwareRenderer {
	struct GBVideoRenderer d;

	color_t* outputBuffer;
	int outputBufferStride;

	// TODO: Implement the pixel FIFO
	uint16_t row[GB_VIDEO_HORIZONTAL_PIXELS + 8];

	color_t palette[192];
	uint8_t lookup[192];

	uint32_t* temporaryBuffer;

	uint8_t scy;
	uint8_t scx;
	uint8_t wy;
	uint8_t wx;
	uint8_t currentWy;
	uint8_t currentWx;
	int lastY;
	int lastX;
	bool hasWindow;

	GBRegisterLCDC lcdc;
	enum GBModel model;

	struct GBVideoRendererSprite obj[GB_VIDEO_MAX_LINE_OBJ];
	int objMax;

	int16_t objOffsetX;
	int16_t objOffsetY;
	int16_t offsetScx;
	int16_t offsetScy;
	int16_t offsetWx;
	int16_t offsetWy;

	int sgbTransfer;
	uint8_t sgbPacket[128];
	uint8_t sgbCommandHeader;
	bool sgbBorders;
	uint32_t sgbBorderMask[18];

	uint8_t lastHighlightAmount;
};

void GBVideoSoftwareRendererCreate(struct GBVideoSoftwareRenderer*);

CXX_GUARD_END

#endif
