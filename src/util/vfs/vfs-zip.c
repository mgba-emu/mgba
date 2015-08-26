/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/vfs.h"

#ifdef USE_LIBZIP
#include <zip.h>


enum {
	BLOCK_SIZE = 1024
};

struct VDirEntryZip {
	struct VDirEntry d;
	struct zip* z;
	zip_int64_t index;
};

struct VDirZip {
	struct VDir d;
	struct zip* z;
	struct VDirEntryZip dirent;
};

struct VFileZip {
	struct VFile d;
	struct zip_file* zf;
	void* buffer;
	size_t offset;
	size_t bufferSize;
	size_t readSize;
	size_t fileSize;
};

static bool _vfzClose(struct VFile* vf);
static off_t _vfzSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vfzRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vfzWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vfzMap(struct VFile* vf, size_t size, int flags);
static void _vfzUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfzTruncate(struct VFile* vf, size_t size);
static ssize_t _vfzSize(struct VFile* vf);
static bool _vfzSync(struct VFile* vf, const void* buffer, size_t size);

static bool _vdzClose(struct VDir* vd);
static void _vdzRewind(struct VDir* vd);
static struct VDirEntry* _vdzListNext(struct VDir* vd);
static struct VFile* _vdzOpenFile(struct VDir* vd, const char* path, int mode);

static const char* _vdezName(struct VDirEntry* vde);
static enum VFSType _vdezType(struct VDirEntry* vde);

struct VDir* VDirOpenZip(const char* path, int flags) {
	int zflags = 0;
	if (flags & O_CREAT) {
		zflags |= ZIP_CREATE;
	}
	if (flags & O_EXCL) {
		zflags |= ZIP_EXCL;
	}

	struct zip* z = zip_open(path, zflags, 0);
	if (!z) {
		return 0;
	}
	struct VDirZip* vd = malloc(sizeof(struct VDirZip));

	vd->d.close = _vdzClose;
	vd->d.rewind = _vdzRewind;
	vd->d.listNext = _vdzListNext;
	vd->d.openFile = _vdzOpenFile;
	vd->z = z;

	vd->dirent.d.name = _vdezName;
	vd->dirent.d.type = _vdezType;
	vd->dirent.index = -1;
	vd->dirent.z = z;

	return &vd->d;
}

bool _vfzClose(struct VFile* vf) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	if (zip_fclose(vfz->zf) < 0) {
		return false;
	}
	free(vfz->buffer);
	free(vfz);
	return true;
}

off_t _vfzSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	size_t position;
	switch (whence) {
	case SEEK_SET:
		position = offset;
		break;
	case SEEK_CUR:
		if (offset < 0 && ((vfz->offset < (size_t) -offset) || (offset == INT_MIN))) {
			return -1;
		}
		position = vfz->offset + offset;
		break;
	case SEEK_END:
		if (offset < 0 && ((vfz->fileSize < (size_t) -offset) || (offset == INT_MIN))) {
			return -1;
		}
		position = vfz->fileSize + offset;
		break;
	default:
		return -1;
	}

	if (position <= vfz->offset) {
		vfz->offset = position;
		return position;
	}

	if (position <= vfz->fileSize) {
		ssize_t read = vf->read(vf, 0, position - vfz->offset);
		if (read < 0) {
			return -1;
		}
		return vfz->offset;
	}

	return -1;
}

