/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/vfs.h"

#include "util/string.h"

#include <dirent.h>

static bool _vdClose(struct VDir* vd);
static void _vdRewind(struct VDir* vd);
static struct VDirEntry* _vdListNext(struct VDir* vd);
static struct VFile* _vdOpenFile(struct VDir* vd, const char* path, int mode);

static const char* _vdeName(struct VDirEntry* vde);

struct VDirEntryDE {
	struct VDirEntry d;
	struct dirent* ent;
};

struct VDirDE {
	struct VDir d;
	DIR* de;
	struct VDirEntryDE vde;
	char* path;
};

struct VDir* VDirOpen(const char* path) {
	DIR* de = opendir(path);
	if (!de) {
		return 0;
	}

	struct VDirDE* vd = malloc(sizeof(struct VDirDE));
	if (!vd) {
		closedir(de);
		return 0;
	}

	vd->d.close = _vdClose;
	vd->d.rewind = _vdRewind;
	vd->d.listNext = _vdListNext;
	vd->d.openFile = _vdOpenFile;
	vd->path = strdup(path);
	vd->de = de;

	vd->vde.d.name = _vdeName;

	return &vd->d;
}

struct VFile* VDirOptionalOpenIncrementFile(struct VDir* dir, const char* realPath, const char* prefix, const char* infix, const char* suffix, int mode) {
	char path[PATH_MAX];
	path[PATH_MAX - 1] = '\0';
	char realPrefix[PATH_MAX];
	realPrefix[PATH_MAX - 1] = '\0';
	if (!dir) {
		if (!realPath) {
			return 0;
		}
		const char* separatorPoint = strrchr(realPath, '/');
		const char* dotPoint;
		size_t len;
		if (!separatorPoint) {
			strcpy(path, "./");
			separatorPoint = realPath;
			dotPoint = strrchr(realPath, '.');
		} else {
			path[0] = '\0';
			dotPoint = strrchr(separatorPoint, '.');

			if (separatorPoint - realPath + 1 >= PATH_MAX - 1) {
				return 0;
			}

			len = separatorPoint - realPath;
			strncat(path, realPath, len);
			path[len] = '\0';
			++separatorPoint;
		}

		if (dotPoint - realPath + 1 >= PATH_MAX - 1) {
			return 0;
		}

		if (dotPoint >= separatorPoint) {
			len = dotPoint - separatorPoint;
		} else {
			len = PATH_MAX - 1;
		}

		strncpy(realPrefix, separatorPoint, len);
		realPrefix[len] = '\0';

		prefix = realPrefix;
		dir = VDirOpen(path);
	}
	if (!dir) {
		// This shouldn't be possible
		return 0;
	}
	dir->rewind(dir);
	struct VDirEntry* dirent;
	size_t prefixLen = strlen(prefix);
	size_t infixLen = strlen(infix);
	unsigned next = 0;
	while ((dirent = dir->listNext(dir))) {
		const char* filename = dirent->name(dirent);
		char* dotPoint = strrchr(filename, '.');
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
		if (strncmp(filename, prefix, prefixLen) == 0) {
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
	snprintf(path, PATH_MAX - 1, "%s%s%u%s", prefix, infix, next, suffix);
	path[PATH_MAX - 1] = '\0';
	return dir->openFile(dir, path, mode);
}

bool _vdClose(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	if (closedir(vdde->de) < 0) {
		return false;
	}
	free(vdde->path);
	free(vdde);
	return true;
}

void _vdRewind(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	rewinddir(vdde->de);
}

struct VDirEntry* _vdListNext(struct VDir* vd) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	vdde->vde.ent = readdir(vdde->de);
	if (vdde->vde.ent) {
		return &vdde->vde.d;
	}

	return 0;
}

struct VFile* _vdOpenFile(struct VDir* vd, const char* path, int mode) {
	struct VDirDE* vdde = (struct VDirDE*) vd;
	if (!path) {
		return 0;
	}
	const char* dir = vdde->path;
	char* combined = malloc(sizeof(char) * (strlen(path) + strlen(dir) + 2));
	sprintf(combined, "%s%s%s", dir, PATH_SEP, path);

	struct VFile* file = VFileOpen(combined, mode);
	free(combined);
	return file;
}

const char* _vdeName(struct VDirEntry* vde) {
	struct VDirEntryDE* vdede = (struct VDirEntryDE*) vde;
	if (vdede->ent) {
		return vdede->ent->d_name;
	}
	return 0;
}

struct VFile* VDirOptionalOpenFile(struct VDir* dir, const char* realPath, const char* prefix, const char* suffix, int mode) {
	char path[PATH_MAX];
	path[PATH_MAX - 1] = '\0';
	struct VFile* vf;
	if (!dir) {
		if (!realPath) {
			return 0;
		}
		char* dotPoint = strrchr(realPath, '.');
		if (dotPoint - realPath + 1 >= PATH_MAX - 1) {
			return 0;
		}
		if (dotPoint > strrchr(realPath, '/')) {
			int len = dotPoint - realPath;
			strncpy(path, realPath, len);
			path[len] = 0;
			strncat(path + len, suffix, PATH_MAX - len - 1);
		} else {
			snprintf(path, PATH_MAX - 1, "%s%s", realPath, suffix);
		}
		vf = VFileOpen(path, mode);
	} else {
		snprintf(path, PATH_MAX - 1, "%s%s", prefix, suffix);
		vf = dir->openFile(dir, path, mode);
	}
	return vf;
}
