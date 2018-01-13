/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/platform/psp2/sce-vfs.h>

#include <psp2/io/dirent.h>

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
static bool _vfsceSync(struct VFile* vf, const void* memory, size_t size);

static bool _vdsceClose(struct VDir* vd);
static void _vdsceRewind(struct VDir* vd);
static struct VDirEntry* _vdsceListNext(struct VDir* vd);
static struct VFile* _vdsceOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vdsceOpenDir(struct VDir* vd, const char* path);
static bool _vdsceDeleteFile(struct VDir* vd, const char* path);

static const char* _vdesceName(struct VDirEntry* vde);
static enum VFSType _vdesceType(struct VDirEntry* vde);

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
	sceIoSyncByFd(vfsce->fd);
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
	sceIoSyncByFd(vfsce->fd);
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

bool _vfsceSync(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileSce* vfsce = (struct VFileSce*) vf;
	if (buffer && size) {
		SceOff cur = sceIoLseek(vfsce->fd, 0, SEEK_CUR);
		sceIoLseek(vfsce->fd, 0, SEEK_SET);
		sceIoWrite(vfsce->fd, buffer, size);
		sceIoLseek(vfsce->fd, cur, SEEK_SET);
	}
	return sceIoSyncByFd(vfsce->fd) >= 0;
}

struct VDir* VDirOpen(const char* path) {
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
