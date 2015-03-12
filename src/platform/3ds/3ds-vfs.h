/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "util/vfs.h"

#include <3ds.h>

struct VFile* VFileOpen3DS(FS_archive* archive, const char* path, int flags);
