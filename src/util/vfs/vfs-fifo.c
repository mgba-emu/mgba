/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>
#include <mgba-util/circle-buffer.h>

struct VFileFIFO {
	struct VFile d;
	struct CircleBuffer* backing;
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

struct VFile* VFileFIFO(struct CircleBuffer* backing) {
	if (!backing) {
		return NULL;
	}

	struct VFileFIFO* vff = malloc(sizeof(*vff));
	if (!vff) {
		return NULL;
	}

	vff->backing = backing;
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


static bool _vffClose(struct VFile* vf) {
	free(vf);
	return true;
}

static off_t _vffSeek(struct VFile* vf, off_t offset, int whence) {
	UNUSED(vf);
	UNUSED(offset);
	UNUSED(whence);
	return 0;
}

static ssize_t _vffRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileFIFO* vff = (struct VFileFIFO*) vf;
	return CircleBufferRead(vff->backing, buffer, size);
}

static ssize_t _vffWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileFIFO* vff = (struct VFileFIFO*) vf;
	return CircleBufferWrite(vff->backing, buffer, size);
}

static void* _vffMap(struct VFile* vf, size_t size, int flags) {
	UNUSED(vf);
	UNUSED(size);
	UNUSED(flags);
	return NULL;
}

static void _vffUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
}

static void _vffTruncate(struct VFile* vf, size_t size) {
	struct VFileFIFO* vff = (struct VFileFIFO*) vf;
	if (!size) {
		CircleBufferClear(vff->backing);
	}
}

static ssize_t _vffSize(struct VFile* vf) {
	struct VFileFIFO* vff = (struct VFileFIFO*) vf;
	return CircleBufferSize(vff->backing);
}

static bool _vffSync(struct VFile* vf, void* buffer, size_t size) {
	UNUSED(vf);
	UNUSED(buffer);
	UNUSED(size);
	return true;
}
