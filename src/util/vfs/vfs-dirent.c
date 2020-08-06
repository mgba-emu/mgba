/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#include <mgba-util/string.h>

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

static bool _vdClose(struct VDir* vd);
static void _vdRewind(struct VDir* vd);
static struct VDirEntry* _vdListNext(struct VDir* vd);
static struct VFile* _vdOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vdOpenDir(struct VDir* vd, const char* path);
static bool _vdDeleteFile(struct VDir* vd, const char* path);

static const char* _vdeName(struct VDirEntry* vde);
static enum VFSType _vdeType(struct VDirEntry* vde);

struct VDirDE;
struct VDirEntryDE {
	struct VDirEntry d;
	struct VDirDE* p;
	struct dirent* ent;
};

struct VDirDE {
	struct VDir d;
	DIR* de;
	struct VDirEntryDE vde;
	char* path;
};

struct VDir* VDirOpen(const char* path) {
#ifdef __wii__
	if (!path || !path[0]) {
		return VDeviceList();
	}
#endif
	DIR* de = opendir(path);
	if (!de) {
		return 0;
	}

	struct VDirDE* vd = malloc(sizeof(struct VDirDE));
	if (!vd) {
		closedir(de);
		return 0;
	}

	vd->d.close = _vdClose;
	vd->d.rewind = _vdRewind;
	vd->d.listNext = _vdListNext;
	vd->d.openFile = _vdOpenFile;
	vd->d.openDir = _vdOpenDir;
	vd->d.deleteFile = _vdDeleteFile;
	vd->path = strdup(path);
	vd->de = de;

	vd->vde.d.name = _vdeName;
	vd->vde.d.type = _vdeType;
	vd->vde.p = vd;

	return &vd->d;
}

bool _vdClose(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	if (closedir(vdde->de) < 0) {
		return false;
	}
	free(vdde->path);
	free(vdde);
	return true;
}

void _vdRewind(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	rewinddir(vdde->de);
}

struct VDirEntry* _vdListNext(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	vdde->vde.ent = readdir(vdde->de);
	if (vdde->vde.ent) {
		return &vdde->vde.d;
	}

	return 0;
}

struct VFile* _vdOpenFile(struct VDir* vd, const char* path, int mode) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdde->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	sprintf(combined, "%s%s%s", dir, PATH_SEP, path);

	struct VFile* file = VFileOpen(combined, mode);
	free(combined);
	return file;
}

struct VDir* _vdOpenDir(struct VDir* vd, const char* path) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdde->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	sprintf(combined, "%s%s%s", dir, PATH_SEP, path);

	struct VDir* vd2 = VDirOpen(combined);
	if (!vd2) {
		vd2 = VDirOpenArchive(combined);
	}
	free(combined);
	return vd2;
}

bool _vdDeleteFile(struct VDir* vd, const char* path) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	if (!path) {
		return false;
	}
	const char* dir = vdde->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	sprintf(combined, "%s%s%s", dir, PATH_SEP, path);

	bool ret = !unlink(combined);
	free(combined);
	return ret;
}

const char* _vdeName(struct VDirEntry* vde) {
	struct VDirEntryDE* vdede = (struct VDirEntryDE*) vde;
	if (vdede->ent) {
		return vdede->ent->d_name;
	}
	return 0;
}

static enum VFSType _vdeType(struct VDirEntry* vde) {
	struct VDirEntryDE* vdede = (struct VDirEntryDE*) vde;
#if !defined(WIN32) && !defined(__HAIKU__)
	if (vdede->ent->d_type == DT_DIR) {
		return VFS_DIRECTORY;
	} else if (vdede->ent->d_type == DT_REG) {
		return VFS_FILE;
	}
#endif
	const char* dir = vdede->p->path;
	char* combined = malloc(sizeof(char) * (strlen(vdede->ent->d_name) + strlen(dir) + 2));
	sprintf(combined, "%s%s%s", dir, PATH_SEP, vdede->ent->d_name);
	struct stat sb;
	stat(combined, &sb);
	free(combined);

	if (S_ISDIR(sb.st_mode)) {
		return VFS_DIRECTORY;
	}
	return VFS_FILE;
}

bool VDirCreate(const char* path) {
	return mkdir(path, 0777) == 0 || errno == EEXIST;
}
