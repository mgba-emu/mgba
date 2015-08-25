/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "vfs.h"

#ifdef _3DS
#include "platform/3ds/3ds-vfs.h"
#endif

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
#elif defined(USE_VFS_3DS)
	int ctrFlags = FS_OPEN_READ;
	switch (flags & O_ACCMODE) {
	case O_WRONLY:
		ctrFlags = FS_OPEN_WRITE;
		break;
	case O_RDWR:
		ctrFlags = FS_OPEN_READ | FS_OPEN_WRITE;
		break;
	case O_RDONLY:
		ctrFlags = FS_OPEN_READ;
		break;
	}

	if (flags & O_CREAT) {
		ctrFlags |= FS_OPEN_CREATE;
	}
	struct VFile* vf = VFileOpen3DS(&sdmcArchive, path, ctrFlags);
	if (!vf) {
		return 0;
	}
	if (flags & O_TRUNC) {
		vf->truncate(vf, 0);
	}
	if (flags & O_APPEND) {
		vf->seek(vf, vf->size(vf), SEEK_SET);
	}
	return vf;
#else
	return VFileOpenFD(path, flags);
#endif
}

ssize_t VFileReadline(struct VFile* vf, char* buffer, size_t size) {
	size_t bytesRead = 0;
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

struct VFile* VDirOptionalOpenFile(struct VDir* dir, const char* realPath, const char* prefix, const char* suffix, int mode) {
	char path[PATH_MAX];
	path[PATH_MAX - 1] = '\0';
	struct VFile* vf;
	if (!dir) {
		if (!realPath) {
			return 0;
		}
		char* dotPoint = strrchr(realPath, '.');
		if (dotPoint - realPath + 1 >= PATH_MAX - 1) {
			return 0;
		}
		if (dotPoint > strrchr(realPath, '/')) {
			int len = dotPoint - realPath;
			strncpy(path, realPath, len);
			path[len] = 0;
			strncat(path + len, suffix, PATH_MAX - len - 1);
		} else {
			snprintf(path, PATH_MAX - 1, "%s%s", realPath, suffix);
		}
		vf = VFileOpen(path, mode);
	} else {
		snprintf(path, PATH_MAX - 1, "%s%s", prefix, suffix);
		vf = dir->openFile(dir, path, mode);
	}
	return vf;
}
