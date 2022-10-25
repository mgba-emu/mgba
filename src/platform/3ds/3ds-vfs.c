/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/platform/3ds/3ds-vfs.h>

#ifdef USE_VFS_3DS
#include <mgba-util/memory.h>
#include <mgba-util/string.h>

#define MAX_ENT 4

struct VFile3DS {
	struct VFile d;

	Handle handle;
	u64 offset;
};

struct VDirEntry3DS {
	struct VDirEntry d;
	FS_DirectoryEntry ent[MAX_ENT];
	u32 entCount;
	u32 currentEnt;
	char utf8Name[256];
};

struct VDir3DS {
	struct VDir d;

	char* path;
	Handle handle;
	struct VDirEntry3DS vde;
};

static bool _vf3dClose(struct VFile* vf);
static off_t _vf3dSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vf3dRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vf3dWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vf3dMap(struct VFile* vf, size_t size, int flags);
static void _vf3dUnmap(struct VFile* vf, void* memory, size_t size);
static void _vf3dTruncate(struct VFile* vf, size_t size);
static ssize_t _vf3dSize(struct VFile* vf);
static bool _vf3dSync(struct VFile* vf, void* buffer, size_t size);

static bool _vd3dClose(struct VDir* vd);
static void _vd3dRewind(struct VDir* vd);
static struct VDirEntry* _vd3dListNext(struct VDir* vd);
static struct VFile* _vd3dOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vd3dOpenDir(struct VDir* vd, const char* path);
static bool _vd3dDeleteFile(struct VDir* vd, const char* path);

static const char* _vd3deName(struct VDirEntry* vde);
static enum VFSType _vd3deType(struct VDirEntry* vde);

struct VFile* VFileOpen3DS(FS_Archive* archive, const char* path, int flags) {
	struct VFile3DS* vf3d = malloc(sizeof(struct VFile3DS));
	if (!vf3d) {
		return 0;
	}

	uint16_t utf16Path[PATH_MAX + 1];
	ssize_t units = utf8_to_utf16(utf16Path, (const uint8_t*) path, PATH_MAX);
	utf16Path[units] = 0;
	FS_Path newPath = fsMakePath(PATH_UTF16, utf16Path);
	Result res = FSUSER_OpenFile(&vf3d->handle, *archive, newPath, flags, 0);
	if (res & 0xFFFC03FF) {
		free(vf3d);
		return 0;
	}

	vf3d->offset = 0;

	vf3d->d.close = _vf3dClose;
	vf3d->d.seek = _vf3dSeek;
	vf3d->d.read = _vf3dRead;
	vf3d->d.readline = VFileReadline;
	vf3d->d.write = _vf3dWrite;
	vf3d->d.map = _vf3dMap;
	vf3d->d.unmap = _vf3dUnmap;
	vf3d->d.truncate = _vf3dTruncate;
	vf3d->d.size = _vf3dSize;
	vf3d->d.sync = _vf3dSync;

	return &vf3d->d;
}

bool _vf3dClose(struct VFile* vf) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;

	FSFILE_Close(vf3d->handle);
	svcCloseHandle(vf3d->handle);
	return true;
}

off_t _vf3dSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u64 size;
	switch (whence) {
	case SEEK_SET:
		vf3d->offset = offset;
		break;
	case SEEK_END:
		FSFILE_GetSize(vf3d->handle, &size);
		vf3d->offset = size;
		// Fall through
	case SEEK_CUR:
		vf3d->offset += offset;
		break;
	}
	return vf3d->offset;
}

ssize_t _vf3dRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u32 sizeRead;
	Result res = FSFILE_Read(vf3d->handle, &sizeRead, vf3d->offset, buffer, size);
	if (res) {
		return -1;
	}
	vf3d->offset += sizeRead;
	return sizeRead;
}

ssize_t _vf3dWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u32 sizeWritten;
	Result res = FSFILE_Write(vf3d->handle, &sizeWritten, vf3d->offset, buffer, size, FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
	if (res) {
		return -1;
	}
	vf3d->offset += sizeWritten;
	return sizeWritten;
}

static void* _vf3dMap(struct VFile* vf, size_t size, int flags) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	UNUSED(flags);
	if (!size) {
		return NULL;
	}
	void* buffer = anonymousMemoryMap(size);
	if (buffer) {
		u32 sizeRead;
		FSFILE_Read(vf3d->handle, &sizeRead, 0, buffer, size);
	}
	return buffer;
}

static void _vf3dUnmap(struct VFile* vf, void* memory, size_t size) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u32 sizeWritten;
	FSFILE_Write(vf3d->handle, &sizeWritten, 0, memory, size, FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
	mappedMemoryFree(memory, size);
}

static void _vf3dTruncate(struct VFile* vf, size_t size) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	FSFILE_SetSize(vf3d->handle, size);
}

ssize_t _vf3dSize(struct VFile* vf) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	u64 size;
	FSFILE_GetSize(vf3d->handle, &size);
	return size;
}

static bool _vf3dSync(struct VFile* vf, void* buffer, size_t size) {
	struct VFile3DS* vf3d = (struct VFile3DS*) vf;
	if (buffer) {
		u32 sizeWritten;
		Result res = FSFILE_Write(vf3d->handle, &sizeWritten, 0, buffer, size, FS_WRITE_FLUSH | FS_WRITE_UPDATE_TIME);
		return R_SUCCEEDED(res);
	}
	FSFILE_Flush(vf3d->handle);
	return true;
}

