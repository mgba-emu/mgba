/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/vfs.h>

#include <mgba-util/string.h>

#ifdef PSP2
#include <mgba-util/platform/psp2/sce-vfs.h>
#endif
#ifdef __3DS__
#include <mgba-util/platform/3ds/3ds-vfs.h>
#endif
#ifdef _WIN32
#include <shlwapi.h>
#include <windows.h>
#endif

struct VFile* VFileOpen(const char* path, int flags) {
#ifdef USE_VFS_FILE
	const char* chflags;
	switch (flags & O_ACCMODE) {
	case O_WRONLY:
		if (flags & O_APPEND) {
			chflags = "ab";
		} else {
			chflags = "wb";
		}
		break;
	case O_RDWR:
		if (flags & O_APPEND) {
			chflags = "a+b";
		} else if (flags & O_TRUNC) {
			chflags = "w+b";
		} else {
			chflags = "r+b";
		}
		break;
	case O_RDONLY:
		chflags = "rb";
		break;
	}
	return VFileFOpen(path, chflags);
#elif defined(PSP2)
	int sceFlags = SCE_O_RDONLY;
	switch (flags & O_ACCMODE) {
	case O_WRONLY:
		sceFlags = SCE_O_WRONLY;
		break;
	case O_RDWR:
		sceFlags = SCE_O_RDWR;
		break;
	case O_RDONLY:
		sceFlags = SCE_O_RDONLY;
		break;
	}

	if (flags & O_APPEND) {
		sceFlags |= SCE_O_APPEND;
	}
	if (flags & O_TRUNC) {
		sceFlags |= SCE_O_TRUNC;
	}
	if (flags & O_CREAT) {
		sceFlags |= SCE_O_CREAT;
	}
	return VFileOpenSce(path, sceFlags, 0666);
#elif defined(USE_VFS_3DS)
	int ctrFlags = FS_OPEN_READ;
	switch (flags & O_ACCMODE) {
	case O_WRONLY:
		ctrFlags = FS_OPEN_WRITE;
		break;
	case O_RDWR:
		ctrFlags = FS_OPEN_READ | FS_OPEN_WRITE;
		break;
	case O_RDONLY:
		ctrFlags = FS_OPEN_READ;
		break;
	}

	if (flags & O_CREAT) {
		ctrFlags |= FS_OPEN_CREATE;
	}
	struct VFile* vf = VFileOpen3DS(&sdmcArchive, path, ctrFlags);
	if (!vf) {
		return 0;
	}
	if (flags & O_TRUNC) {
		vf->truncate(vf, 0);
	}
	if (flags & O_APPEND) {
		vf->seek(vf, vf->size(vf), SEEK_SET);
	}
	return vf;
#else
	return VFileOpenFD(path, flags);
#endif
}

struct VDir* VDirOpenArchive(const char* path) {
	struct VDir* dir = 0;
	UNUSED(path);
#if defined(USE_LIBZIP) || defined(USE_MINIZIP)
	if (!dir) {
		dir = VDirOpenZip(path, O_RDONLY);
	}
#endif
#ifdef USE_LZMA
	if (!dir) {
		dir = VDirOpen7z(path, O_RDONLY);
	}
#endif
	return dir;
}

ssize_t VFileReadline(struct VFile* vf, char* buffer, size_t size) {
	size_t bytesRead = 0;
	while (bytesRead < size - 1) {
		ssize_t newRead = vf->read(vf, &buffer[bytesRead], 1);
		if (newRead <= 0) {
			break;
		}
		bytesRead += newRead;
		if (buffer[bytesRead - newRead] == '\n') {
			break;
		}
	}
	buffer[bytesRead] = '\0';
	return bytesRead;
}

ssize_t VFileWrite32LE(struct VFile* vf, int32_t word) {
	uint32_t leword;
	STORE_32LE(word, 0, &leword);
	return vf->write(vf, &leword, 4);
}

ssize_t VFileWrite16LE(struct VFile* vf, int16_t hword) {
	uint16_t lehword;
	STORE_16LE(hword, 0, &lehword);
	return vf->write(vf, &lehword, 2);
}

ssize_t VFileRead32LE(struct VFile* vf, void* word) {
	uint32_t leword;
	ssize_t r = vf->read(vf, &leword, 4);
	if (r == 4) {
		STORE_32LE(leword, 0, word);
	}
	return r;
}

ssize_t VFileRead16LE(struct VFile* vf, void* hword) {
	uint16_t lehword;
	ssize_t r = vf->read(vf, &lehword, 2);
	if (r == 2) {
		STORE_16LE(lehword, 0, hword);
	}
	return r;
}

