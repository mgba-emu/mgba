/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef N3DS_VFS_H
#define N3DS_VFS_H

#include <mgba-util/vfs.h>

#include <3ds.h>

extern FS_Archive sdmcArchive;

struct VFile* VFileOpen3DS(FS_Archive* archive, const char* path, int flags);

#endif
