#include "util/vfile.h"

#include <fcntl.h>

struct VFileFD {
	struct VFile d;
	int fd;
};

static bool _vfdClose(struct VFile* vf);
static size_t _vfdSeek(struct VFile* vf, off_t offset, int whence);
static size_t _vfdRead(struct VFile* vf, void* buffer, size_t size);
static size_t _vfdReadline(struct VFile* vf, char* buffer, size_t size);
static size_t _vfdWrite(struct VFile* vf, void* buffer, size_t size);

struct VFile* VFileOpen(const char* path, int flags) {
	int fd = open(path, flags);
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
