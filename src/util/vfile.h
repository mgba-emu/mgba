#ifndef VFILE_H
#define VFILE_H

#include "common.h"

struct VFile {
	bool (*close)(struct VFile* vf);
	size_t (*seek)(struct VFile* vf, off_t offset, int whence);
	size_t (*read)(struct VFile* vf, void* buffer, size_t size);
	size_t (*readline)(struct VFile* vf, char* buffer, size_t size);
	size_t (*write)(struct VFile* vf, void* buffer, size_t size);
};

struct VFile* VFileOpen(const char* path, int flags);
struct VFile* VFileFromFD(int fd);

#endif