void separatePath(const char* path, char* dirname, char* basename, char* extension) {
	if (!path) {
		return;
	}
	char* dotPoint = strrchr(path, '.');
	char* separatorPoint = strnrstr(path, PATH_SEP, strlen(path));
	if (separatorPoint) {
		if (dirname) {
			ptrdiff_t len = separatorPoint - path;
			if (PATH_MAX <= len) {
				len = PATH_MAX - 1;
			} else if (!len) {
				len = 1;
			}
			strncpy(dirname, path, len);
			dirname[len] = '\0';
		}
		path = separatorPoint + 1;
	} else if (dirname) {
		strcpy(dirname, ".");
	}
	if (basename) {
		size_t len;
		if (dotPoint) {
			len = dotPoint - path;
		} else {
			len = strlen(path);
		}
		if (PATH_MAX <= len) {
			len = PATH_MAX - 1;
		}
		strncpy(basename, path, len);
		basename[len] = '\0';
	}
	if (extension) {
		if (dotPoint) {
			++dotPoint;
			size_t len = strlen(dotPoint);
			if (PATH_MAX <= len) {
				len = PATH_MAX - 1;
			}
			strncpy(extension, dotPoint, PATH_MAX - 1);
			extension[len] = '\0';
		} else {
			extension[0] = '\0';
		}
	}
}

bool isAbsolute(const char* path) {
	// XXX: Is this robust?
#ifdef _WIN32
	WCHAR wpath[PATH_MAX];
	MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, PATH_MAX);
	return !PathIsRelativeW(wpath);
#else
	return path[0] == '/';
#endif
}

void makeAbsolute(const char* path, const char* base, char* out) {
	if (isAbsolute(path)) {
		strncpy(out, path, PATH_MAX);
		return;
	}

	char buf[PATH_MAX];
	snprintf(buf, sizeof(buf), "%s" PATH_SEP "%s", base, path);
#ifdef _WIN32
	WCHAR wbuf[PATH_MAX];
	WCHAR wout[PATH_MAX];
	MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, PATH_MAX);
	if (GetFullPathNameW(wbuf, PATH_MAX, wout, NULL)) {
		WideCharToMultiByte(CP_UTF8, 0, wout, -1, out, PATH_MAX, 0, 0);
		return;
	}
#elif defined(HAVE_REALPATH)
	if (realpath(buf, out)) {
		return;
	}
#endif
	strncpy(out, buf, PATH_MAX);
}

struct VFile* VDirFindFirst(struct VDir* dir, bool (*filter)(struct VFile*)) {
	dir->rewind(dir);
	struct VDirEntry* dirent = dir->listNext(dir);
	while (dirent) {
		struct VFile* vf = dir->openFile(dir, dirent->name(dirent), O_RDONLY);
		if (!vf) {
			dirent = dir->listNext(dir);
			continue;
		}
		if (filter(vf)) {
			return vf;
		}
		vf->close(vf);
		dirent = dir->listNext(dir);
	}
	return 0;
}

struct VFile* VDirFindNextAvailable(struct VDir* dir, const char* basename, const char* infix, const char* suffix, int mode) {
	if (!dir) {
		return 0;
	}
	dir->rewind(dir);
	struct VDirEntry* dirent;
	size_t prefixLen = strlen(basename);
	size_t infixLen = strlen(infix);
	char path[PATH_MAX];
	unsigned next = 0;
	while ((dirent = dir->listNext(dir))) {
		const char* filename = dirent->name(dirent);
		const char* dotPoint = strrchr(filename, '.');
		size_t len = strlen(filename);
		if (dotPoint) {
			len = (dotPoint - filename);
		}
		const char* separator = strnrstr(filename, infix, len);
		if (!separator) {
			continue;
		}
		len = separator - filename;
		if (len != prefixLen) {
			continue;
		}
		if (strncmp(filename, basename, prefixLen) == 0) {
			int nlen;
			separator += infixLen;
			snprintf(path, PATH_MAX - 1, "%%u%s%%n", suffix);
			unsigned increment;
			if (sscanf(separator, path, &increment, &nlen) < 1) {
				continue;
			}
			len = strlen(separator);
			if (nlen < (ssize_t) len) {
				continue;
			}
			if (next <= increment) {
				next = increment + 1;
			}
		}
	}
	snprintf(path, PATH_MAX - 1, "%s%s%u%s", basename, infix, next, suffix);
	path[PATH_MAX - 1] = '\0';
	return dir->openFile(dir, path, mode);
}
