/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/patch/ips.h>

#include <mgba-util/patch.h>
#include <mgba-util/vfs.h>

static size_t _IPSOutputSize(struct Patch* patch, size_t inSize);
static bool _IPSApplyPatch(struct Patch* patch, const void* in, size_t inSize, void* out, size_t outSize);

bool loadPatchIPS(struct Patch* patch) {
	patch->vf->seek(patch->vf, 0, SEEK_SET);

	char buffer[5];
	if (patch->vf->read(patch->vf, buffer, 5) != 5) {
		return false;
	}

	if (memcmp(buffer, "PATCH", 5) != 0) {
		return false;
	}

	patch->vf->seek(patch->vf, -3, SEEK_END);
	if (patch->vf->read(patch->vf, buffer, 3) != 3) {
		return false;
	}

	if (memcmp(buffer, "EOF", 3) != 0) {
		return false;
	}

	patch->outputSize = _IPSOutputSize;
	patch->applyPatch = _IPSApplyPatch;
	return true;
}

size_t _IPSOutputSize(struct Patch* patch, size_t inSize) {
	UNUSED(patch);
	UNUSED(inSize);
	return 16 * 1024 * 1024; // IPS patches can grow up to 16MiB, but not beyond
}

bool _IPSApplyPatch(struct Patch* patch, const void* in, size_t inSize, void* out, size_t outSize) {
	if (patch->vf->seek(patch->vf, 5, SEEK_SET) != 5) {
		return false;
	}
	memcpy(out, in, inSize > outSize ? outSize : inSize);
	uint8_t* buf = out;

	while (true) {
		uint32_t offset = 0;
		uint16_t size = 0;

		if (patch->vf->read(patch->vf, &offset, 3) != 3) {
			return false;
		}

		if (offset == 0x464F45) {
			return true;
		}

		offset = (offset >> 16) | (offset & 0xFF00) | ((offset << 16) & 0xFF0000);
		if (patch->vf->read(patch->vf, &size, 2) != 2) {
			return false;
		}
		if (!size) {
			// RLE chunk
			if (patch->vf->read(patch->vf, &size, 2) != 2) {
				return false;
			}
			size = (size >> 8) | (size << 8);
			uint8_t byte;
			if (patch->vf->read(patch->vf, &byte, 1) != 1) {
				return false;
			}
			if (offset + size > outSize) {
				return false;
			}
			memset(&buf[offset], byte, size);
		} else {
			size = (size >> 8) | (size << 8);
			if (offset + size > outSize) {
				return false;
			}
			if (patch->vf->read(patch->vf, &buf[offset], size) != size) {
				return false;
			}
		}
	}
}
