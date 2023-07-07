/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#include <mgba-util/math.h>
#include <mgba-util/string.h>

#ifdef USE_LIBZIP
#include <zip.h>

struct VDirEntryZip {
	struct VDirEntry d;
	struct zip* z;
	zip_int64_t index;
};

struct VDirZip {
	struct VDir d;
	struct zip* z;
	bool write;
	struct VDirEntryZip dirent;
};

struct VFileZip {
	struct VFile d;
	struct zip* z;
	struct zip_file* zf;
	void* buffer;
	size_t offset;
	size_t bufferSize;
	size_t readSize;
	size_t writeSize;
	size_t fileSize;
	char* name;
	bool write;
};

enum {
	BLOCK_SIZE = 1024
};
#else
#include <minizip/zip.h>
#include <minizip/unzip.h>
#include <mgba-util/memory.h>

struct VDirEntryZip {
	struct VDirEntry d;
	char name[PATH_MAX];
	size_t fileSize;
	unzFile uz;
	zipFile z;
};

struct VDirZip {
	struct VDir d;
	unzFile uz;
	zipFile z;
	struct VDirEntryZip dirent;
	bool atStart;
};

struct VFileZip {
	struct VFile d;
	unzFile uz;
	zipFile z;
	void* buffer;
	size_t bufferSize;
	size_t fileSize;
};
#endif

static bool _vfzClose(struct VFile* vf);
static off_t _vfzSeek(struct VFile* vf, off_t offset, int whence);
static ssize_t _vfzRead(struct VFile* vf, void* buffer, size_t size);
static ssize_t _vfzWrite(struct VFile* vf, const void* buffer, size_t size);
static void* _vfzMap(struct VFile* vf, size_t size, int flags);
static void _vfzUnmap(struct VFile* vf, void* memory, size_t size);
static void _vfzTruncate(struct VFile* vf, size_t size);
static ssize_t _vfzSize(struct VFile* vf);
static bool _vfzSync(struct VFile* vf, void* buffer, size_t size);

static bool _vdzClose(struct VDir* vd);
static void _vdzRewind(struct VDir* vd);
static struct VDirEntry* _vdzListNext(struct VDir* vd);
static struct VFile* _vdzOpenFile(struct VDir* vd, const char* path, int mode);
static struct VDir* _vdzOpenDir(struct VDir* vd, const char* path);
static bool _vdzDeleteFile(struct VDir* vd, const char* path);

static const char* _vdezName(struct VDirEntry* vde);
static enum VFSType _vdezType(struct VDirEntry* vde);

#ifndef USE_LIBZIP
static voidpf _vfmzOpen(voidpf opaque, const char* filename, int mode) {
	UNUSED(opaque);
	int flags = 0;
	switch (mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) {
	case ZLIB_FILEFUNC_MODE_READ:
		flags = O_RDONLY;
		break;
	case ZLIB_FILEFUNC_MODE_WRITE:
		flags = O_WRONLY;
		break;
	case ZLIB_FILEFUNC_MODE_READ | ZLIB_FILEFUNC_MODE_WRITE:
		flags = O_RDWR;
		break;
	}
	if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
		flags |= O_CREAT;
		if (!(mode & ZLIB_FILEFUNC_MODE_EXISTING)) {
			flags |= O_TRUNC;
		}
	}
	return VFileOpen(filename, flags);
}

static uLong _vfmzRead(voidpf opaque, voidpf stream, void* buf, uLong size) {
	UNUSED(opaque);
	struct VFile* vf = stream;
	ssize_t r = vf->read(vf, buf, size);
	if (r < 0) {
		return 0;
	}
	return r;
}

static uLong _vfmzWrite(voidpf opaque, voidpf stream, const void* buf, uLong size) {
	UNUSED(opaque);
	struct VFile* vf = stream;
	ssize_t r = vf->write(vf, buf, size);
	if (r < 0) {
		return 0;
	}
	return r;
}

int _vfmzClose(voidpf opaque, voidpf stream) {
	UNUSED(opaque);
	struct VFile* vf = stream;
	return vf->close(vf);
}

int _vfmzError(voidpf opaque, voidpf stream) {
	UNUSED(opaque);
	struct VFile* vf = stream;
	return vf->seek(vf, 0, SEEK_CUR) < 0;
}

