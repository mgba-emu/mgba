/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#ifdef USE_LZMA

#include <mgba-util/string.h>

#include "third-party/lzma/7z.h"
#include "third-party/lzma/7zAlloc.h"
#include "third-party/lzma/7zBuf.h"
#include "third-party/lzma/7zCrc.h"
#include "third-party/lzma/7zFile.h"
#include "third-party/lzma/7zVersion.h"

#define BUFFER_SIZE 0x2000

struct VDirEntry7z {
	struct VDirEntry d;

	struct VDir7z* vd;
	UInt32 index;
	char* utf8;
};

struct VDir7z {
	struct VDir d;
	struct VDirEntry7z dirent;

	// What is all this garbage?
	CFileInStream archiveStream;
	CLookToRead2 lookStream;
	CSzArEx db;
	ISzAlloc allocImp;
	ISzAlloc allocTempImp;
};

struct VFile7z {
	struct VFile d;

	struct VDir7z* vd;

	size_t offset;

	Byte* outBuffer;
	size_t bufferOffset;
	size_t size;
};

static bool _vf7zClose(struct VFile* vf);
static off_t _vf7zSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vf7zRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vf7zWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vf7zMap(struct VFile* vf, size_t size, int flags);
static void _vf7zUnmap(struct VFile* vf, void* memory, size_t size);
static void _vf7zTruncate(struct VFile* vf, size_t size);
static ssize_t _vf7zSize(struct VFile* vf);
static bool _vf7zSync(struct VFile* vf, void* buffer, size_t size);

static bool _vd7zClose(struct VDir* vd);
static void _vd7zRewind(struct VDir* vd);
static struct VDirEntry* _vd7zListNext(struct VDir* vd);
static struct VFile* _vd7zOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vd7zOpenDir(struct VDir* vd, const char* path);
static bool _vd7zDeleteFile(struct VDir* vd, const char* path);

static const char* _vde7zName(struct VDirEntry* vde);
static enum VFSType _vde7zType(struct VDirEntry* vde);

struct VDir* VDirOpen7z(const char* path, int flags) {
	if (flags & O_WRONLY || flags & O_CREAT) {
		return 0;
	}

	struct VDir7z* vd = malloc(sizeof(struct VDir7z));

	// What does any of this mean, Igor?
	if (InFile_Open(&vd->archiveStream.file, path)) {
		free(vd);
		return 0;
	}

	vd->allocImp.Alloc = SzAlloc;
	vd->allocImp.Free = SzFree;

	vd->allocTempImp.Alloc = SzAllocTemp;
	vd->allocTempImp.Free = SzFreeTemp;

	FileInStream_CreateVTable(&vd->archiveStream);
	LookToRead2_CreateVTable(&vd->lookStream, False);

	vd->lookStream.realStream = &vd->archiveStream.vt;
	vd->lookStream.buf = malloc(BUFFER_SIZE);
	vd->lookStream.bufSize = BUFFER_SIZE;

	LookToRead2_Init(&vd->lookStream);

	CrcGenerateTable();

	SzArEx_Init(&vd->db);
	SRes res = SzArEx_Open(&vd->db, &vd->lookStream.vt, &vd->allocImp, &vd->allocTempImp);
	if (res != SZ_OK) {
		SzArEx_Free(&vd->db, &vd->allocImp);
		File_Close(&vd->archiveStream.file);
		free(vd->lookStream.buf);
		free(vd);
		return 0;
	}

	vd->dirent.index = -1;
	vd->dirent.utf8 = 0;
	vd->dirent.vd = vd;
	vd->dirent.d.name = _vde7zName;
	vd->dirent.d.type = _vde7zType;

	vd->d.close = _vd7zClose;
	vd->d.rewind = _vd7zRewind;
	vd->d.listNext = _vd7zListNext;
	vd->d.openFile = _vd7zOpenFile;
	vd->d.openDir = _vd7zOpenDir;
	vd->d.deleteFile = _vd7zDeleteFile;

	return &vd->d;
}

bool _vf7zClose(struct VFile* vf) {
	struct VFile7z* vf7z = (struct VFile7z*) vf;
	IAlloc_Free(&vf7z->vd->allocImp, vf7z->outBuffer);
	free(vf7z);
	return true;
}

off_t _vf7zSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFile7z* vf7z = (struct VFile7z*) vf;

	size_t position;
	switch (whence) {
	case SEEK_SET:
		position = offset;
		break;
	case SEEK_CUR:
		if (offset < 0 && ((vf7z->offset < (size_t) -offset) || (offset == INT_MIN))) {
			return -1;
		}
		position = vf7z->offset + offset;
		break;
	case SEEK_END:
		if (offset < 0 && ((vf7z->size < (size_t) -offset) || (offset == INT_MIN))) {
			return -1;
		}
		position = vf7z->size + offset;
		break;
	default:
		return -1;
	}

	if (position > vf7z->size) {
		return -1;
	}

	vf7z->offset = position;
	return position;
}

ssize_t _vf7zRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFile7z* vf7z = (struct VFile7z*) vf;

	if (size + vf7z->offset >= vf7z->size) {
		size = vf7z->size - vf7z->offset;
	}

	memcpy(buffer, vf7z->outBuffer + vf7z->offset + vf7z->bufferOffset, size);
	vf7z->offset += size;
	return size;
}

