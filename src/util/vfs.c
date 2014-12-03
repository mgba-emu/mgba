/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/vfs.h"

#include "util/string.h"

#include <fcntl.h>
#include <dirent.h>

#ifndef _WIN32
#include <sys/mman.h>
#define PATH_SEP '/'
#else
#include <io.h>
#include <windows.h>
#define PATH_SEP '\\'
#endif

struct VFileFD {
	struct VFile d;
	int fd;
#ifdef _WIN32
	HANDLE hMap;
#endif
};

static bool _vfdClose(struct VFile* vf);
static off_t _vfdSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vfdRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vfdReadline(struct VFile* vf, char* buffer, size_t size);
static ssize_t _vfdWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vfdMap(struct VFile* vf, size_t size, int flags);
static void _vfdUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfdTruncate(struct VFile* vf, size_t size);

static bool _vdClose(struct VDir* vd);
static void _vdRewind(struct VDir* vd);
static struct VDirEntry* _vdListNext(struct VDir* vd);
static struct VFile* _vdOpenFile(struct VDir* vd, const char* path, int mode);

static const char* _vdeName(struct VDirEntry* vde);

struct VFile* VFileOpen(const char* path, int flags) {
	if (!path) {
		return 0;
	}
#ifdef _WIN32
	flags |= O_BINARY;
#endif
	int fd = open(path, flags, 0666);
	return VFileFromFD(fd);
}

struct VFile* VFileFromFD(int fd) {
	if (fd < 0) {
		return 0;
	}

	struct VFileFD* vfd = malloc(sizeof(struct VFileFD));
	if (!vfd) {
		return 0;
	}

	vfd->fd = fd;
	vfd->d.close = _vfdClose;
	vfd->d.seek = _vfdSeek;
	vfd->d.read = _vfdRead;
	vfd->d.readline = _vfdReadline;
	vfd->d.write = _vfdWrite;
	vfd->d.map = _vfdMap;
	vfd->d.unmap = _vfdUnmap;
	vfd->d.truncate = _vfdTruncate;

	return &vfd->d;
}

bool _vfdClose(struct VFile* vf) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	if (close(vfd->fd) < 0) {
		return false;
	}
	free(vfd);
	return true;
}

off_t _vfdSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	return lseek(vfd->fd, offset, whence);
}

ssize_t _vfdRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	return read(vfd->fd, buffer, size);
}

ssize_t _vfdReadline(struct VFile* vf, char* buffer, size_t size) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	size_t bytesRead = 0;
	while (bytesRead < size - 1) {
		size_t newRead = read(vfd->fd, &buffer[bytesRead], 1);
		bytesRead += newRead;
		if (!newRead || buffer[bytesRead] == '\n') {
			break;
		}
	}
	return buffer[bytesRead] = '\0';
}

ssize_t _vfdWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	return write(vfd->fd, buffer, size);
}

#ifndef _WIN32
static void* _vfdMap(struct VFile* vf, size_t size, int flags) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	int mmapFlags = MAP_PRIVATE;
	if (flags & MAP_WRITE) {
		mmapFlags = MAP_SHARED;
	}
	return mmap(0, size, PROT_READ | PROT_WRITE, mmapFlags, vfd->fd, 0);
}

static void _vfdUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	munmap(memory, size);
}
#else
static void* _vfdMap(struct VFile* vf, size_t size, int flags) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	int createFlags = PAGE_WRITECOPY;
	int mapFiles = FILE_MAP_COPY;
	if (flags & MAP_WRITE) {
		createFlags = PAGE_READWRITE;
		mapFiles = FILE_MAP_WRITE;
	}
	size_t location = lseek(vfd->fd, 0, SEEK_CUR);
	size_t fileSize = lseek(vfd->fd, 0, SEEK_END);
	lseek(vfd->fd, location, SEEK_SET);
	if (size > fileSize) {
		size = fileSize;
	}
	vfd->hMap = CreateFileMapping((HANDLE) _get_osfhandle(vfd->fd), 0, createFlags, 0, size & 0xFFFFFFFF, 0);
	return MapViewOfFile(vfd->hMap, mapFiles, 0, 0, size);
}

static void _vfdUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(size);
	struct VFileFD* vfd = (struct VFileFD*) vf;
	UnmapViewOfFile(memory);
	CloseHandle(vfd->hMap);
	vfd->hMap = 0;
}
#endif

static void _vfdTruncate(struct VFile* vf, size_t size) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	ftruncate(vfd->fd, size);
}