struct VDir* VDirOpen(const char* path) {
	struct VDir3DS* vd3d = malloc(sizeof(struct VDir3DS));
	if (!vd3d) {
		return 0;
	}

	uint16_t utf16Path[PATH_MAX + 1];
	ssize_t units = utf8_to_utf16(utf16Path, (const uint8_t*) path, PATH_MAX);
	utf16Path[units] = 0;
	FS_Path newPath = fsMakePath(PATH_UTF16, utf16Path);
	Result res = FSUSER_OpenDirectory(&vd3d->handle, sdmcArchive, newPath);
	if (res & 0xFFFC03FF) {
		free(vd3d);
		return 0;
	}

	vd3d->path = strdup(path);

	vd3d->d.close = _vd3dClose;
	vd3d->d.rewind = _vd3dRewind;
	vd3d->d.listNext = _vd3dListNext;
	vd3d->d.openFile = _vd3dOpenFile;
	vd3d->d.openDir = _vd3dOpenDir;
	vd3d->d.deleteFile = _vd3dDeleteFile;

	vd3d->vde.d.name = _vd3deName;
	vd3d->vde.d.type = _vd3deType;
	vd3d->vde.entCount = 0;

	return &vd3d->d;
}

static bool _vd3dClose(struct VDir* vd) {
	struct VDir3DS* vd3d = (struct VDir3DS*) vd;
	FSDIR_Close(vd3d->handle);
	free(vd3d->path);
	free(vd3d);
	return true;
}

static void _vd3dRewind(struct VDir* vd) {
	struct VDir3DS* vd3d = (struct VDir3DS*) vd;
	FSDIR_Close(vd3d->handle);
	uint16_t utf16Path[PATH_MAX + 1];
	ssize_t units = utf8_to_utf16(utf16Path, (const uint8_t*) vd3d->path, PATH_MAX);
	utf16Path[units] = 0;
	FS_Path newPath = fsMakePath(PATH_UTF16, utf16Path);
	FSUSER_OpenDirectory(&vd3d->handle, sdmcArchive, newPath);
}

static struct VDirEntry* _vd3dListNext(struct VDir* vd) {
	struct VDir3DS* vd3d = (struct VDir3DS*) vd;
	memset(vd3d->vde.utf8Name, 0, sizeof(vd3d->vde.utf8Name));
	if (!vd3d->vde.entCount || vd3d->vde.currentEnt + 1 >= vd3d->vde.entCount) {
		memset(&vd3d->vde.ent, 0, sizeof(vd3d->vde.ent));
		FSDIR_Read(vd3d->handle, &vd3d->vde.entCount, MAX_ENT, vd3d->vde.ent);
		vd3d->vde.currentEnt = 0;
	} else {
		++vd3d->vde.currentEnt;
	}
	if (!vd3d->vde.entCount) {
		return NULL;
	}
	return &vd3d->vde.d;
}

static struct VFile* _vd3dOpenFile(struct VDir* vd, const char* path, int mode) {
	struct VDir3DS* vd3d = (struct VDir3DS*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vd3d->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	if (dir[strlen(dir) - 1] == '/') {
		sprintf(combined, "%s%s", dir, path);
	} else {
		sprintf(combined, "%s/%s", dir, path);
	}

	struct VFile* file = VFileOpen(combined, mode);
	free(combined);
	return file;
}

static struct VDir* _vd3dOpenDir(struct VDir* vd, const char* path) {
	struct VDir3DS* vd3d = (struct VDir3DS*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vd3d->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	if (dir[strlen(dir) - 1] == '/') {
		sprintf(combined, "%s%s", dir, path);
	} else {
		sprintf(combined, "%s/%s", dir, path);
	}

	struct VDir* vd2 = VDirOpen(combined);
	if (!vd2) {
		vd2 = VDirOpenArchive(combined);
	}
	free(combined);
	return vd2;
}

static bool _vd3dDeleteFile(struct VDir* vd, const char* path) {
	struct VDir3DS* vd3d = (struct VDir3DS*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vd3d->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	if (dir[strlen(dir) - 1] == '/') {
		sprintf(combined, "%s%s", dir, path);
	} else {
		sprintf(combined, "%s/%s", dir, path);
	}

	uint16_t utf16Path[PATH_MAX + 1];
	ssize_t units = utf8_to_utf16(utf16Path, (const uint8_t*) combined, PATH_MAX);
	utf16Path[units] = 0;
	FS_Path newPath = fsMakePath(PATH_UTF16, utf16Path);
	bool ret = !FSUSER_DeleteFile(sdmcArchive, newPath);
	free(combined);
	return ret;
}

static const char* _vd3deName(struct VDirEntry* vde) {
	struct VDirEntry3DS* vd3de = (struct VDirEntry3DS*) vde;
	if (!vd3de->utf8Name[0]) {
		utf16_to_utf8((uint8_t*) vd3de->utf8Name, vd3de->ent[vd3de->currentEnt].name, sizeof(vd3de->utf8Name));
	}
	return vd3de->utf8Name;
}

static enum VFSType _vd3deType(struct VDirEntry* vde) {
	struct VDirEntry3DS* vd3de = (struct VDirEntry3DS*) vde;
	if (vd3de->ent[vd3de->currentEnt].attributes & FS_ATTRIBUTE_DIRECTORY) {
		return VFS_DIRECTORY;
	}
	return VFS_FILE;
}

bool VDirCreate(const char* path) {
	Result rc = FSUSER_CreateDirectory(sdmcArchive, fsMakePath(PATH_ASCII, path), 0);
	return R_SUCCEEDED(rc) || rc == 0xC82044BE;
}
#endif
