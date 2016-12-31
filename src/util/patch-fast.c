/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/patch/fast.h>

DEFINE_VECTOR(PatchFastExtents, struct PatchFastExtent);

size_t _fastOutputSize(struct Patch* patch, size_t inSize);
bool _fastApplyPatch(struct Patch* patch, const void* in, size_t inSize, void* out, size_t outSize);

void initPatchFast(struct PatchFast* patch) {
	PatchFastExtentsInit(&patch->extents, 32);
	patch->d.outputSize = _fastOutputSize;
	patch->d.applyPatch = _fastApplyPatch;
}

void deinitPatchFast(struct PatchFast* patch) {
	PatchFastExtentsDeinit(&patch->extents);
}

bool diffPatchFast(struct PatchFast* patch, const void* restrict in, const void* restrict out, size_t size) {
	PatchFastExtentsClear(&patch->extents);
	const uint32_t* iptr = in;
	const uint32_t* optr = out;
	size_t extentOff = 0;
	struct PatchFastExtent* extent = NULL;
	size_t off;
	for (off = 0; off < (size & ~15); off += 16) {
		uint32_t a = iptr[0] ^ optr[0];
		uint32_t b = iptr[1] ^ optr[1];
		uint32_t c = iptr[2] ^ optr[2];
		uint32_t d = iptr[3] ^ optr[3];
		iptr += 4;
		optr += 4;
		if (a | b | c | d) {
			if (!extent) {
				extent = PatchFastExtentsAppend(&patch->extents);
				extent->offset = off;
				extentOff = 0;
			}
			extent->extent[extentOff] = a;
			extent->extent[extentOff + 1] = b;
			extent->extent[extentOff + 2] = c;
			extent->extent[extentOff + 3] = d;
			extentOff += 4;
			if (extentOff == PATCH_FAST_EXTENT) {
				extent->length = extentOff * 4;
				extent = NULL;
			}
		} else if (extent) {
			extent->length = extentOff * 4;
			extent = NULL;
		}
	}
	if (extent) {
		extent->length = extentOff * 4;
		extent = NULL;
	}
	const uint32_t* iptr8 = iptr;
	const uint32_t* optr8 = optr;
	for (; off < size; ++off) {
		uint8_t a = iptr8[0] ^ optr8[0];
		++iptr8;
		++optr8;
		if (a) {
			if (!extent) {
				extent = PatchFastExtentsAppend(&patch->extents);
				extent->offset = off;
			}
			((uint8_t*) extent->extent)[extentOff] = a;
			++extentOff;
		} else if (extent) {
			extent->length = extentOff;
			extent = NULL;
		}
	}
	if (extent) {
		extent->length = extentOff;
		extent = NULL;
	}

	return true;
}

size_t _fastOutputSize(struct Patch* patch, size_t inSize) {
	UNUSED(patch);
	return inSize;
}

bool _fastApplyPatch(struct Patch* p, const void* in, size_t inSize, void* out, size_t outSize) {
	struct PatchFast* patch = (struct PatchFast*) p;
	if (inSize != outSize) {
		return false;
	}
	const uint32_t* iptr = in;
	uint32_t* optr = out;
	size_t lastWritten = 0;
	size_t s;
	for (s = 0; s < PatchFastExtentsSize(&patch->extents); ++s) {
		struct PatchFastExtent* extent = PatchFastExtentsGetPointer(&patch->extents, s);
		if (extent->length + extent->offset > outSize) {
			return false;
		}
		memcpy(optr, iptr, extent->offset - lastWritten);
		optr = (uint32_t*) out + extent->offset / 4;
		iptr = (uint32_t*) in + extent->offset / 4;
		uint32_t* eptr = extent->extent;
		size_t off;
		for (off = 0; off < (extent->length & ~15); off += 16) {
			optr[0] = iptr[0] ^ eptr[0];
			optr[1] = iptr[1] ^ eptr[1];
			optr[2] = iptr[2] ^ eptr[2];
			optr[3] = iptr[3] ^ eptr[3];
			optr += 4;
			iptr += 4;
			eptr += 4;
		}
		for (; off < extent->length; ++off) {
			*(uint8_t*) optr = *(uint8_t*) iptr ^ *(uint8_t*) eptr;
			++optr;
			++iptr;
			++eptr;
		}
		lastWritten = extent->offset + off;
	}
	memcpy(optr, iptr, outSize - lastWritten);
	return true;
}