struct VDirEntryDE {
	struct VDirEntry d;
	struct dirent* ent;
};

struct VDirDE {
	struct VDir d;
	DIR* de;
	struct VDirEntryDE vde;
	char* path;
};

struct VDir* VDirOpen(const char* path) {
	DIR* de = opendir(path);
	if (!de) {
		return 0;
	}

	struct VDirDE* vd = malloc(sizeof(struct VDirDE));
	if (!vd) {
		return 0;
	}

	vd->d.close = _vdClose;
	vd->d.rewind = _vdRewind;
	vd->d.listNext = _vdListNext;
	vd->d.openFile = _vdOpenFile;
	vd->path = strdup(path);
	vd->de = de;

	vd->vde.d.name = _vdeName;

	return &vd->d;
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

struct VFile* VDirOptionalOpenIncrementFile(struct VDir* dir, const char* realPath, const char* prefix, const char* infix, const char* suffix, int mode) {
	char path[PATH_MAX];
	path[PATH_MAX - 1] = '\0';
	char realPrefix[PATH_MAX];
	realPrefix[PATH_MAX - 1] = '\0';
	if (!dir) {
		if (!realPath) {
			return 0;
		}
		const char* separatorPoint = strrchr(realPath, '/');
		const char* dotPoint;
		size_t len;
		if (!separatorPoint) {
			strcpy(path, "./");
			separatorPoint = realPath;
			dotPoint = strrchr(realPath, '.');
		} else {
			path[0] = '\0';
			dotPoint = strrchr(separatorPoint, '.');

			if (separatorPoint - realPath + 1 >= PATH_MAX - 1) {
				return 0;
			}

			len = separatorPoint - realPath;
			strncat(path, realPath, len);
			path[len] = '\0';
			++separatorPoint;
		}

		if (dotPoint - realPath + 1 >= PATH_MAX - 1) {
			return 0;
		}

		if (dotPoint >= separatorPoint) {
			len = dotPoint - separatorPoint;
		} else {
			len = PATH_MAX - 1;
		}

		strncpy(realPrefix, separatorPoint, len);
		realPrefix[len] = '\0';

		prefix = realPrefix;
		dir = VDirOpen(path);
	}
	if (!dir) {
		// This shouldn't be possible
		return 0;
	}
	dir->rewind(dir);
	struct VDirEntry* dirent;
	size_t prefixLen = strlen(prefix);
	size_t infixLen = strlen(infix);
	unsigned next = 0;
	while ((dirent = dir->listNext(dir))) {
		const char* filename = dirent->name(dirent);
		char* dotPoint = strrchr(filename, '.');
		size_t len = strlen(filename);
		if (dotPoint) {
			len = (dotPoint - filename);
		}
		const char* separator = strnrstr(filename, infix, len);
		if (!separator) {
			continue;
		}
		len = separator - filename;
		if (len != prefixLen) {
			continue;
		}
		if (strncmp(filename, prefix, prefixLen) == 0) {
			int nlen;
			separator += infixLen;
			snprintf(path, PATH_MAX - 1, "%%u%s%%n", suffix);
			unsigned increment;
			if (sscanf(separator, path, &increment, &nlen) < 1) {
				continue;
			}
			len = strlen(separator);
			if (nlen < (ssize_t) len) {
				continue;
			}
			if (next <= increment) {
				next = increment + 1;
			}
		}
	}
	snprintf(path, PATH_MAX - 1, "%s%s%u%s", prefix, infix, next, suffix);
	path[PATH_MAX - 1] = '\0';
	return dir->openFile(dir, path, mode);
}

bool _vdClose(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	if (closedir(vdde->de) < 0) {
		return false;
	}
	free(vdde->path);
	free(vdde);
	return true;
}

void _vdRewind(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	rewinddir(vdde->de);
}

struct VDirEntry* _vdListNext(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	vdde->vde.ent = readdir(vdde->de);
	if (vdde->vde.ent) {
		return &vdde->vde.d;
	}

	return 0;
}

struct VFile* _vdOpenFile(struct VDir* vd, const char* path, int mode) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdde->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	sprintf(combined, "%s%c%s", dir, PATH_SEP, path);

	struct VFile* file = VFileOpen(combined, mode);
	free(combined);
	return file;
}

const char* _vdeName(struct VDirEntry* vde) {
	struct VDirEntryDE* vdede = (struct VDirEntryDE*) vde;
	if (vdede->ent) {
		return vdede->ent->d_name;
	}
	return 0;
}
