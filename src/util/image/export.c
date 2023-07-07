/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/image/export.h>

#include <mgba/core/interface.h>
#include <mgba-util/vfs.h>

bool mPaletteExportRIFF(struct VFile* vf, size_t entries, const uint16_t* colors) {
	if (entries > 0xFFFF) {
		return false;
	}
	uint32_t chunkSize = 4 + 4 * entries;
	uint32_t size = chunkSize + 12;

	// Header
	if (vf->write(vf, "RIFF", 4) < 4) {
		return false;
	}
	if (VFileWrite32LE(vf, size) < 4) {
		return false;
	}
	if (vf->write(vf, "PAL ", 4) < 4) {
		return false;
	}

	// Data chunk
	if (vf->write(vf, "data", 4) < 4) {
		return false;
	}
	if (VFileWrite32LE(vf, chunkSize) < 4) {
		return false;
	}
	if (VFileWrite16LE(vf, 0x0300) < 2) {
		return false;
	}
	if (VFileWrite16LE(vf, entries) < 2) {
		return false;
	}

	size_t i;
	for (i = 0; i < entries; ++i) {
		uint8_t block[4] = {
			M_R8(colors[i]),
			M_G8(colors[i]),
			M_B8(colors[i]),
			0
		};
		if (vf->write(vf, block, 4) < 4) {
			return false;
		}
	}

	return true;
}

bool mPaletteExportACT(struct VFile* vf, size_t entries, const uint16_t* colors) {
	if (entries > 256) {
		return false;
	}
	size_t i;
	for (i = 0; i < entries; ++i) {
		uint8_t block[3] = {
			M_R8(colors[i]),
			M_G8(colors[i]),
			M_B8(colors[i]),
		};
		if (vf->write(vf, block, 3) < 3) {
			return false;
		}
	}
	for (; i < 256; ++i) {
		uint8_t block[3] = { 0, 0, 0 };
		if (vf->write(vf, block, 3) < 3) {
			return false;
		}
	}
	return true;
}