ssize_t _vf7zWrite(struct VFile* vf, const void* buffer, size_t size) {
	// TODO
	UNUSED(vf);
	UNUSED(buffer);
	UNUSED(size);
	return -1;
}

void* _vf7zMap(struct VFile* vf, size_t size, int flags) {
	struct VFile7z* vf7z = (struct VFile7z*) vf;

	UNUSED(flags);
	if (size > vf7z->size) {
		return 0;
	}

	return vf7z->outBuffer + vf7z->bufferOffset;
}

void _vf7zUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
}

void _vf7zTruncate(struct VFile* vf, size_t size) {
	// TODO
	UNUSED(vf);
	UNUSED(size);
}

ssize_t _vf7zSize(struct VFile* vf) {
	struct VFile7z* vf7z = (struct VFile7z*) vf;
	return vf7z->size;
}

bool _vd7zClose(struct VDir* vd) {
	struct VDir7z* vd7z = (struct VDir7z*) vd;
	SzArEx_Free(&vd7z->db, &vd7z->allocImp);
	File_Close(&vd7z->archiveStream.file);

	free(vd7z->lookStream.buf);
	free(vd7z->dirent.utf8);
	vd7z->dirent.utf8 = 0;

	free(vd7z);
	return true;
}

void _vd7zRewind(struct VDir* vd) {
	struct VDir7z* vd7z = (struct VDir7z*) vd;
	free(vd7z->dirent.utf8);
	vd7z->dirent.utf8 = 0;
	vd7z->dirent.index = -1;
}

struct VDirEntry* _vd7zListNext(struct VDir* vd) {
	struct VDir7z* vd7z = (struct VDir7z*) vd;
	if (vd7z->db.NumFiles <= vd7z->dirent.index + 1) {
		return 0;
	}
	free(vd7z->dirent.utf8);
	vd7z->dirent.utf8 = 0;
	++vd7z->dirent.index;
	return &vd7z->dirent.d;
}

struct VFile* _vd7zOpenFile(struct VDir* vd, const char* path, int mode) {
	UNUSED(mode);
	// TODO: support truncating, appending and creating, and write
	struct VDir7z* vd7z = (struct VDir7z*) vd;

	if ((mode & O_RDWR) == O_RDWR) {
		// Read/Write support is not yet implemented.
		return 0;
	}

	if (mode & O_WRONLY) {
		// Write support is not yet implemented.
		return 0;
	}

	size_t pathLength = strlen(path);

	UInt32 i;
	for (i = 0; i < vd7z->db.NumFiles; ++i) {
		if (SzArEx_IsDir(&vd7z->db, i)) {
			continue;
		}
		size_t nameLength = SzArEx_GetFileNameUtf16(&vd7z->db, i, 0) * sizeof(UInt16);
		UInt16* name = malloc(nameLength);
		SzArEx_GetFileNameUtf16(&vd7z->db, i, name);

		if (utfcmp(name, path, nameLength - sizeof(UInt16), pathLength) == 0) {
			free(name);
			break;
		}

		free(name);
	}

	if (i == vd7z->db.NumFiles) {
		return 0; // No file found
	}

	struct VFile7z* vf = malloc(sizeof(struct VFile7z));
	vf->vd = vd7z;

	size_t outBufferSize;
	UInt32 blockIndex;

	vf->outBuffer = 0;
	SRes res = SzArEx_Extract(&vd7z->db, &vd7z->lookStream.vt, i, &blockIndex,
		&vf->outBuffer, &outBufferSize,
		&vf->bufferOffset, &vf->size,
		&vd7z->allocImp, &vd7z->allocTempImp);

	if (res != SZ_OK) {
		free(vf);
		return 0;
	}

	vf->d.close = _vf7zClose;
	vf->d.seek = _vf7zSeek;
	vf->d.read = _vf7zRead;
	vf->d.readline = VFileReadline;
	vf->d.write = _vf7zWrite;
	vf->d.map = _vf7zMap;
	vf->d.unmap = _vf7zUnmap;
	vf->d.truncate = _vf7zTruncate;
	vf->d.size = _vf7zSize;
	vf->d.sync = _vf7zSync;
	vf->offset = 0;

	return &vf->d;
}

struct VDir* _vd7zOpenDir(struct VDir* vd, const char* path) {
	UNUSED(vd);
	UNUSED(path);
	return 0;
}

bool _vd7zDeleteFile(struct VDir* vd, const char* path) {
	UNUSED(vd);
	UNUSED(path);
	// TODO
	return false;
}

bool _vf7zSync(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
	return false;
}

const char* _vde7zName(struct VDirEntry* vde) {
	struct VDirEntry7z* vde7z = (struct VDirEntry7z*) vde;
	if (!vde7z->utf8) {
		size_t nameLength = SzArEx_GetFileNameUtf16(&vde7z->vd->db, vde7z->index, 0) * sizeof(UInt16);
		UInt16* name = malloc(nameLength);
		SzArEx_GetFileNameUtf16(&vde7z->vd->db, vde7z->index, name);
		vde7z->utf8 = utf16to8(name, nameLength - sizeof(UInt16));
		free(name);
	}

	return vde7z->utf8;
}

static enum VFSType _vde7zType(struct VDirEntry* vde) {
	struct VDirEntry7z* vde7z = (struct VDirEntry7z*) vde;
	if (SzArEx_IsDir(&vde7z->vd->db, vde7z->index)) {
		return VFS_DIRECTORY;
	}
	return VFS_FILE;
}

#endif
