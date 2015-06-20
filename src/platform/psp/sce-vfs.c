/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sce-vfs.h"

#include "util/vfs.h"
#include "util/memory.h"

struct VFileSce {
	struct VFile d;

	SceUID fd;
};

static bool _vfsceClose(struct VFile* vf);
static off_t _vfsceSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vfsceRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vfsceWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vfsceMap(struct VFile* vf, size_t size, int flags);
static void _vfsceUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfsceTruncate(struct VFile* vf, size_t size);
static ssize_t _vfsceSize(struct VFile* vf);

struct VFile* VFileOpenSce(const char* path, int flags, SceMode mode) {
	struct VFileSce* vfsce = malloc(sizeof(struct VFileSce));
	if (!vfsce) {
		return 0;
	}

	vfsce->fd = sceIoOpen(path, flags, mode);
	if (vfsce->fd < 0) {
		free(vfsce);
		return 0;
	}

	vfsce->d.close = _vfsceClose;
	vfsce->d.seek = _vfsceSeek;
	vfsce->d.read = _vfsceRead;
	vfsce->d.readline = 0;
	vfsce->d.write = _vfsceWrite;
	vfsce->d.map = _vfsceMap;
	vfsce->d.unmap = _vfsceUnmap;
	vfsce->d.truncate = _vfsceTruncate;
	vfsce->d.size = _vfsceSize;

	return &vfsce->d;
}

bool _vfsceClose(struct VFile* vf) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;

	return sceIoClose(vfsce->fd) >= 0;
}

off_t _vfsceSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	return sceIoLseek(vfsce->fd, offset, whence);
}

ssize_t _vfsceRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	return sceIoRead(vfsce->fd, buffer, size);
}

ssize_t _vfsceWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	return sceIoWrite(vfsce->fd, buffer, size);
}

static void* _vfsceMap(struct VFile* vf, size_t size, int flags) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	UNUSED(flags);
	void* buffer = anonymousMemoryMap(size);
	if (buffer) {
		sceIoRead(vfsce->fd, buffer, size);
	}
	return buffer;
}

static void _vfsceUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	mappedMemoryFree(memory, size);
}

static void _vfsceTruncate(struct VFile* vf, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	// TODO
}

ssize_t _vfsceSize(struct VFile* vf) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	SceOff cur = sceIoLseek(vfsce->fd, 0, SEEK_CUR);
	SceOff end = sceIoLseek(vfsce->fd, 0, SEEK_END);
	sceIoLseek(vfsce->fd, cur, SEEK_SET);
	return end;
}
