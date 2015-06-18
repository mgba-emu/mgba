/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef N3DS_VFS_H
#define N3DS_VFS_H

#include "util/vfs.h"

#define asm __asm__

#include <3ds.h>

struct VFile* VFileOpen3DS(FS_archive* archive, const char* path, int flags);

#endif
