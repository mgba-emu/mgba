#include "util/vfile.h"

#include <fcntl.h>

#ifndef _WIN32
#include <sys/mman.h>
#else
#include <io.h>
#include <Windows.h>
#endif

struct VFileFD {
	struct VFile d;
	int fd;
#ifdef _WIN32
	HANDLE hMap;
#endif
};

static bool _vfdClose(struct VFile* vf);
static size_t _vfdSeek(struct VFile* vf, off_t offset, int whence);
static size_t _vfdRead(struct VFile* vf, void* buffer, size_t size);
static size_t _vfdReadline(struct VFile* vf, char* buffer, size_t size);
static size_t _vfdWrite(struct VFile* vf, void* buffer, size_t size);
static void* _vfdMap(struct VFile* vf, size_t size, int flags);
static void _vfdUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfdTruncate(struct VFile* vf, size_t size);

struct VFile* VFileOpen(const char* path, int flags) {
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

size_t _vfdSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	return lseek(vfd->fd, offset, whence);
}

size_t _vfdRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	return read(vfd->fd, buffer, size);
}

size_t _vfdReadline(struct VFile* vf, char* buffer, size_t size) {
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

size_t _vfdWrite(struct VFile* vf, void* buffer, size_t size) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	return write(vfd->fd, buffer, size);
}

#ifndef _WIN32
static void* _vfdMap(struct VFile* vf, size_t size, int flags) {
	struct VFileFD* vfd = (struct VFileFD*) vf;
	int mmapFlags = MAP_PRIVATE;
	if (flags & MEMORY_WRITE) {
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
	int createFlags = PAGE_READONLY;
	int mapFiles = FILE_MAP_READ;
	if (flags & MEMORY_WRITE) {
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
	return MapViewOfFile(hMap, mapFiles, 0, 0, size);
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
