/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VFS_H
#define VFS_H

#include "util/common.h"

enum {
	MAP_READ = 1,
	MAP_WRITE = 2
};

struct VFile {
	bool (*close)(struct VFile* vf);
	off_t (*seek)(struct VFile* vf, off_t offset, int whence);
	ssize_t (*read)(struct VFile* vf, void* buffer, size_t size);
	ssize_t (*readline)(struct VFile* vf, char* buffer, size_t size);
	ssize_t (*write)(struct VFile* vf, const void* buffer, size_t size);
	void* (*map)(struct VFile* vf, size_t size, int flags);
	void (*unmap)(struct VFile* vf, void* memory, size_t size);
	void (*truncate)(struct VFile* vf, size_t size);
};

struct VDirEntry {
	const char* (*name)(struct VDirEntry* vde);
};

struct VDir {
	bool (*close)(struct VDir* vd);
	void (*rewind)(struct VDir* vd);
	struct VDirEntry* (*listNext)(struct VDir* vd);
	struct VFile* (*openFile)(struct VDir* vd, const char* name, int mode);
};

struct VFile* VFileOpen(const char* path, int flags);
struct VFile* VFileFromFD(int fd);

struct VDir* VDirOpen(const char* path);

#ifdef ENABLE_LIBZIP
struct VDir* VDirOpenZip(const char* path, int flags);
#endif

struct VFile* VDirOptionalOpenFile(struct VDir* dir, const char* realPath, const char* prefix, const char* suffix, int mode);
struct VFile* VDirOptionalOpenIncrementFile(struct VDir* dir, const char* realPath, const char* prefix, const char* infix, const char* suffix, int mode);

#endif
