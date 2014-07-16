#ifndef VFILE_H
#define VFILE_H

#include "common.h"

#include "memory.h"

struct VFile {
	bool (*close)(struct VFile* vf);
	size_t (*seek)(struct VFile* vf, off_t offset, int whence);
	size_t (*read)(struct VFile* vf, void* buffer, size_t size);
	size_t (*readline)(struct VFile* vf, char* buffer, size_t size);
	size_t (*write)(struct VFile* vf, void* buffer, size_t size);
	void* (*map)(struct VFile* vf, size_t size, int flags);
	void (*unmap)(struct VFile* vf, void* memory, size_t size);
	void (*truncate)(struct VFile* vf, size_t size);
};

struct VFile* VFileOpen(const char* path, int flags);
struct VFile* VFileFromFD(int fd);

#endif
