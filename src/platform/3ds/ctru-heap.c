/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <3ds/archive.h>

#include <mgba-util/common.h>

uint32_t* romBuffer = NULL;
size_t romBufferSize;

FS_Archive sdmcArchive;

void userAppInit(void) {
	FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""));

	romBuffer = malloc(0x02000000);
	if (romBuffer) {
		romBufferSize = 0x02000000;
		return;
	}
	romBuffer = malloc(0x01000000);
	if (romBuffer) {
		romBufferSize = 0x01000000;
		return;
	}
}
