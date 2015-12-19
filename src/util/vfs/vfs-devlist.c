/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/vfs.h"

#include <fat.h>

static bool _vdlClose(struct VDir* vd);
static void _vdlRewind(struct VDir* vd);
static struct VDirEntry* _vdlListNext(struct VDir* vd);
static struct VFile* _vdlOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vdlOpenDir(struct VDir* vd, const char* path);

static const char* _vdleName(struct VDirEntry* vde);
static enum VFSType _vdleType(struct VDirEntry* vde);

extern const struct {
	const char* name; 
	const DISC_INTERFACE* (*getInterface)(void);
} _FAT_disc_interfaces[];

struct VDirEntryDevList {
	struct VDirEntry d;
	size_t index;
	char* name;
};

struct VDirDevList {
	struct VDir d;
	struct VDirEntryDevList vde;
};

struct VDir* VDeviceList() {
	struct VDirDevList* vd = malloc(sizeof(struct VDirDevList));
	if (!vd) {
		return 0;
	}

	vd->d.close = _vdlClose;
	vd->d.rewind = _vdlRewind;
	vd->d.listNext = _vdlListNext;
	vd->d.openFile = _vdlOpenFile;
	vd->d.openDir = _vdlOpenDir;

	vd->vde.d.name = _vdleName;
	vd->vde.d.type = _vdleType;
	vd->vde.index = 0;
	vd->vde.name = 0;

	return &vd->d;
}

static bool _vdlClose(struct VDir* vd) {
	struct VDirDevList* vdl = (struct VDirDevList*) vd;
	free(vdl->vde.name);
	free(vdl);
	return true;
}

static void _vdlRewind(struct VDir* vd) {
	struct VDirDevList* vdl = (struct VDirDevList*) vd;
	free(vdl->vde.name);
	vdl->vde.name = 0;
	vdl->vde.index = 0;
}

static struct VDirEntry* _vdlListNext(struct VDir* vd) {
	struct VDirDevList* vdl = (struct VDirDevList*) vd;
	if (vdl->vde.name) {
		++vdl->vde.index;
		free(vdl->vde.name);
		vdl->vde.name = 0;
	}
	while (true) {
		if (!_FAT_disc_interfaces[vdl->vde.index].name || !_FAT_disc_interfaces[vdl->vde.index].getInterface) {
			return 0;
		}
		const DISC_INTERFACE* iface = _FAT_disc_interfaces[vdl->vde.index].getInterface();
		if (iface && iface->isInserted()) {
			vdl->vde.name = malloc(strlen(_FAT_disc_interfaces[vdl->vde.index].name) + 3);
			sprintf(vdl->vde.name, "%s:", _FAT_disc_interfaces[vdl->vde.index].name);
			return &vdl->vde.d;
		}

		++vdl->vde.index;
	}
}

static struct VFile* _vdlOpenFile(struct VDir* vd, const char* path, int mode) {
	UNUSED(vd);
	UNUSED(path);
	UNUSED(mode);
	return 0;
}

static struct VDir* _vdlOpenDir(struct VDir* vd, const char* path) {
	UNUSED(vd);
	return VDirOpen(path);
}

static const char* _vdleName(struct VDirEntry* vde) {
	struct VDirEntryDevList* vdle = (struct VDirEntryDevList*) vde;
	return vdle->name;
}

static enum VFSType _vdleType(struct VDirEntry* vde) {
	UNUSED(vde);
	return VFS_DIRECTORY;
}
