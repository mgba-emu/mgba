/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "3ds-vfs.h"

#include "util/memory.h"

struct VFile3DS {
	struct VFile d;

	Handle handle;
	u64 offset;
};

static bool _vf3dClose(struct VFile* vf);
static off_t _vf3dSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vf3dRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vf3dWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vf3dMap(struct VFile* vf, size_t size, int flags);
static void _vf3dUnmap(struct VFile* vf, void* memory, size_t size);
static void _vf3dTruncate(struct VFile* vf, size_t size);
static ssize_t _vf3dSize(struct VFile* vf);

struct VFile* VFileOpen3DS(FS_archive* archive, const char* path, int flags) {
	struct VFile3DS* vf3d = malloc(sizeof(struct VFile3DS));
	if (!vf3d) {
		return 0;
	}

	FS_path newPath = FS_makePath(PATH_CHAR, path);
	Result res = FSUSER_OpenFile(0, &vf3d->handle, *archive, newPath, flags, FS_ATTRIBUTE_NONE);
	if (res & 0xFFFC03FF) {
		free(vf3d);
		return 0;
	}

	vf3d->offset = 0;

	vf3d->d.close = _vf3dClose;
	vf3d->d.seek = _vf3dSeek;
	vf3d->d.read = _vf3dRead;
	vf3d->d.readline = 0;
	vf3d->d.write = _vf3dWrite;
	vf3d->d.map = _vf3dMap;
	vf3d->d.unmap = _vf3dUnmap;
	vf3d->d.truncate = _vf3dTruncate;
	vf3d->d.size = _vf3dSize;

	return &vf3d->d;
}

bool _vf3dClose(struct VFile* vf) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;

	FSFILE_Close(vf3d->handle);
	svcCloseHandle(vf3d->handle);
	return true;
}

off_t _vf3dSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u64 size;
	switch (whence) {
	case SEEK_SET:
		vf3d->offset = offset;
		break;
	case SEEK_END:
		FSFILE_GetSize(vf3d->handle, &size);
		vf3d->offset = size;
		// Fall through
	case SEEK_CUR:
		vf3d->offset += offset;
		break;
	}
	return vf3d->offset;
}

ssize_t _vf3dRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u32 sizeRead;
	Result res = FSFILE_Read(vf3d->handle, &sizeRead, vf3d->offset, buffer, size);
	if (res) {
		return -1;
	}
	vf3d->offset += sizeRead;
	return sizeRead;
}

ssize_t _vf3dWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u32 sizeWritten;
	Result res = FSFILE_Write(vf3d->handle, &sizeWritten, vf3d->offset, buffer, size, FS_WRITE_FLUSH);
	if (res) {
		return -1;
	}
	vf3d->offset += sizeWritten;
	return sizeWritten;
}

static void* _vf3dMap(struct VFile* vf, size_t size, int flags) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	UNUSED(flags);
	void* buffer = anonymousMemoryMap(size);
	if (buffer) {
		u32 sizeRead;
		FSFILE_Read(vf3d->handle, &sizeRead, 0, buffer, size);
	}
	return buffer;
}

static void _vf3dUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	mappedMemoryFree(memory, size);
}

static void _vf3dTruncate(struct VFile* vf, size_t size) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	FSFILE_SetSize(vf3d->handle, size);
}

ssize_t _vf3dSize(struct VFile* vf) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u64 size;
	FSFILE_GetSize(vf3d->handle, &size);
	return size;
}
