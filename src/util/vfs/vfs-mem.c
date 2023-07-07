/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>
#include <mgba-util/math.h>
#include <mgba-util/memory.h>

struct VFileMem {
	struct VFile d;
	void* mem;
	size_t size;
	size_t bufferSize;
	size_t offset;
};

static bool _vfmClose(struct VFile* vf);
static bool _vfmCloseFree(struct VFile* vf);
static off_t _vfmSeek(struct VFile* vf, off_t offset, int whence);
static off_t _vfmSeekExpanding(struct VFile* vf, off_t offset, int whence);
static ssize_t _vfmRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vfmWrite(struct VFile* vf, const void* buffer, size_t size);
static ssize_t _vfmWriteExpanding(struct VFile* vf, const void* buffer, size_t size);
static ssize_t _vfmWriteNoop(struct VFile* vf, const void* buffer, size_t size);
static void* _vfmMap(struct VFile* vf, size_t size, int flags);
static void _vfmUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfmTruncate(struct VFile* vf, size_t size);
static void _vfmTruncateNoop(struct VFile* vf, size_t size);
static ssize_t _vfmSize(struct VFile* vf);
static bool _vfmSync(struct VFile* vf, void* buffer, size_t size);

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
	vfm->bufferSize = size;
	vfm->offset = 0;
	vfm->d.close = _vfmClose;
	vfm->d.seek = _vfmSeek;
	vfm->d.read = _vfmRead;
	vfm->d.readline = VFileReadline;
	vfm->d.write = _vfmWrite;
	vfm->d.map = _vfmMap;
	vfm->d.unmap = _vfmUnmap;
	vfm->d.truncate = _vfmTruncateNoop;
	vfm->d.size = _vfmSize;
	vfm->d.sync = _vfmSync;

	return &vfm->d;
}

struct VFile* VFileFromConstMemory(const void* mem, size_t size) {
	if (!mem || !size) {
		return 0;
	}

	struct VFileMem* vfm = malloc(sizeof(struct VFileMem));
	if (!vfm) {
		return 0;
	}

	vfm->mem = (void*) mem;
	vfm->size = size;
	vfm->bufferSize = size;
	vfm->offset = 0;
	vfm->d.close = _vfmClose;
	vfm->d.seek = _vfmSeek;
	vfm->d.read = _vfmRead;
	vfm->d.readline = VFileReadline;
	vfm->d.write = _vfmWriteNoop;
	vfm->d.map = _vfmMap;
	vfm->d.unmap = _vfmUnmap;
	vfm->d.truncate = _vfmTruncateNoop;
	vfm->d.size = _vfmSize;
	vfm->d.sync = _vfmSync;

	return &vfm->d;
}

struct VFile* VFileMemChunk(const void* mem, size_t size) {
	struct VFileMem* vfm = malloc(sizeof(struct VFileMem));
	if (!vfm) {
		return 0;
	}

	vfm->size = size;
	vfm->bufferSize = toPow2(size);
	if (size) {
		vfm->mem = anonymousMemoryMap(vfm->bufferSize);
		if (mem) {
			memcpy(vfm->mem, mem, size);
		}
	} else {
		vfm->mem = 0;
	}
	vfm->offset = 0;
	vfm->d.close = _vfmCloseFree;
	vfm->d.seek = _vfmSeekExpanding;
	vfm->d.read = _vfmRead;
	vfm->d.readline = VFileReadline;
	vfm->d.write = _vfmWriteExpanding;
	vfm->d.map = _vfmMap;
	vfm->d.unmap = _vfmUnmap;
	vfm->d.truncate = _vfmTruncate;
	vfm->d.size = _vfmSize;
	vfm->d.sync = _vfmSync;

	return &vfm->d;
}

void _vfmExpand(struct VFileMem* vfm, size_t newSize) {
	size_t alignedSize = toPow2(newSize);
	if (alignedSize > vfm->bufferSize) {
		void* oldBuf = vfm->mem;
		vfm->mem = anonymousMemoryMap(alignedSize);
		if (oldBuf) {
			if (newSize < vfm->size) {
				memcpy(vfm->mem, oldBuf, newSize);
			} else {
				memcpy(vfm->mem, oldBuf, vfm->size);
			}
			mappedMemoryFree(oldBuf, vfm->bufferSize);
		}
		vfm->bufferSize = alignedSize;
	}
	vfm->size = newSize;
}

bool _vfmClose(struct VFile* vf) {
	struct VFileMem* vfm = (struct VFileMem*) vf;
	vfm->mem = 0;
	free(vfm);
	return true;
}

bool _vfmCloseFree(struct VFile* vf) {
	struct VFileMem* vfm = (struct VFileMem*) vf;
	mappedMemoryFree(vfm->mem, vfm->bufferSize);
	vfm->mem = 0;
	free(vfm);
	return true;
}

off_t _vfmSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	size_t position;
	switch (whence) {
	case SEEK_SET:
		if (offset < 0) {
			return -1;
		}
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

off_t _vfmSeekExpanding(struct VFile* vf, off_t offset, int whence) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	size_t position;
	switch (whence) {
	case SEEK_SET:
		if (offset < 0) {
			return -1;
		}
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
		_vfmExpand(vfm, position);
	}

	vfm->offset = position;
	return position;
}

ssize_t _vfmRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	if (size + vfm->offset >= vfm->size) {
		size = vfm->size - vfm->offset;
	}
	if (size) {
		memcpy(buffer, (void*) ((uintptr_t) vfm->mem + vfm->offset), size);
		vfm->offset += size;
	}
	return size;
}

ssize_t _vfmWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	if (size + vfm->offset >= vfm->size) {
		size = vfm->size - vfm->offset;
	}

	memcpy((void*) ((uintptr_t) vfm->mem + vfm->offset), buffer, size);
	vfm->offset += size;
	return size;
}

ssize_t _vfmWriteExpanding(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileMem* vfm = (struct VFileMem*) vf;

	if (size + vfm->offset > vfm->size) {
		_vfmExpand(vfm, vfm->offset + size);
	}

	memcpy((void*) ((uintptr_t) vfm->mem + vfm->offset), buffer, size);
	vfm->offset += size;
	return size;
}


ssize_t _vfmWriteNoop(struct VFile* vf, const void* buffer, size_t size) {
	UNUSED(vf);
	UNUSED(buffer);
	UNUSED(size);
	return -1;
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
	struct VFileMem* vfm = (struct VFileMem*) vf;
	_vfmExpand(vfm, size);
}

void _vfmTruncateNoop(struct VFile* vf, size_t size) {
	// TODO: Return value?
	UNUSED(vf);
	UNUSED(size);
}

ssize_t _vfmSize(struct VFile* vf) {
	struct VFileMem* vfm = (struct VFileMem*) vf;
	return vfm->size;
}

bool _vfmSync(struct VFile* vf, void* buffer, size_t size) {
	UNUSED(vf);
	UNUSED(buffer);
	UNUSED(size);
	return true;
}
