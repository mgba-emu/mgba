/* Copyright (c) 2013-2026 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "libretro-vfs.h"

#include <fcntl.h>

static struct retro_vfs_interface* _vfs = NULL;
static uint32_t _vfsVersion = 0;

struct VFileRetro {
	struct VFile d;
	// Captured at open; the module-level interface can change while a file is open
	struct retro_vfs_interface* vfs;
	uint32_t vfsVersion;
	struct retro_vfs_file_handle* handle;
	void* mapped;
	size_t mappedSize;
	int mapFlags;
};

void libretroVFSInit(retro_environment_t env) {
	struct retro_vfs_interface_info info = {
		.required_interface_version = 2,
		.iface = NULL,
	};
	if (!env(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &info)) {
		info.required_interface_version = 1;
		info.iface = NULL;
		if (!env(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &info)) {
			// Keep any interface we already have; probe callbacks fail this
			// query on a running core
			return;
		}
	}
	_vfs = info.iface;
	_vfsVersion = info.required_interface_version;
}

static bool _vfrClose(struct VFile* vf) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	if (vfr->mapped) {
		vf->unmap(vf, vfr->mapped, vfr->mappedSize);
	}
	int ret = vfr->vfs->close(vfr->handle);
	free(vfr);
	return ret == 0;
}

static off_t _vfrSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	int position;
	switch (whence) {
	case SEEK_SET:
		position = RETRO_VFS_SEEK_POSITION_START;
		break;
	case SEEK_CUR:
		position = RETRO_VFS_SEEK_POSITION_CURRENT;
		break;
	case SEEK_END:
		position = RETRO_VFS_SEEK_POSITION_END;
		break;
	default:
		return -1;
	}
	return vfr->vfs->seek(vfr->handle, offset, position);
}

static ssize_t _vfrRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	return vfr->vfs->read(vfr->handle, buffer, size);
}

static ssize_t _vfrWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	return vfr->vfs->write(vfr->handle, buffer, size);
}

static void* _vfrMap(struct VFile* vf, size_t size, int flags) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	if (vfr->mapped) {
		return NULL;
	}
	void* buffer = malloc(size);
	if (!buffer) {
		return NULL;
	}
	int64_t position = vfr->vfs->tell(vfr->handle);
	vfr->vfs->seek(vfr->handle, 0, RETRO_VFS_SEEK_POSITION_START);
	int64_t read = vfr->vfs->read(vfr->handle, buffer, size);
	vfr->vfs->seek(vfr->handle, position, RETRO_VFS_SEEK_POSITION_START);
	if (read < 0) {
		free(buffer);
		return NULL;
	}
	if ((size_t) read < size) {
		memset((char*) buffer + read, 0, size - read);
	}
	vfr->mapped = buffer;
	vfr->mappedSize = size;
	vfr->mapFlags = flags;
	return buffer;
}

static void _vfrUnmap(struct VFile* vf, void* memory, size_t size) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	if (memory != vfr->mapped) {
		return;
	}
	if (vfr->mapFlags & MAP_WRITE) {
		int64_t position = vfr->vfs->tell(vfr->handle);
		vfr->vfs->seek(vfr->handle, 0, RETRO_VFS_SEEK_POSITION_START);
		vfr->vfs->write(vfr->handle, memory, size);
		vfr->vfs->seek(vfr->handle, position, RETRO_VFS_SEEK_POSITION_START);
		vfr->vfs->flush(vfr->handle);
	}
	free(memory);
	vfr->mapped = NULL;
	vfr->mappedSize = 0;
}

static void _vfrTruncate(struct VFile* vf, size_t size) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	// truncate is VFS API v2; silently unsupported on v1 frontends
	if (vfr->vfsVersion >= 2 && vfr->vfs->truncate) {
		vfr->vfs->truncate(vfr->handle, size);
	}
}

static ssize_t _vfrSize(struct VFile* vf) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	return vfr->vfs->size(vfr->handle);
}

static bool _vfrSync(struct VFile* vf, void* buffer, size_t size) {
	struct VFileRetro* vfr = (struct VFileRetro*) vf;
	if (buffer && size) {
		int64_t position = vfr->vfs->tell(vfr->handle);
		vfr->vfs->seek(vfr->handle, 0, RETRO_VFS_SEEK_POSITION_START);
		int64_t written = vfr->vfs->write(vfr->handle, buffer, size);
		vfr->vfs->seek(vfr->handle, position, RETRO_VFS_SEEK_POSITION_START);
		if (written < 0 || (size_t) written != size) {
			return false;
		}
	}
	return vfr->vfs->flush(vfr->handle) == 0;
}

struct VFile* VFileOpenLibretro(const char* path, int flags) {
	if (!_vfs || !path) {
		return NULL;
	}
	unsigned mode;
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		mode = RETRO_VFS_FILE_ACCESS_READ;
		break;
	case O_WRONLY:
		mode = RETRO_VFS_FILE_ACCESS_WRITE;
		if (!(flags & O_TRUNC)) {
			mode |= RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
		}
		break;
	case O_RDWR:
		mode = RETRO_VFS_FILE_ACCESS_READ_WRITE;
		if (!(flags & O_TRUNC)) {
			mode |= RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
		}
		break;
	default:
		return NULL;
	}
	struct retro_vfs_file_handle* handle = _vfs->open(path, mode, RETRO_VFS_FILE_ACCESS_HINT_NONE);
	if (!handle && (flags & O_CREAT) && (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING)) {
		// UPDATE_EXISTING can't create; retry creating (file was absent, so nothing is discarded)
		handle = _vfs->open(path, mode & ~RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING, RETRO_VFS_FILE_ACCESS_HINT_NONE);
	}
	if (!handle) {
		return NULL;
	}
	struct VFileRetro* vfr = calloc(1, sizeof(*vfr));
	if (!vfr) {
		_vfs->close(handle);
		return NULL;
	}
	vfr->vfs = _vfs;
	vfr->vfsVersion = _vfsVersion;
	vfr->handle = handle;
	vfr->d.close = _vfrClose;
	vfr->d.seek = _vfrSeek;
	vfr->d.read = _vfrRead;
	vfr->d.readline = VFileReadline;
	vfr->d.write = _vfrWrite;
	vfr->d.map = _vfrMap;
	vfr->d.unmap = _vfrUnmap;
	vfr->d.truncate = _vfrTruncate;
	vfr->d.size = _vfrSize;
	vfr->d.sync = _vfrSync;
	if (flags & O_APPEND) {
		_vfs->seek(handle, 0, RETRO_VFS_SEEK_POSITION_END);
	}
	return &vfr->d;
}
