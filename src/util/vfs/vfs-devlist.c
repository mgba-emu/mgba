/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#include <sys/iosupport.h>

static bool _vdlClose(struct VDir* vd);
static void _vdlRewind(struct VDir* vd);
static struct VDirEntry* _vdlListNext(struct VDir* vd);
static struct VFile* _vdlOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vdlOpenDir(struct VDir* vd, const char* path);
static bool _vdlDeleteFile(struct VDir* vd, const char* path);

static const char* _vdleName(struct VDirEntry* vde);
static enum VFSType _vdleType(struct VDirEntry* vde);

struct VDirEntryDevList {
	struct VDirEntry d;
	size_t index;
	char name[PATH_MAX + 1];
};

struct VDirDevList {
	struct VDir d;
	struct VDirEntryDevList vde;
};

struct VDir* VDeviceList() {
	struct VDirDevList* vd = malloc(sizeof(struct VDirDevList));
	if (!vd) {
		return NULL;
	}

	vd->d.close = _vdlClose;
	vd->d.rewind = _vdlRewind;
	vd->d.listNext = _vdlListNext;
	vd->d.openFile = _vdlOpenFile;
	vd->d.openDir = _vdlOpenDir;
	vd->d.deleteFile = _vdlDeleteFile;

	vd->vde.d.name = _vdleName;
	vd->vde.d.type = _vdleType;
	vd->vde.index = 0;
	vd->vde.name[0] = '\0';

	return &vd->d;
}

static bool _vdlClose(struct VDir* vd) {
	struct VDirDevList* vdl = (struct VDirDevList*) vd;
	free(vdl);
	return true;
}

static void _vdlRewind(struct VDir* vd) {
	struct VDirDevList* vdl = (struct VDirDevList*) vd;
	vdl->vde.name[0] = '\0';
	vdl->vde.index = 0;
}

static struct VDirEntry* _vdlListNext(struct VDir* vd) {
	struct VDirDevList* vdl = (struct VDirDevList*) vd;
	while (vdl->vde.index < STD_MAX) {
		const devoptab_t *devops = devoptab_list[vdl->vde.index];
		++vdl->vde.index;
		if (!devops || !devops->name || devops->dirStateSize <= 0) {
			continue;
		}
		snprintf(vdl->vde.name, sizeof(vdl->vde.name), "%s:", devops->name);
		return &vdl->vde.d;
	}
	return NULL;
}

static struct VFile* _vdlOpenFile(struct VDir* vd, const char* path, int mode) {
	UNUSED(vd);
	UNUSED(path);
	UNUSED(mode);
	return NULL;
}

static struct VDir* _vdlOpenDir(struct VDir* vd, const char* path) {
	UNUSED(vd);
	return VDirOpen(path);
}

static bool _vdlDeleteFile(struct VDir* vd, const char* path) {
	UNUSED(vd);
	UNUSED(path);
	return false;
}

static const char* _vdleName(struct VDirEntry* vde) {
	struct VDirEntryDevList* vdle = (struct VDirEntryDevList*) vde;
	return vdle->name;
}

static enum VFSType _vdleType(struct VDirEntry* vde) {
	UNUSED(vde);
	return VFS_DIRECTORY;
}
