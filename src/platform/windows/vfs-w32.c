/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#include <mgba-util/string.h>
#include <strsafe.h>

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
	WIN32_FIND_DATAW ffData;
	char* utf8Name;
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
	wchar_t name[MAX_PATH + 1];
	MultiByteToWideChar(CP_UTF8, 0, path, -1, name, MAX_PATH);
	StringCchCatNW(name, MAX_PATH, L"\\*", 2);
	WIN32_FIND_DATAW ffData;
	HANDLE handle = FindFirstFileW(name, &ffData);
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
	vd->vde.utf8Name = NULL;

	return &vd->d;
}

bool _vdwClose(struct VDir* vd) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	FindClose(vdw->handle);
	free(vdw->path);
	if (vdw->vde.utf8Name) {
		free(vdw->vde.utf8Name);
		vdw->vde.utf8Name = NULL;
	}
	free(vdw);
	return true;
}

void _vdwRewind(struct VDir* vd) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	FindClose(vdw->handle);
	wchar_t name[MAX_PATH + 1];
	MultiByteToWideChar(CP_UTF8, 0, vdw->path, -1, name, MAX_PATH);
	StringCchCatNW(name, MAX_PATH, L"\\*", 2);
	if (vdw->vde.utf8Name) {
		free(vdw->vde.utf8Name);
		vdw->vde.utf8Name = NULL;
	}
	vdw->handle = FindFirstFileW(name, &vdw->vde.ffData);
}

struct VDirEntry* _vdwListNext(struct VDir* vd) {
	struct VDirW32* vdw = (struct VDirW32*) vd;
	if (FindNextFileW(vdw->handle, &vdw->vde.ffData)) {
		if (vdw->vde.utf8Name) {
			free(vdw->vde.utf8Name);
			vdw->vde.utf8Name = NULL;
		}
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
	size_t size = sizeof(char) * (strlen(path) + strlen(dir) + 2);
	char* combined = malloc(size);
	StringCbPrintfA(combined, size, "%s\\%s", dir, path);

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
	size_t size = sizeof(char) * (strlen(path) + strlen(dir) + 2);
	char* combined = malloc(size);
	StringCbPrintfA(combined, size, "%s\\%s", dir, path);

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
	wchar_t dir[MAX_PATH + 1];
	wchar_t pathw[MAX_PATH + 1];
	wchar_t combined[MAX_PATH + 1];
	MultiByteToWideChar(CP_UTF8, 0, vdw->path, -1, dir, MAX_PATH);
	MultiByteToWideChar(CP_UTF8, 0, path, -1, pathw, MAX_PATH);
	StringCchPrintfW(combined, MAX_PATH, L"%ws\\%ws", dir, pathw);

	return DeleteFileW(combined);
}

const char* _vdweName(struct VDirEntry* vde) {
	struct VDirEntryW32* vdwe = (struct VDirEntryW32*) vde;
	if (vdwe->utf8Name) {
		return vdwe->utf8Name;
	}
	size_t len = 4 * wcslen(vdwe->ffData.cFileName);
	vdwe->utf8Name = malloc(len);
	WideCharToMultiByte(CP_UTF8, 0, vdwe->ffData.cFileName, -1, vdwe->utf8Name, len - 1, NULL, NULL);
	return vdwe->utf8Name;
}

static enum VFSType _vdweType(struct VDirEntry* vde) {
	struct VDirEntryW32* vdwe = (struct VDirEntryW32*) vde;
	if (vdwe->ffData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		return VFS_DIRECTORY;
	}
	return VFS_FILE;
}

bool VDirCreate(const char* path) {
	wchar_t wpath[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);
	if (CreateDirectoryW(wpath, NULL)) {
		return true;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return true;
	}
	return false;
}