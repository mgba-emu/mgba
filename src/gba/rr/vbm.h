/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/common.h"

#include "gba/supervisor/rr.h"

struct GBAVBMContext {
	struct GBARRContext d;

	bool isPlaying;

	struct VFile* vbmFile;
	int32_t inputOffset;
};

void GBAVBMContextCreate(struct GBAVBMContext*);

bool GBAVBMSetStream(struct GBAVBMContext*, struct VFile*);
