/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/platform/psp2/sce-vfs.h>

#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>

#include <mgba-util/vfs.h>
#include <mgba-util/memory.h>

#ifndef SCE_CST_SIZE
#define SCE_CST_SIZE 0x0004
#endif

struct VFileSce {
	struct VFile d;

	SceUID fd;
};

struct VDirEntrySce {
	struct VDirEntry d;
	SceIoDirent ent;
};

struct VDirSce {
	struct VDir d;
	struct VDirEntrySce de;
	SceUID fd;
	char* path;
};

static bool _vfsceClose(struct VFile* vf);
static off_t _vfsceSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vfsceRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vfsceWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vfsceMap(struct VFile* vf, size_t size, int flags);
static void _vfsceUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfsceTruncate(struct VFile* vf, size_t size);
static ssize_t _vfsceSize(struct VFile* vf);
static bool _vfsceSync(struct VFile* vf, void* memory, size_t size);

static bool _vdsceClose(struct VDir* vd);
static void _vdsceRewind(struct VDir* vd);
static struct VDirEntry* _vdsceListNext(struct VDir* vd);
static struct VFile* _vdsceOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vdsceOpenDir(struct VDir* vd, const char* path);
static bool _vdsceDeleteFile(struct VDir* vd, const char* path);

static const char* _vdesceName(struct VDirEntry* vde);
static enum VFSType _vdesceType(struct VDirEntry* vde);

static bool _vdlsceClose(struct VDir* vd);
static void _vdlsceRewind(struct VDir* vd);
static struct VDirEntry* _vdlsceListNext(struct VDir* vd);
static struct VFile* _vdlsceOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vdlsceOpenDir(struct VDir* vd, const char* path);
static bool _vdlsceDeleteFile(struct VDir* vd, const char* path);

static const char* _vdlesceName(struct VDirEntry* vde);
static enum VFSType _vdlesceType(struct VDirEntry* vde);

struct VFile* VFileOpenSce(const char* path, int flags, SceMode mode) {
	struct VFileSce* vfsce = malloc(sizeof(struct VFileSce));
	if (!vfsce) {
		return 0;
	}

	vfsce->fd = sceIoOpen(path, flags, mode);
	if (vfsce->fd < 0) {
		free(vfsce);
		return 0;
	}

	vfsce->d.close = _vfsceClose;
	vfsce->d.seek = _vfsceSeek;
	vfsce->d.read = _vfsceRead;
	vfsce->d.readline = VFileReadline;
	vfsce->d.write = _vfsceWrite;
	vfsce->d.map = _vfsceMap;
	vfsce->d.unmap = _vfsceUnmap;
	vfsce->d.truncate = _vfsceTruncate;
	vfsce->d.size = _vfsceSize;
	vfsce->d.sync = _vfsceSync;

	return &vfsce->d;
}

bool _vfsceClose(struct VFile* vf) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	sceIoSyncByFd(vfsce->fd, 0);
	return sceIoClose(vfsce->fd) >= 0;
}

off_t _vfsceSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	return sceIoLseek(vfsce->fd, offset, whence);
}

ssize_t _vfsceRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	return sceIoRead(vfsce->fd, buffer, size);
}

ssize_t _vfsceWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	return sceIoWrite(vfsce->fd, buffer, size);
}

static void* _vfsceMap(struct VFile* vf, size_t size, int flags) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	UNUSED(flags);
	if (!size) {
		return NULL;
	}
	void* buffer = anonymousMemoryMap(size);
	if (buffer) {
		SceOff cur = sceIoLseek(vfsce->fd, 0, SEEK_CUR);
		sceIoLseek(vfsce->fd, 0, SEEK_SET);
		sceIoRead(vfsce->fd, buffer, size);
		sceIoLseek(vfsce->fd, cur, SEEK_SET);
	}
	return buffer;
}

static void _vfsceUnmap(struct VFile* vf, void* memory, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	SceOff cur = sceIoLseek(vfsce->fd, 0, SEEK_CUR);
	sceIoLseek(vfsce->fd, 0, SEEK_SET);
	sceIoWrite(vfsce->fd, memory, size);
	sceIoLseek(vfsce->fd, cur, SEEK_SET);
	sceIoSyncByFd(vfsce->fd, 0);
	mappedMemoryFree(memory, size);
}

static void _vfsceTruncate(struct VFile* vf, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	SceIoStat stat = { .st_size = size };
	sceIoChstatByFd(vfsce->fd, &stat, SCE_CST_SIZE);
}

ssize_t _vfsceSize(struct VFile* vf) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	SceOff cur = sceIoLseek(vfsce->fd, 0, SEEK_CUR);
	SceOff end = sceIoLseek(vfsce->fd, 0, SEEK_END);
	sceIoLseek(vfsce->fd, cur, SEEK_SET);
	return end;
}

bool _vfsceSync(struct VFile* vf, void* buffer, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	if (buffer && size) {
		SceOff cur = sceIoLseek(vfsce->fd, 0, SEEK_CUR);
		sceIoLseek(vfsce->fd, 0, SEEK_SET);
		int res = sceIoWrite(vfsce->fd, buffer, size);
		sceIoLseek(vfsce->fd, cur, SEEK_SET);
		return res == size;
	}
	return sceIoSyncByFd(vfsce->fd, 0) >= 0;
}

struct VDir* VDirOpen(const char* path) {
	if (!path || !path[0]) {
		return VDeviceList();
	}

	SceUID dir = sceIoDopen(path);
	if (dir < 0) {
		return 0;
	}

	struct VDirSce* vd = malloc(sizeof(struct VDirSce));
	vd->fd = dir;
	vd->d.close = _vdsceClose;
	vd->d.rewind = _vdsceRewind;
	vd->d.listNext = _vdsceListNext;
	vd->d.openFile = _vdsceOpenFile;
	vd->d.openDir = _vdsceOpenDir;
	vd->d.deleteFile = _vdsceDeleteFile;
	vd->path = strdup(path);