long _vfmzTell(voidpf opaque, voidpf stream) {
	UNUSED(opaque);
	struct VFile* vf = stream;
	return vf->seek(vf, 0, SEEK_CUR);
}

long _vfmzSeek(voidpf opaque, voidpf stream, uLong offset, int origin) {
	UNUSED(opaque);
	struct VFile* vf = stream;
	return vf->seek(vf, offset, origin) < 0;
}
#endif

struct VDir* VDirOpenZip(const char* path, int flags) {
#ifndef USE_LIBZIP
	UNUSED(flags);
	zlib_filefunc_def ops = {
		.zopen_file = _vfmzOpen,
		.zread_file = _vfmzRead,
		.zwrite_file = _vfmzWrite,
		.ztell_file = _vfmzTell,
		.zseek_file = _vfmzSeek,
		.zclose_file = _vfmzClose,
		.zerror_file = _vfmzError,
		.opaque = 0
	};
	unzFile uz = NULL;
	zipFile z = NULL;

	if ((flags & O_ACCMODE) == O_RDWR) {
		return 0; // Read/write not supported
	}
	if ((flags & O_ACCMODE) == O_WRONLY) {
		z = zipOpen2(path, 0, NULL, &ops);
		if (!z) {
			return 0;
		}
	} else {
		uz = unzOpen2(path, &ops);
		if (!uz) {
			return 0;
		}
	}
#else
	int zflags = 0;
	if (flags & O_CREAT) {
		zflags |= ZIP_CREATE;
	}
	if (flags & O_TRUNC) {
		zflags |= ZIP_TRUNCATE;
	}
	if (flags & O_EXCL) {
		zflags |= ZIP_EXCL;
	}
	if (!(flags & O_WRONLY)) {
		zflags |= ZIP_RDONLY;
	}

	struct zip* z = zip_open(path, zflags, 0);
	if (!z) {
		return 0;
	}
#endif
	struct VDirZip* vd = malloc(sizeof(struct VDirZip));

	vd->d.close = _vdzClose;
	vd->d.rewind = _vdzRewind;
	vd->d.listNext = _vdzListNext;
	vd->d.openFile = _vdzOpenFile;
	vd->d.openDir = _vdzOpenDir;
	vd->d.deleteFile = _vdzDeleteFile;
	vd->z = z;

#ifdef USE_LIBZIP
	vd->write = !!(flags & O_WRONLY);
#else
	vd->atStart = true;
	vd->uz = uz;
#endif

	vd->dirent.d.name = _vdezName;
	vd->dirent.d.type = _vdezType;
#ifdef USE_LIBZIP
	vd->dirent.index = -1;
#else
	vd->dirent.uz = uz;
#endif
	vd->dirent.z = z;

	return &vd->d;
}

#ifdef USE_LIBZIP
bool _vfzClose(struct VFile* vf) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	if (vfz->write) {
		zip_source_t* source = zip_source_buffer(vfz->z, vfz->buffer, vfz->writeSize, 1);
		vfz->buffer = NULL;
		if (source && zip_file_add(vfz->z, vfz->name, source, ZIP_FL_OVERWRITE) < 0) {
			zip_source_free(source);
			return false;
		}
		free(vfz->name);
	}
	if (vfz->zf && zip_fclose(vfz->zf) < 0) {
		return false;
	}
	if (vfz->buffer) {
		free(vfz->buffer);
	}
	free(vfz);
	return true;
}

off_t _vfzSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	size_t position;
	switch (whence) {
	case SEEK_SET:
		position = offset;
		break;
	case SEEK_CUR:
		if (offset < 0 && ((vfz->offset < (size_t) -offset) || (offset == INT_MIN))) {
			return -1;
		}
		position = vfz->offset + offset;
		break;
	case SEEK_END:
		if (offset < 0 && ((vfz->fileSize < (size_t) -offset) || (offset == INT_MIN))) {
			return -1;
		}
		position = vfz->fileSize + offset;
		break;
	default:
		return -1;
	}

	if (position <= vfz->offset) {
		vfz->offset = position;
		return position;
	}

	if (position <= vfz->fileSize) {
		ssize_t read = vf->read(vf, 0, position - vfz->offset);
		if (read < 0) {
			return -1;
		}
		return vfz->offset;
	}

	return -1;
}

