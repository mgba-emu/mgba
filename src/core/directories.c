/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/directories.h>

#include <mgba/core/config.h>
#include <mgba-util/vfs.h>

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
void mDirectorySetInit(struct mDirectorySet* dirs) {
	dirs->base = NULL;
	dirs->archive = NULL;
	dirs->save = NULL;
	dirs->patch = NULL;
	dirs->state = NULL;
	dirs->screenshot = NULL;
	dirs->cheats = NULL;
}

static void mDirectorySetDetachDir(struct mDirectorySet* dirs, struct VDir* dir) {
	if (!dir) {
		return;
	}

	if (dirs->base == dir) {
		dirs->base = NULL;
	}
	if (dirs->archive == dir) {
		dirs->archive = NULL;
	}
	if (dirs->save == dir) {
		dirs->save = NULL;
	}
	if (dirs->patch == dir) {
		dirs->patch = NULL;
	}
	if (dirs->state == dir) {
		dirs->state = NULL;
	}
	if (dirs->screenshot == dir) {
		dirs->screenshot = NULL;
	}
	if (dirs->cheats == dir) {
		dirs->cheats = NULL;
	}

	dir->close(dir);
}

void mDirectorySetDeinit(struct mDirectorySet* dirs) {
	mDirectorySetDetachBase(dirs);
	mDirectorySetDetachDir(dirs, dirs->archive);
	mDirectorySetDetachDir(dirs, dirs->save);
	mDirectorySetDetachDir(dirs, dirs->patch);
	mDirectorySetDetachDir(dirs, dirs->state);
	mDirectorySetDetachDir(dirs, dirs->screenshot);
	mDirectorySetDetachDir(dirs, dirs->cheats);
}

void mDirectorySetAttachBase(struct mDirectorySet* dirs, struct VDir* base) {
	dirs->base = base;
	if (!dirs->save) {
		dirs->save = dirs->base;
	}
	if (!dirs->patch) {
		dirs->patch = dirs->base;
	}
	if (!dirs->state) {
		dirs->state = dirs->base;
	}
	if (!dirs->screenshot) {
		dirs->screenshot = dirs->base;
	}
	if (!dirs->cheats) {
		dirs->cheats = dirs->base;
	}
}

void mDirectorySetDetachBase(struct mDirectorySet* dirs) {
	mDirectorySetDetachDir(dirs, dirs->archive);
	mDirectorySetDetachDir(dirs, dirs->base);
}

struct VFile* mDirectorySetOpenPath(struct mDirectorySet* dirs, const char* path, bool (*filter)(struct VFile*)) {
	struct VDir* archive = VDirOpenArchive(path);
	struct VFile* file;
	if (archive) {
		file = VDirFindFirst(archive, filter);
		if (!file) {
			archive->close(archive);
		} else {
			mDirectorySetDetachDir(dirs, dirs->archive);
			dirs->archive = archive;
		}
	} else {
		file = VFileOpen(path, O_RDONLY);
		if (file && !filter(file)) {
			file->close(file);
			file = 0;
		}
	}
	if (file) {
		char dirname[PATH_MAX];
		separatePath(path, dirname, dirs->baseName, 0);
		mDirectorySetAttachBase(dirs, VDirOpen(dirname));
	}
	return file;
}

struct VFile* mDirectorySetOpenSuffix(struct mDirectorySet* dirs, struct VDir* dir, const char* suffix, int mode) {
	char name[PATH_MAX + 1] = "";
	snprintf(name, sizeof(name) - 1, "%s%s", dirs->baseName, suffix);
	return dir->openFile(dir, name, mode);
}

void mDirectorySetMapOptions(struct mDirectorySet* dirs, const struct mCoreOptions* opts) {
	char abspath[PATH_MAX + 1];
	char configDir[PATH_MAX + 1];
	mCoreConfigDirectory(configDir, sizeof(configDir));

	if (opts->savegamePath) {
		makeAbsolute(opts->savegamePath, configDir, abspath);
		struct VDir* dir = VDirOpen(abspath);
		if (!dir && VDirCreate(abspath)) {
			dir = VDirOpen(abspath);
		}
		if (dir) {
			if (dirs->save && dirs->save != dirs->base) {
				dirs->save->close(dirs->save);
			}
			dirs->save = dir;
		}
	}

	if (opts->savestatePath) {
		makeAbsolute(opts->savestatePath, configDir, abspath);
		struct VDir* dir = VDirOpen(abspath);
		if (!dir && VDirCreate(abspath)) {
			dir = VDirOpen(abspath);
		}
		if (dir) {
			if (dirs->state && dirs->state != dirs->base) {
				dirs->state->close(dirs->state);
			}
			dirs->state = dir;
		}
	}

	if (opts->screenshotPath) {
		makeAbsolute(opts->screenshotPath, configDir, abspath);
		struct VDir* dir = VDirOpen(abspath);
		if (!dir && VDirCreate(abspath)) {
			dir = VDirOpen(abspath);
		}
		if (dir) {
			if (dirs->screenshot && dirs->screenshot != dirs->base) {
				dirs->screenshot->close(dirs->screenshot);
			}
			dirs->screenshot = dir;
		}
	}

	if (opts->patchPath) {
		makeAbsolute(opts->patchPath, configDir, abspath);
		struct VDir* dir = VDirOpen(abspath);
		if (!dir && VDirCreate(abspath)) {
			dir = VDirOpen(abspath);
		}
		if (dir) {
			if (dirs->patch && dirs->patch != dirs->base) {
				dirs->patch->close(dirs->patch);
			}
			dirs->patch = dir;
		}
	}

	if (opts->cheatsPath) {
		makeAbsolute(opts->cheatsPath, configDir, abspath);
		struct VDir* dir = VDirOpen(abspath);
		if (!dir && VDirCreate(abspath)) {
			dir = VDirOpen(abspath);
		}
		if (dir) {
			if (dirs->cheats && dirs->cheats != dirs->base) {
				dirs->cheats->close(dirs->cheats);
			}
			dirs->cheats = dir;
		}
	}
}
#endif
