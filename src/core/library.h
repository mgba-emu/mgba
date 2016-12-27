/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef M_LIBRARY_H
#define M_LIBRARY_H

#include "util/common.h"

CXX_GUARD_START

#include "core/core.h"
#include "util/vector.h"

struct mLibraryEntry {
	char* filename;
	char* title;
	char internalTitle[17];
	char internalCode[9];
	size_t filesize;
	enum mPlatform platform;
};

DECLARE_VECTOR(mLibraryListing, struct mLibraryEntry);

struct mLibrary {
	struct mLibraryListing listing;
};

void mLibraryInit(struct mLibrary*);
void mLibraryDeinit(struct mLibrary*);

struct VDir;
struct VFile;
void mLibraryLoadDirectory(struct mLibrary* library, struct VDir* dir);
void mLibraryAddEntry(struct mLibrary* library, const char* filename, struct VFile* vf);

CXX_GUARD_END

#endif