ssize_t _vfzRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	size_t bytesRead = 0;
	if (!vfz->buffer) {
		vfz->bufferSize = BLOCK_SIZE;
		vfz->buffer = malloc(BLOCK_SIZE);
		if (vfz->readSize) {
			abort();
		}
	}

	while (bytesRead < size) {
		if (vfz->offset < vfz->readSize) {
			size_t diff = vfz->readSize - vfz->offset;
			void* start = &((uint8_t*) vfz->buffer)[vfz->offset];
			if (diff > size - bytesRead) {
				diff = size - bytesRead;
			}
			if (buffer) {
				void* bufferOffset = &((uint8_t*) buffer)[bytesRead];
				memcpy(bufferOffset, start, diff);
			}
			vfz->offset += diff;
			bytesRead += diff;
			if (diff == size) {
				break;
			}
		}
		// offset == readSize
		if (vfz->readSize == vfz->bufferSize) {
			vfz->bufferSize *= 2;
			if (vfz->bufferSize > vfz->fileSize) {
				vfz->bufferSize = vfz->fileSize;
			}
			vfz->buffer = realloc(vfz->buffer, vfz->bufferSize);
		}
		if (vfz->readSize < vfz->bufferSize) {
			void* start = &((uint8_t*) vfz->buffer)[vfz->readSize];
			size_t toRead = vfz->bufferSize - vfz->readSize;
			if (toRead > BLOCK_SIZE) {
				toRead = BLOCK_SIZE;
			}
			ssize_t zipRead = zip_fread(vfz->zf, start, toRead);
			if (zipRead < 0) {
				if (bytesRead == 0) {
					return -1;
				}
				break;
			}
			if (zipRead == 0) {
				break;
			}
			vfz->readSize += zipRead;
		} else {
			break;
		}
	}
	return bytesRead;
}

ssize_t _vfzWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	size_t bytesWritten = 0;
	if (!vfz->buffer) {
		vfz->bufferSize = toPow2(size);
		vfz->buffer = malloc(vfz->bufferSize);
	} else if (size > vfz->bufferSize || size > vfz->bufferSize - vfz->offset) {
		vfz->bufferSize = toPow2(vfz->offset + size);
		vfz->buffer = realloc(vfz->buffer, vfz->bufferSize);
	}

	void* start = &((uint8_t*) vfz->buffer)[vfz->offset];
	if (buffer) {
		memcpy(start, buffer, size);
	} else {
		memset(start, 0, size);
	}
	vfz->offset += size;
	if (vfz->offset > vfz->writeSize) {
		vfz->writeSize = vfz->offset;
	}
	bytesWritten += size;
	return bytesWritten;
}

void* _vfzMap(struct VFile* vf, size_t size, int flags) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	UNUSED(flags);
	if (size > vfz->readSize) {
		vf->read(vf, 0, size - vfz->readSize);
	}
	return vfz->buffer;
}

void _vfzUnmap(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
}

void _vfzTruncate(struct VFile* vf, size_t size) {
	// TODO
	UNUSED(vf);
	UNUSED(size);
}

ssize_t _vfzSize(struct VFile* vf) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	return vfz->fileSize;
}

bool _vdzClose(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	if (zip_close(vdz->z) < 0) {
		return false;
	}
	free(vdz);
	return true;
}

void _vdzRewind(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	vdz->dirent.index = -1;
}

struct VDirEntry* _vdzListNext(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	zip_int64_t maxIndex = zip_get_num_entries(vdz->z, 0);
	if (maxIndex <= vdz->dirent.index + 1) {
		return 0;
	}
	++vdz->dirent.index;
	return &vdz->dirent.d;
}

struct VFile* _vdzOpenFile(struct VDir* vd, const char* path, int mode) {
	UNUSED(mode);
	// TODO: support truncating, appending and creating, and write
	struct VDirZip* vdz = (struct VDirZip*) vd;

	if ((mode & O_ACCMODE) == O_RDWR) {
		// libzip doesn't allow for random access, so read/write is impossible without
		// reading the entire file first. This approach will be supported eventually.
		return 0;
	}

	struct zip_file* zf = NULL;
	struct zip_stat s = {0};
	if ((mode & O_ACCMODE) == O_WRONLY) {
		if (!vdz->write) {
			return 0;
		}
	} else {
		if (zip_stat(vdz->z, path, 0, &s) < 0) {
			return 0;
		}

		zf = zip_fopen(vdz->z, path, 0);
		if (!zf) {
			return 0;
		}
	}

