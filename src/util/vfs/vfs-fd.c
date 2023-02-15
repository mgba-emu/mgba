/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(_POSIX_MAPPED_FILES)
#include <sys/mman.h>
#include <sys/time.h>
#else
#include <mgba-util/memory.h>
#endif

#include <mgba-util/vector.h>

#ifdef _WIN32
struct HandleMappingTuple {
	HANDLE handle;
	void* mapping;
};

DECLARE_VECTOR(HandleMappingList, struct HandleMappingTuple);
DEFINE_VECTOR(HandleMappingList, struct HandleMappingTuple);
#endif

struct VFileFD {
	struct VFile d;
	int fd;
#ifdef _WIN32
	struct HandleMappingList handles;
#elif !defined(_POSIX_MAPPED_FILES)
	bool writable;
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
static bool _vfdSync(struct VFile* vf, void* buffer, size_t size);

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
#ifdef _WIN32
	HandleMappingListInit(&vfd->handles, 4);
#elif !defined(_POSIX_MAPPED_FILES)
	vfd->writable = false;
#endif

	return &vfd->d;
}

bool _vfdClose(struct VFile* vf) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
#ifdef _WIN32
	size_t i;
	for (i = 0; i < HandleMappingListSize(&vfd->handles); ++i) {
		UnmapViewOfFile(HandleMappingListGetPointer(&vfd->handles, i)->mapping);
		CloseHandle(HandleMappingListGetPointer(&vfd->handles, i)->handle);
	}
	HandleMappingListDeinit(&vfd->handles);
#endif
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

#ifdef _POSIX_MAPPED_FILES
static void* _vfdMap(struct VFile* vf, size_t size, int flags) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	if (!size) {
		return NULL;
	}
	int mmapFlags = MAP_PRIVATE;
	if (flags & MAP_WRITE) {
		mmapFlags = MAP_SHARED;
	}
	void* mapped = mmap(0, size, PROT_READ | PROT_WRITE, mmapFlags, vfd->fd, 0);
	if (mapped == MAP_FAILED) {
		return NULL;
	}
	return mapped;
}

static void _vfdUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	msync(memory, size, MS_SYNC);
	munmap(memory, size);
}
#elif defined(_WIN32)
static void* _vfdMap(struct VFile* vf, size_t size, int flags) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	if (!size) {
		return NULL;
	}
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
	struct HandleMappingTuple tuple = {0};
	tuple.handle = CreateFileMapping((HANDLE) _get_osfhandle(vfd->fd), 0, createFlags, 0, size & 0xFFFFFFFF, 0);
	tuple.mapping = MapViewOfFile(tuple.handle, mapFiles, 0, 0, size);
	*HandleMappingListAppend(&vfd->handles) = tuple;
	return tuple.mapping;
}

static void _vfdUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(size);
	struct VFileFD* vfd = (struct VFileFD*) vf;
	FlushViewOfFile(memory, size);
	size_t i;
	for (i = 0; i < HandleMappingListSize(&vfd->handles); ++i) {
		if (HandleMappingListGetPointer(&vfd->handles, i)->mapping == memory) {
			UnmapViewOfFile(memory);
			CloseHandle(HandleMappingListGetPointer(&vfd->handles, i)->handle);
			HandleMappingListShift(&vfd->handles, i, 1);
			break;
		}
	}
}
#else
static void* _vfdMap(struct VFile* vf, size_t size, int flags) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	if (!size) {
		vfd->writable = false;
		return NULL;
	}
	if (flags & MAP_WRITE) {
		vfd->writable = true;
	}
	void* mem = anonymousMemoryMap(size);
	if (!mem) {
		return NULL;
	}

	off_t pos = lseek(vfd->fd, 0, SEEK_CUR);
	lseek(vfd->fd, 0, SEEK_SET);
	read(vfd->fd, mem, size);
	lseek(vfd->fd, pos, SEEK_SET);
	return mem;
}

static void _vfdUnmap(struct VFile* vf, void* memory, size_t size) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	if (vfd->writable) {
		off_t pos = lseek(vfd->fd, 0, SEEK_CUR);
		lseek(vfd->fd, 0, SEEK_SET);
		write(vfd->fd, memory, size);
		lseek(vfd->fd, pos, SEEK_SET);
	}
	mappedMemoryFree(memory, size);
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

static bool _vfdSync(struct VFile* vf, void* buffer, size_t size) {
	UNUSED(buffer);
	UNUSED(size);
	struct VFileFD* vfd = (struct VFileFD*) vf;
#ifndef _WIN32
#ifdef HAVE_FUTIMENS
	futimens(vfd->fd, NULL);
#elif defined(HAVE_FUTIMES)
	futimes(vfd->fd, NULL);
#endif
	if (buffer && size) {
#ifdef _POSIX_MAPPED_FILES
		return msync(buffer, size, MS_ASYNC) == 0;
#else
		off_t pos = lseek(vfd->fd, 0, SEEK_CUR);
		lseek(vfd->fd, 0, SEEK_SET);
		ssize_t res = write(vfd->fd, buffer, size);
		lseek(vfd->fd, pos, SEEK_SET);
		if (res < 0) {
			return false;
		}
#endif
	}
	return fsync(vfd->fd) == 0;
#else
	HANDLE h = (HANDLE) _get_osfhandle(vfd->fd);
	FILETIME ft;
	SYSTEMTIME st;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	SetFileTime(h, NULL, &ft, &ft);
	if (buffer && size) {
		return FlushViewOfFile(buffer, size);
	}
	return FlushFileBuffers(h);
#endif
}
