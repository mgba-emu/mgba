/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef PATCH_FAST_H
#define PATCH_FAST_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/patch.h>
#include <mgba-util/vector.h>

#define PATCH_FAST_EXTENT 128

struct PatchFastExtent {
	size_t length;
	size_t offset;
	uint32_t extent[PATCH_FAST_EXTENT];
};

DECLARE_VECTOR(PatchFastExtents, struct PatchFastExtent);

struct PatchFast {
	struct Patch d;

	struct PatchFastExtents extents;
};

void initPatchFast(struct PatchFast*);
void deinitPatchFast(struct PatchFast*);
bool diffPatchFast(struct PatchFast* patch, const void* restrict in, const void* restrict out, size_t size);

CXX_GUARD_END

#endif