	struct VFileZip* vfz = calloc(1, sizeof(struct VFileZip));
	vfz->zf = zf;
	vfz->z = vdz->z;
	vfz->fileSize = s.size;
	if ((mode & O_ACCMODE) == O_WRONLY) {
		vfz->name = strdup(path);
		vfz->write = true;
	}

	vfz->d.close = _vfzClose;
	vfz->d.seek = _vfzSeek;
	vfz->d.read = _vfzRead;
	vfz->d.readline = VFileReadline;
	vfz->d.write = _vfzWrite;
	vfz->d.map = _vfzMap;
	vfz->d.unmap = _vfzUnmap;
	vfz->d.truncate = _vfzTruncate;
	vfz->d.size = _vfzSize;
	vfz->d.sync = _vfzSync;

	return &vfz->d;
}

struct VDir* _vdzOpenDir(struct VDir* vd, const char* path) {
	UNUSED(vd);
	UNUSED(path);
	return 0;
}

bool _vdzDeleteFile(struct VDir* vd, const char* path) {
	UNUSED(vd);
	UNUSED(path);
	// TODO
	return false;
}

bool _vfzSync(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
	return false;
}

const char* _vdezName(struct VDirEntry* vde) {
	struct VDirEntryZip* vdez = (struct VDirEntryZip*) vde;
	struct zip_stat s;
	if (zip_stat_index(vdez->z, vdez->index, 0, &s) < 0) {
		return 0;
	}
	return s.name;
}

static enum VFSType _vdezType(struct VDirEntry* vde) {
	if (endswith(vde->name(vde), "/")) {
		return VFS_DIRECTORY;
	}
	return VFS_FILE;
}
#else
bool _vfzClose(struct VFile* vf) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	if (vfz->uz) {
		unzCloseCurrentFile(vfz->uz);
	}
	if (vfz->z) {
		zipCloseFileInZip(vfz->z);
	}
	if (vfz->buffer) {
		mappedMemoryFree(vfz->buffer, vfz->bufferSize);
	}
	free(vfz);
	return true;
}

off_t _vfzSeek(struct VFile* vf, off_t offset, int whence) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	if (!vfz->uz) {
		return -1;
	}

	int64_t currentPos = unztell64(vfz->uz);
	int64_t pos;
	switch (whence) {
	case SEEK_SET:
		pos = 0;
		break;
	case SEEK_CUR:
		pos = unztell64(vfz->uz);
		break;
	case SEEK_END:
		pos = vfz->fileSize;
		break;
	default:
		return -1;
	}

	if (pos < 0 || pos + offset < 0) {
		return -1;
	}
	pos += offset;
	if (currentPos > pos) {
		unzCloseCurrentFile(vfz->uz);
		unzOpenCurrentFile(vfz->uz);
		currentPos = 0;
	}
	while (currentPos < pos) {
		char tempBuf[1024];
		ssize_t toRead = sizeof(tempBuf);
		if (toRead > pos - currentPos) {
			toRead = pos - currentPos;
		}
		ssize_t read = vf->read(vf, tempBuf, toRead);
		if (read < toRead) {
			return -1;
		}
		currentPos += read;
	}

	return unztell64(vfz->uz);
}

ssize_t _vfzRead(struct VFile* vf, void* buffer, size_t size) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	return unzReadCurrentFile(vfz->uz, buffer, size);
}

ssize_t _vfzWrite(struct VFile* vf, const void* buffer, size_t size) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	int res = zipWriteInFileInZip(vfz->z, buffer, size);
	if (res != ZIP_OK) {
		return res;
	}
	return size;
}

void* _vfzMap(struct VFile* vf, size_t size, int flags) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	// TODO
	UNUSED(flags);

	off_t pos = vf->seek(vf, 0, SEEK_CUR);
	if (pos < 0) {
		return 0;
	}

	vfz->buffer = anonymousMemoryMap(size);
	if (!vfz->buffer) {
		return 0;
	}

	unzCloseCurrentFile(vfz->uz);
	unzOpenCurrentFile(vfz->uz);
	vf->read(vf, vfz->buffer, size);
	unzCloseCurrentFile(vfz->uz);
	unzOpenCurrentFile(vfz->uz);
	vf->seek(vf, pos, SEEK_SET);

	vfz->bufferSize = size;

	return vfz->buffer;
}

