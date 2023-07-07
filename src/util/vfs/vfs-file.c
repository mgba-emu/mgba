/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#include <mgba-util/memory.h>

#include <errno.h>
#include <stdio.h>

struct VFileFILE {
	struct VFile d;
	FILE* file;
	bool writable;
};

static bool _vffClose(struct VFile* vf);
static off_t _vffSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vffRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vffWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vffMap(struct VFile* vf, size_t size, int flags);
static void _vffUnmap(struct VFile* vf, void* memory, size_t size);
static void _vffTruncate(struct VFile* vf, size_t size);
static ssize_t _vffSize(struct VFile* vf);
static bool _vffSync(struct VFile* vf, void* buffer, size_t size);

struct VFile* VFileFOpen(const char* path, const char* mode) {
	if (!path && !mode) {
		return 0;
	}
	FILE* file = fopen(path, mode);
	if (!file && errno == ENOENT && strcmp(mode, "r+b") == 0) {
		file = fopen(path, "w+b");
	}
	return VFileFromFILE(file);
}

struct VFile* VFileFromFILE(FILE* file) {
	if (!file) {
		return 0;
	}

	struct VFileFILE* vff = malloc(sizeof(struct VFileFILE));
	if (!vff) {
		return 0;
	}

	vff->file = file;
	vff->writable = false;
	vff->d.close = _vffClose;
	vff->d.seek = _vffSeek;
	vff->d.read = _vffRead;
	vff->d.readline = VFileReadline;
	vff->d.write = _vffWrite;
	vff->d.map = _vffMap;
	vff->d.unmap = _vffUnmap;
	vff->d.truncate = _vffTruncate;
	vff->d.size = _vffSize;
	vff->d.sync = _vffSync;

	return &vff->d;
}

bool _vffClose(struct VFile* vf) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	if (fclose(vff->file) < 0) {
		return false;
	}
	free(vff);
	return true;
}

off_t _vffSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	fseek(vff->file, offset, whence);
	return ftell(vff->file);
}

ssize_t _vffRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	return fread(buffer, 1, size, vff->file);
}

ssize_t _vffWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	return fwrite(buffer, 1, size, vff->file);
}

static void* _vffMap(struct VFile* vf, size_t size, int flags) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	if (flags & MAP_WRITE) {
		vff->writable = true;
	}
	void* mem = anonymousMemoryMap(size);
	if (!mem) {
		return 0;
	}
	long pos = ftell(vff->file);
	fseek(vff->file, 0, SEEK_SET);
	fread(mem, size, 1, vff->file);
	fseek(vff->file, pos, SEEK_SET);
	return mem;
}

static void _vffUnmap(struct VFile* vf, void* memory, size_t size) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	if (vff->writable) {
		long pos = ftell(vff->file);
		fseek(vff->file, 0, SEEK_SET);
		fwrite(memory, size, 1, vff->file);
		fseek(vff->file, pos, SEEK_SET);
	}
	mappedMemoryFree(memory, size);
}

static void _vffTruncate(struct VFile* vf, size_t size) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	long pos = ftell(vff->file);
	fseek(vff->file, 0, SEEK_END);
	ssize_t realSize = ftell(vff->file);
	if (realSize < 0) {
		return;
	}
	while (size > (size_t) realSize) {
		static const char zeros[128] = "";
		size_t diff = size - realSize;
		if (diff > sizeof(zeros)) {
			diff = sizeof(zeros);
		}
		fwrite(zeros, diff, 1, vff->file);
		realSize += diff;
	}
	fseek(vff->file, pos, SEEK_SET);
}

static ssize_t _vffSize(struct VFile* vf) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	long pos = ftell(vff->file);
	fseek(vff->file, 0, SEEK_END);
	ssize_t size = ftell(vff->file);
	fseek(vff->file, pos, SEEK_SET);
	return size;
}

static bool _vffSync(struct VFile* vf, void* buffer, size_t size) {
	struct VFileFILE* vff = (struct VFileFILE*) vf;
	if (buffer && size) {
		long pos = ftell(vff->file);
		fseek(vff->file, 0, SEEK_SET);
		size_t res = fwrite(buffer, size, 1, vff->file);
		fseek(vff->file, pos, SEEK_SET);
		if (res != 1) {
			return false;
		}
	}
	return fflush(vff->file) == 0;
}
