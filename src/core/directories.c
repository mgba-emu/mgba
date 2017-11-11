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
	dirs->base = 0;
	dirs->archive = 0;
	dirs->save = 0;
	dirs->patch = 0;
	dirs->state = 0;
	dirs->screenshot = 0;
	dirs->cheats = 0;
}

void mDirectorySetDeinit(struct mDirectorySet* dirs) {
	mDirectorySetDetachBase(dirs);

	if (dirs->archive) {
		if (dirs->archive == dirs->save) {
			dirs->save = NULL;
		}
		if (dirs->archive == dirs->patch) {
			dirs->patch = NULL;
		}
		if (dirs->archive == dirs->state) {
			dirs->state = NULL;
		}
		if (dirs->archive == dirs->screenshot) {
			dirs->screenshot = NULL;
		}
		if (dirs->archive == dirs->cheats) {
			dirs->cheats = NULL;
		}
		dirs->archive->close(dirs->archive);
		dirs->archive = NULL;
	}

	if (dirs->save) {
		if (dirs->save == dirs->patch) {
			dirs->patch = NULL;
		}
		if (dirs->save == dirs->state) {
			dirs->state = NULL;
		}
		if (dirs->save == dirs->screenshot) {
			dirs->screenshot = NULL;
		}
		if (dirs->save == dirs->cheats) {
			dirs->cheats = NULL;
		}
		dirs->save->close(dirs->save);
		dirs->save = NULL;
	}

	if (dirs->patch) {
		if (dirs->patch == dirs->state) {
			dirs->state = NULL;
		}
		if (dirs->patch == dirs->screenshot) {
			dirs->screenshot = NULL;
		}
		if (dirs->patch == dirs->cheats) {
			dirs->cheats = NULL;
		}
		dirs->patch->close(dirs->patch);
		dirs->patch = NULL;
	}

	if (dirs->state) {
		if (dirs->state == dirs->screenshot) {
			dirs->state = NULL;
		}
		if (dirs->state == dirs->cheats) {
			dirs->cheats = NULL;
		}
		dirs->state->close(dirs->state);
		dirs->state = NULL;
	}

	if (dirs->screenshot) {
		if (dirs->screenshot == dirs->cheats) {
			dirs->cheats = NULL;
		}
		dirs->screenshot->close(dirs->screenshot);
		dirs->screenshot = NULL;
	}

	if (dirs->cheats) {
		dirs->cheats->close(dirs->cheats);
		dirs->cheats = NULL;
	}
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
	if (dirs->save == dirs->base) {
		dirs->save = NULL;
	}
	if (dirs->patch == dirs->base) {
		dirs->patch = NULL;
	}
	if (dirs->state == dirs->base) {
		dirs->state = NULL;
	}
	if (dirs->screenshot == dirs->base) {
		dirs->screenshot = NULL;
	}
	if (dirs->cheats == dirs->base) {
		dirs->cheats = NULL;
	}

	if (dirs->base) {
		dirs->base->close(dirs->base);
		dirs->base = NULL;
	}
}

struct VFile* mDirectorySetOpenPath(struct mDirectorySet* dirs, const char* path, bool (*filter)(struct VFile*)) {
	dirs->archive = VDirOpenArchive(path);
	struct VFile* file;
	if (dirs->archive) {
		file = VDirFindFirst(dirs->archive, filter);
		if (!file) {
			dirs->archive->close(dirs->archive);
			dirs->archive = 0;
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
	if (opts->savegamePath) {
		struct VDir* dir = VDirOpen(opts->savegamePath);
		if (dir) {
			if (dirs->save && dirs->save != dirs->base) {
				dirs->save->close(dirs->save);
			}
			dirs->save = dir;
		}
	}

	if (opts->savestatePath) {
		struct VDir* dir = VDirOpen(opts->savestatePath);
		if (dir) {
			if (dirs->state && dirs->state != dirs->base) {
				dirs->state->close(dirs->state);
			}
			dirs->state = dir;
		}
	}

	if (opts->screenshotPath) {
		struct VDir* dir = VDirOpen(opts->screenshotPath);
		if (dir) {
			if (dirs->screenshot && dirs->screenshot != dirs->base) {
				dirs->screenshot->close(dirs->screenshot);
			}
			dirs->screenshot = dir;
		}
	}

	if (opts->patchPath) {
		struct VDir* dir = VDirOpen(opts->patchPath);
		if (dir) {
			if (dirs->patch && dirs->patch != dirs->base) {
				dirs->patch->close(dirs->patch);
			}
			dirs->patch = dir;
		}
	}

	if (opts->cheatsPath) {
		struct VDir* dir = VDirOpen(opts->cheatsPath);
		if (dir) {
			if (dirs->cheats && dirs->cheats != dirs->base) {
				dirs->cheats->close(dirs->cheats);
			}
			dirs->cheats = dir;
		}
	}
}
#endif