void _vfzUnmap(struct VFile* vf, void* memory, size_t size) {
	struct VFileZip* vfz = (struct VFileZip*) vf;

	if (memory != vfz->buffer) {
		return;
	}

	mappedMemoryFree(vfz->buffer, size);
	vfz->buffer = 0;
}

void _vfzTruncate(struct VFile* vf, size_t size) {
	// TODO
	UNUSED(vf);
	UNUSED(size);
}

ssize_t _vfzSize(struct VFile* vf) {
	struct VFileZip* vfz = (struct VFileZip*) vf;
	return vfz->fileSize;
}

bool _vdzClose(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	if (vdz->uz && unzClose(vdz->uz) < 0) {
		return false;
	}
	if (vdz->z && zipClose(vdz->z, NULL) < 0) {
		return false;
	}
	free(vdz);
	return true;
}

void _vdzRewind(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	vdz->atStart = unzGoToFirstFile(vdz->uz) == UNZ_OK;
}

struct VDirEntry* _vdzListNext(struct VDir* vd) {
	struct VDirZip* vdz = (struct VDirZip*) vd;
	if (!vdz->atStart) {
		if (unzGoToNextFile(vdz->uz) == UNZ_END_OF_LIST_OF_FILE) {
			return 0;
		}
	} else {
		vdz->atStart = false;
	}
	unz_file_info64 info;
	int status = unzGetCurrentFileInfo64(vdz->uz, &info, vdz->dirent.name, sizeof(vdz->dirent.name), 0, 0, 0, 0);
	if (status < 0) {
		return 0;
	}
	vdz->dirent.fileSize = info.uncompressed_size;
	return &vdz->dirent.d;
}

struct VFile* _vdzOpenFile(struct VDir* vd, const char* path, int mode) {
	UNUSED(mode);
	struct VDirZip* vdz = (struct VDirZip*) vd;

	if ((mode & O_ACCMODE) == O_RDWR) {
		// minizip implementation only supports read or write
		return 0;
	}

	unz_file_info64 info = {0};
	if ((mode & O_ACCMODE) == O_RDONLY) {
		if (unzLocateFile(vdz->uz, path, 0) != UNZ_OK) {
			return 0;
		}

		if (unzOpenCurrentFile(vdz->uz) < 0) {
			return 0;
		}

		int status = unzGetCurrentFileInfo64(vdz->uz, &info, 0, 0, 0, 0, 0, 0);
		if (status < 0) {
			return 0;
		}
	} else {
		if (zipOpenNewFileInZip(vdz->z, path, NULL, NULL, 0, NULL, 0, NULL, Z_DEFLATED, 3) < 0) {
			return 0;
		}
	}

	struct VFileZip* vfz = calloc(1, sizeof(struct VFileZip));
	vfz->uz = vdz->uz;
	vfz->z = vdz->z;
	vfz->buffer = 0;
	vfz->bufferSize = 0;
	vfz->fileSize = info.uncompressed_size;

	vfz->d.close = _vfzClose;
	vfz->d.seek = _vfzSeek;
	vfz->d.read = _vfzRead;
	vfz->d.readline = VFileReadline;
	vfz->d.write = _vfzWrite;
	vfz->d.map = _vfzMap;
	vfz->d.unmap = _vfzUnmap;
	vfz->d.truncate = _vfzTruncate;
	vfz->d.size = _vfzSize;
	vfz->d.sync = _vfzSync;

	return &vfz->d;
}

struct VDir* _vdzOpenDir(struct VDir* vd, const char* path) {
	UNUSED(vd);
	UNUSED(path);
	return 0;
}

bool _vdzDeleteFile(struct VDir* vd, const char* path) {
	UNUSED(vd);
	UNUSED(path);
	// TODO
	return false;
}

bool _vfzSync(struct VFile* vf, void* memory, size_t size) {
	UNUSED(vf);
	UNUSED(memory);
	UNUSED(size);
	return false;
}

const char* _vdezName(struct VDirEntry* vde) {
	struct VDirEntryZip* vdez = (struct VDirEntryZip*) vde;
	return vdez->name;
}

static enum VFSType _vdezType(struct VDirEntry* vde) {
	struct VDirEntryZip* vdez = (struct VDirEntryZip*) vde;
	if (endswith(vdez->name, "/")) {
		return VFS_DIRECTORY;
	}
	return VFS_FILE;
}
#endif
