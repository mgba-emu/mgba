/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/library.h>

#include <mgba-util/vfs.h>

DEFINE_VECTOR(mLibraryListing, struct mLibraryEntry);

void mLibraryInit(struct mLibrary* library) {
	mLibraryListingInit(&library->listing, 0);
}

void mLibraryDeinit(struct mLibrary* library) {
	size_t i;
	for (i = 0; i < mLibraryListingSize(&library->listing); ++i) {
		struct mLibraryEntry* entry = mLibraryListingGetPointer(&library->listing, i);
		free(entry->filename);
		free(entry->title);
	}
	mLibraryListingDeinit(&library->listing);
}

void mLibraryLoadDirectory(struct mLibrary* library, struct VDir* dir) {
	struct VDirEntry* dirent = dir->listNext(dir);
	while (dirent) {
		struct VFile* vf = dir->openFile(dir, dirent->name(dirent), O_RDONLY);
		if (!vf) {
			dirent = dir->listNext(dir);
			continue;
		}
		mLibraryAddEntry(library, dirent->name(dirent), vf);
		dirent = dir->listNext(dir);
	}
}

void mLibraryAddEntry(struct mLibrary* library, const char* filename, struct VFile* vf) {
	struct mCore* core;
	if (!vf) {
		vf = VFileOpen(filename, O_RDONLY);
	}
	if (!vf) {
		return;
	}
	core = mCoreFindVF(vf);
	if (core) {
		core->init(core);
		core->loadROM(core, vf);

		struct mLibraryEntry* entry = mLibraryListingAppend(&library->listing);
		core->getGameTitle(core, entry->internalTitle);
		core->getGameCode(core, entry->internalCode);
		entry->title = NULL;
		entry->filename = strdup(filename);
		entry->filesize = vf->size(vf);
		// Note: this destroys the VFile
		core->deinit(core);
	} else {
		vf->close(vf);
	}
}