ssize_t _vfzRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	size_t bytesRead = 0;
	if (!vfz->buffer) {
		vfz->bufferSize = BLOCK_SIZE;
		vfz->buffer = malloc(BLOCK_SIZE);
	}

	while (bytesRead < size) {
		if (vfz->offset < vfz->readSize) {
			size_t diff = vfz->readSize - vfz->offset;
			void* start = &((uint8_t*) vfz->buffer)[vfz->offset];
			if (diff > size - bytesRead) {
				diff = size - bytesRead;
			}
			if (buffer) {
				void* bufferOffset = &((uint8_t*) buffer)[bytesRead];
				memcpy(bufferOffset, start, diff);
			}
			vfz->offset += diff;
			bytesRead += diff;
			if (diff == size) {
				break;
			}
		}
		// offset == readSize
		if (vfz->readSize == vfz->bufferSize) {
			vfz->bufferSize *= 2;
			if (vfz->bufferSize > vfz->fileSize) {
				vfz->bufferSize = vfz->fileSize;
			}
			vfz->buffer = realloc(vfz->buffer, vfz->bufferSize);
		}
		if (vfz->readSize < vfz->bufferSize) {
			void* start = &((uint8_t*) vfz->buffer)[vfz->readSize];
			size_t toRead = vfz->bufferSize - vfz->readSize;
			if (toRead > BLOCK_SIZE) {
				toRead = BLOCK_SIZE;
			}
			ssize_t zipRead = zip_fread(vfz->zf, start, toRead);
			if (zipRead < 0) {
				if (bytesRead == 0) {
					return -1;
				}
				break;
			}
			if (zipRead == 0) {
				break;
			}
			vfz->readSize += zipRead;
		} else {
			break;
		}
	}
	return bytesRead;
}

ssize_t _vfzWrite(struct VFile* vf, const void* buffer, size_t size) {
	// TODO
	UNUSED(vf);
	UNUSED(buffer);
	UNUSED(size);
	return -1;
}

void* _vfzMap(struct VFile* vf, size_t size, int flags) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	UNUSED(flags);
	if (size > vfz->readSize) {
		vf->read(vf, 0, size - vfz->readSize);
	}
	return vfz->buffer;
}

void _vfzUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
}

void _vfzTruncate(struct VFile* vf, size_t size) {
	// TODO
	UNUSED(vf);
	UNUSED(size);
}

ssize_t _vfzSize(struct VFile* vf) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	return vfz->fileSize;
}

bool _vdzClose(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	if (zip_close(vdz->z) < 0) {
		return false;
	}
	free(vdz);
	return true;
}

void _vdzRewind(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	vdz->dirent.index = -1;
}

struct VDirEntry* _vdzListNext(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	zip_int64_t maxIndex = zip_get_num_entries(vdz->z, 0);
	if (maxIndex <= vdz->dirent.index + 1) {
		return 0;
	}
	++vdz->dirent.index;
	return &vdz->dirent.d;
}

struct VFile* _vdzOpenFile(struct VDir* vd, const char* path, int mode) {
	UNUSED(mode);
	// TODO: support truncating, appending and creating, and write
	struct VDirZip* vdz = (struct VDirZip*) vd;

	if ((mode & O_RDWR) == O_RDWR) {
		// libzip doesn't allow for random access, so read/write is impossible without
		// reading the entire file first. This approach will be supported eventually.
		return 0;
	}

	if (mode & O_WRONLY) {
		// Write support is not yet implemented.
		return 0;
	}

	struct zip_stat s;
	if (zip_stat(vdz->z, path, 0, &s) < 0) {
		return 0;
	}

	struct zip_file* zf = zip_fopen(vdz->z, path, 0);
	if (!zf) {
		return 0;
	}

	struct VFileZip* vfz = malloc(sizeof(struct VFileZip));
	vfz->zf = zf;
	vfz->buffer = 0;
	vfz->offset = 0;
	vfz->bufferSize = 0;
	vfz->readSize = 0;
	vfz->fileSize = s.size;

	vfz->d.close = _vfzClose;
	vfz->d.seek = _vfzSeek;
	vfz->d.read = _vfzRead;
	vfz->d.readline = VFileReadline;
	vfz->d.write = _vfzWrite;
	vfz->d.map = _vfzMap;
	vfz->d.unmap = _vfzUnmap;
	vfz->d.truncate = _vfzTruncate;
	vfz->d.size = _vfzSize;
	vfz->d.sync = _vfzSync;

	return &vfz->d;
}

bool _vfzSync(struct VFile* vf, const void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
	return false;
}

const char* _vdezName(struct VDirEntry* vde) {
	struct VDirEntryZip* vdez = (struct VDirEntryZip*) vde;
	struct zip_stat s;
	if (zip_stat_index(vdez->z, vdez->index, 0, &s) < 0) {
		return 0;
	}
	return s.name;
}

static enum VFSType _vdezType(struct VDirEntry* vde) {
	struct VDirEntryZip* vdez = (struct VDirEntryZip*) vde;
	return VFS_UNKNOWN;
}

#endif
