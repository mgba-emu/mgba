/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "vfs.h"

struct VFile* VFileOpen(const char* path, int flags) {
#ifdef USE_VFS_FILE
	const char* chflags;
	switch (flags & O_ACCMODE) {
	case O_WRONLY:
		if (flags & O_APPEND) {
			chflags = "ab";
		} else {
			chflags = "wb";
		}
		break;
	case O_RDWR:
		if (flags & O_APPEND) {
			chflags = "a+b";
		} else if (flags & O_TRUNC) {
			chflags = "w+b";
		} else {
			chflags = "r+b";
		}
		break;
	case O_RDONLY:
		chflags = "rb";
		break;
	}
	return VFileFOpen(path, chflags);
#else
	return VFileOpenFD(path, flags);
#endif
}

ssize_t VFileReadline(struct VFile* vf, char* buffer, size_t size) {
	ssize_t bytesRead = 0;
	while (bytesRead < size - 1) {
		ssize_t newRead = vf->read(vf, &buffer[bytesRead], 1);
		if (newRead <= 0) {
			break;
		}
		bytesRead += newRead;
		if (buffer[bytesRead] == '\n') {
			break;
		}
	}
	buffer[bytesRead] = '\0';
	return bytesRead;
}

ssize_t VFileWrite32LE(struct VFile* vf, int32_t word) {
	uint32_t leword;
	STORE_32LE(word, 0, &leword);
	return vf->write(vf, &leword, 4);
}

ssize_t VFileWrite16LE(struct VFile* vf, int16_t hword) {
	uint16_t lehword;
	STORE_16LE(hword, 0, &lehword);
	return vf->write(vf, &lehword, 2);
}

ssize_t VFileRead32LE(struct VFile* vf, void* word) {
	uint32_t leword;
	ssize_t r = vf->read(vf, &leword, 4);
	if (r == 4) {
		STORE_32LE(leword, 0, word);
	}
	return r;
}

ssize_t VFileRead16LE(struct VFile* vf, void* hword) {
	uint16_t lehword;
	ssize_t r = vf->read(vf, &lehword, 2);
	if (r == 2) {
		STORE_16LE(lehword, 0, hword);
	}
	return r;
}
