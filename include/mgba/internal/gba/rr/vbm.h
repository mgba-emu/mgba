/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_VBM_H
#define GBA_VBM_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gba/rr/rr.h>

struct GBAVBMContext {
	struct GBARRContext d;

	bool isPlaying;

	struct VFile* vbmFile;
	int32_t inputOffset;
};

void GBAVBMContextCreate(struct GBAVBMContext*);

bool GBAVBMSetStream(struct GBAVBMContext*, struct VFile*);

CXX_GUARD_END

#endif