	vd->de.d.name = _vdesceName;
	vd->de.d.type = _vdesceType;

	return &vd->d;
}

bool _vdsceClose(struct VDir* vd) {
	struct VDirSce* vdsce = (struct VDirSce*) vd;
	if (sceIoDclose(vdsce->fd) < 0) {
		return false;
	}
	free(vdsce->path);
	free(vdsce);
	return true;
}

void _vdsceRewind(struct VDir* vd) {
	struct VDirSce* vdsce = (struct VDirSce*) vd;
	sceIoDclose(vdsce->fd);
	vdsce->fd = sceIoDopen(vdsce->path);
}

struct VDirEntry* _vdsceListNext(struct VDir* vd) {
	struct VDirSce* vdsce = (struct VDirSce*) vd;
	if (sceIoDread(vdsce->fd, &vdsce->de.ent) <= 0) {
		return 0;
	}
	return &vdsce->de.d;
}

struct VFile* _vdsceOpenFile(struct VDir* vd, const char* path, int mode) {
	struct VDirSce* vdsce = (struct VDirSce*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdsce->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + strlen(PATH_SEP) + 1));
	sprintf(combined, "%s%s%s", dir, PATH_SEP, path);

	struct VFile* file = VFileOpen(combined, mode);
	free(combined);
	return file;
}

struct VDir* _vdsceOpenDir(struct VDir* vd, const char* path) {
	struct VDirSce* vdsce = (struct VDirSce*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdsce->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + strlen(PATH_SEP) + 1));
	sprintf(combined, "%s%s%s", dir, PATH_SEP, path);

	struct VDir* vd2 = VDirOpen(combined);
	if (!vd2) {
		vd2 = VDirOpenArchive(combined);
	}
	free(combined);
	return vd2;
}

bool _vdsceDeleteFile(struct VDir* vd, const char* path) {
	struct VDirSce* vdsce = (struct VDirSce*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdsce->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + strlen(PATH_SEP) + 1));
	sprintf(combined, "%s%s%s", dir, PATH_SEP, path);

	bool ret = sceIoRemove(combined) >= 0;
	free(combined);
	return ret;
}

static const char* _vdesceName(struct VDirEntry* vde) {
	struct VDirEntrySce* vdesce = (struct VDirEntrySce*) vde;
	return vdesce->ent.d_name;
}

static enum VFSType _vdesceType(struct VDirEntry* vde) {
	struct VDirEntrySce* vdesce = (struct VDirEntrySce*) vde;
	if (SCE_S_ISDIR(vdesce->ent.d_stat.st_mode)) {
		return VFS_DIRECTORY;
	}
	return VFS_FILE;
}

struct VDirEntrySceDevList {
	struct VDirEntry d;
	ssize_t index;
	const char* name;
};

struct VDirSceDevList {
	struct VDir d;
	struct VDirEntrySceDevList vde;
};

static const char* _devs[] = {
	"ux0:",
	"ur0:",
	"uma0:"
};

struct VDir* VDeviceList() {
	struct VDirSceDevList* vd = malloc(sizeof(struct VDirSceDevList));
	if (!vd) {
		return 0;
	}

	vd->d.close = _vdlsceClose;
	vd->d.rewind = _vdlsceRewind;
	vd->d.listNext = _vdlsceListNext;
	vd->d.openFile = _vdlsceOpenFile;
	vd->d.openDir = _vdlsceOpenDir;
	vd->d.deleteFile = _vdlsceDeleteFile;

	vd->vde.d.name = _vdlesceName;
	vd->vde.d.type = _vdlesceType;
	vd->vde.index = -1;
	vd->vde.name = 0;

	return &vd->d;
}

static bool _vdlsceClose(struct VDir* vd) {
	struct VDirSceDevList* vdl = (struct VDirSceDevList*) vd;
	free(vdl);
	return true;
}

static void _vdlsceRewind(struct VDir* vd) {
	struct VDirSceDevList* vdl = (struct VDirSceDevList*) vd;
	vdl->vde.name = NULL;
	vdl->vde.index = -1;
}

static struct VDirEntry* _vdlsceListNext(struct VDir* vd) {
	struct VDirSceDevList* vdl = (struct VDirSceDevList*) vd;
	while (vdl->vde.index < 3) {
		++vdl->vde.index;
		vdl->vde.name = _devs[vdl->vde.index];
		SceUID dir = sceIoDopen(vdl->vde.name);
		if (dir < 0) {
			continue;
		}
		sceIoDclose(dir);
		return &vdl->vde.d;
	}
	return 0;
}

static struct VFile* _vdlsceOpenFile(struct VDir* vd, const char* path, int mode) {
	UNUSED(vd);
	UNUSED(path);
	UNUSED(mode);
	return NULL;
}

static struct VDir* _vdlsceOpenDir(struct VDir* vd, const char* path) {
	UNUSED(vd);
	return VDirOpen(path);
}

static bool _vdlsceDeleteFile(struct VDir* vd, const char* path) {
	UNUSED(vd);
	UNUSED(path);
	return false;
}

static const char* _vdlesceName(struct VDirEntry* vde) {
	struct VDirEntrySceDevList* vdle = (struct VDirEntrySceDevList*) vde;
	return vdle->name;
}

static enum VFSType _vdlesceType(struct VDirEntry* vde) {
	UNUSED(vde);
	return VFS_DIRECTORY;
}

bool VDirCreate(const char* path) {
	// TODO: Verify vitasdk explanation of return values
	sceIoMkdir(path, 0777);
	return true;
}
