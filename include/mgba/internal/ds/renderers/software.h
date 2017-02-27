/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_VIDEO_SOFTWARE_H
#define DS_VIDEO_SOFTWARE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/ds/video.h>
#include <mgba/internal/gba/renderers/video-software.h>

struct DSVideoSoftwareRenderer {
	struct DSVideoRenderer d;

	struct GBAVideoSoftwareRenderer engA;
	struct GBAVideoSoftwareRenderer engB;

	DSRegisterDISPCNT dispcntA;
	DSRegisterDISPCNT dispcntB;
	DSRegisterPOWCNT1 powcnt;

	color_t* outputBuffer;
	int outputBufferStride;

	uint32_t row[DS_VIDEO_HORIZONTAL_PIXELS];

	color_t extPaletteA[16384];
	color_t extPaletteB[16384];
	color_t variantPaletteA[16384];
	color_t variantPaletteB[16384];
	color_t objExtPaletteA[4096];
	color_t objExtPaletteB[4096];
	color_t objVariantPaletteA[4096];
	color_t objVariantPaletteB[4096];
};

void DSVideoSoftwareRendererCreate(struct DSVideoSoftwareRenderer* renderer);

CXX_GUARD_END

#endif
