/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#include <fcntl.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/mman.h>
#include <sys/time.h>
#else
#include <windows.h>
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
static ssize_t _vfdWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vfdMap(struct VFile* vf, size_t size, int flags);
static void _vfdUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfdTruncate(struct VFile* vf, size_t size);
static ssize_t _vfdSize(struct VFile* vf);
static bool _vfdSync(struct VFile* vf, const void* buffer, size_t size);

struct VFile* VFileOpenFD(const char* path, int flags) {
	if (!path) {
		return 0;
	}
#ifdef _WIN32
	flags |= O_BINARY;
	wchar_t wpath[PATH_MAX];
	MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, sizeof(wpath) / sizeof(*wpath));
	int fd = _wopen(wpath, flags, _S_IREAD | _S_IWRITE);
#else
	int fd = open(path, flags, 0666);
#endif
	return VFileFromFD(fd);
}

struct VFile* VFileFromFD(int fd) {
	if (fd < 0) {
		return 0;
	}

	struct stat stat;
	if (fstat(fd, &stat) < 0 || (stat.st_mode & S_IFDIR)) {
		close(fd);
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
	vfd->d.readline = VFileReadline;
	vfd->d.write = _vfdWrite;
	vfd->d.map = _vfdMap;
	vfd->d.unmap = _vfdUnmap;
	vfd->d.truncate = _vfdTruncate;
	vfd->d.size = _vfdSize;
	vfd->d.sync = _vfdSync;

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
	size_t fileSize;
	struct stat stat;
	if (fstat(vfd->fd, &stat) < 0) {
		return 0;
	}
	fileSize = stat.st_size;
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

static ssize_t _vfdSize(struct VFile* vf) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	struct stat stat;
	if (fstat(vfd->fd, &stat) < 0) {
		return -1;
	}
	return stat.st_size;
}

static bool _vfdSync(struct VFile* vf, const void* buffer, size_t size) {
	UNUSED(buffer);
	UNUSED(size);
	struct VFileFD* vfd = (struct VFileFD*) vf;
#ifndef _WIN32
#ifdef __HAIKU__
	futimens(vfd->fd, NULL);
#else
	futimes(vfd->fd, NULL);
#endif
	if (buffer && size) {
		return msync(buffer, size, MS_SYNC) == 0;
	}
	return fsync(vfd->fd) == 0;
#else
	HANDLE h = (HANDLE) _get_osfhandle(vfd->fd);
	FILETIME ft;
	SYSTEMTIME st;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	SetFileTime(h, NULL, &ft, &ft);
	return FlushFileBuffers(h);
#endif
}
