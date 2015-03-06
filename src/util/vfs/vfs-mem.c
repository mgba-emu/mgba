/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/vfs.h"

struct VFileMem {
	struct VFile d;
	void* mem;
	size_t size;
	size_t offset;
};

static bool _vfmClose(struct VFile* vf);
static off_t _vfmSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vfmRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vfmWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vfmMap(struct VFile* vf, size_t size, int flags);
static void _vfmUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfmTruncate(struct VFile* vf, size_t size);
static ssize_t _vfmSize(struct VFile* vf);

struct VFile* VFileFromMemory(void* mem, size_t size) {
	if (!mem || !size) {
		return 0;
	}

	struct VFileMem* vfm = malloc(sizeof(struct VFileMem));
	if (!vfm) {
		return 0;
	}

	vfm->mem = mem;
	vfm->size = size;
	vfm->offset = 0;
	vfm->d.close = _vfmClose;
	vfm->d.seek = _vfmSeek;
	vfm->d.read = _vfmRead;
	vfm->d.readline = VFileReadline;
	vfm->d.write = _vfmWrite;
	vfm->d.map = _vfmMap;
	vfm->d.unmap = _vfmUnmap;
	vfm->d.truncate = _vfmTruncate;
	vfm->d.size = _vfmSize;

	return &vfm->d;
}

bool _vfmClose(struct VFile* vf) {
	struct VFileMem* vfm = (struct VFileMem*) vf;
	vfm->mem = 0;
	free(vfm);
	return true;
}

off_t _vfmSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	size_t position;
	switch (whence) {
	case SEEK_SET:
		position = offset;
		break;
	case SEEK_CUR:
		if (offset < 0 && ((vfm->offset < (size_t) -offset) || (offset == INT_MIN))) {
			return -1;
		}
		position = vfm->offset + offset;
		break;
	case SEEK_END:
		if (offset < 0 && ((vfm->size < (size_t) -offset) || (offset == INT_MIN))) {
			return -1;
		}
		position = vfm->size + offset;
		break;
	default:
		return -1;
	}

	if (position > vfm->size) {
		return -1;
	}

	vfm->offset = position;
	return position;
}

ssize_t _vfmRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	if (size + vfm->offset >= vfm->size) {
		size = vfm->size - vfm->offset;
	}

	memcpy(buffer, vfm->mem + vfm->offset, size);
	vfm->offset += size;
	return size;
}

ssize_t _vfmWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	if (size + vfm->offset >= vfm->size) {
		size = vfm->size - vfm->offset;
	}

	memcpy(vfm->mem + vfm->offset, buffer, size);
	vfm->offset += size;
	return size;
}

void* _vfmMap(struct VFile* vf, size_t size, int flags) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	UNUSED(flags);
	if (size > vfm->size) {
		return 0;
	}

	return vfm->mem;
}

void _vfmUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
}

void _vfmTruncate(struct VFile* vf, size_t size) {
	// TODO: Return value?
	UNUSED(vf);
	UNUSED(size);
}

ssize_t _vfmSize(struct VFile* vf) {
	struct VFileMem* vfm = (struct VFileMem*) vf;
	return vfm->size;
}
