/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/vfs.h"

#include "util/string.h"

static bool _vdwClose(struct VDir* vd);
static void _vdwRewind(struct VDir* vd);
static struct VDirEntry* _vdwListNext(struct VDir* vd);
static struct VFile* _vdwOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vdwOpenDir(struct VDir* vd, const char* path);
static bool _vdwDeleteFile(struct VDir* vd, const char* path);

static const char* _vdweName(struct VDirEntry* vde);
static enum VFSType _vdweType(struct VDirEntry* vde);

struct VDirW32;
struct VDirEntryW32 {
	struct VDirEntry d;
	WIN32_FIND_DATA ffData;
};

struct VDirW32 {
	struct VDir d;
	HANDLE handle;
	struct VDirEntryW32 vde;
	char* path;
};

struct VDir* VDirOpen(const char* path) {
	if (!path || !path[0]) {
		return 0;
	}
	char name[MAX_PATH];
	_snprintf(name, sizeof(name), "%s\\*", path);
	WIN32_FIND_DATA ffData;
	HANDLE handle = FindFirstFile(name, &ffData);
	if (handle == INVALID_HANDLE_VALUE) {
		return 0;
	}

	struct VDirW32* vd = malloc(sizeof(struct VDirW32));
	if (!vd) {
		FindClose(handle);
		return 0;
	}

	vd->d.close = _vdwClose;
	vd->d.rewind = _vdwRewind;
	vd->d.listNext = _vdwListNext;
	vd->d.openFile = _vdwOpenFile;
	vd->d.openDir = _vdwOpenDir;
	vd->d.deleteFile = _vdwDeleteFile;
	vd->handle = handle;
	vd->path = _strdup(path);

	vd->vde.d.name = _vdweName;
	vd->vde.d.type = _vdweType;
	vd->vde.ffData = ffData;

	return &vd->d;
}

bool _vdwClose(struct VDir* vd) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	FindClose(vdw->handle);
	free(vdw);
	return true;
}

void _vdwRewind(struct VDir* vd) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	FindClose(vdw->handle);
	char name[MAX_PATH];
	_snprintf(name, sizeof(name), "%s\\*", vdw->path);
	vdw->handle = FindFirstFile(name, &vdw->vde.ffData);
}

struct VDirEntry* _vdwListNext(struct VDir* vd) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	if (FindNextFile(vdw->handle, &vdw->vde.ffData)) {
		return &vdw->vde.d;
	}

	return 0;
}

struct VFile* _vdwOpenFile(struct VDir* vd, const char* path, int mode) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdw->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	sprintf(combined, "%s\\%s", dir, path);

	struct VFile* file = VFileOpen(combined, mode);
	free(combined);
	return file;
}

struct VDir* _vdwOpenDir(struct VDir* vd, const char* path) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdw->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	sprintf(combined, "%s\\%s", dir, path);

	struct VDir* vd2 = VDirOpen(combined);
	if (!vd2) {
		vd2 = VDirOpenArchive(combined);
	}
	free(combined);
	return vd2;
}

bool _vdwDeleteFile(struct VDir* vd, const char* path) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdw->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	sprintf(combined, "%s\\%s", dir, path);

	bool ret = DeleteFile(combined);
	free(combined);
	return ret;
}

const char* _vdweName(struct VDirEntry* vde) {
	struct VDirEntryW32* vdwe = (struct VDirEntryW32*) vde;
	return vdwe->ffData.cFileName;
}

static enum VFSType _vdweType(struct VDirEntry* vde) {
	struct VDirEntryW32* vdwe = (struct VDirEntryW32*) vde;
	if (vdwe->ffData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		return VFS_DIRECTORY;
	}
	return VFS_FILE;
}
