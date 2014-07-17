#ifndef VFS_H
#define VFS_H

#include "common.h"

#include "memory.h"

struct VFile {
	bool (*close)(struct VFile* vf);
	off_t (*seek)(struct VFile* vf, off_t offset, int whence);
	ssize_t (*read)(struct VFile* vf, void* buffer, size_t size);
	ssize_t (*readline)(struct VFile* vf, char* buffer, size_t size);
	ssize_t (*write)(struct VFile* vf, void* buffer, size_t size);
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

#endif
