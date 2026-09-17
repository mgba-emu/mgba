/* Copyright (c) 2013-2026 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LIBRETRO_VFS_H
#define LIBRETRO_VFS_H

#include <mgba-util/vfs.h>

#include "libretro.h"

void libretroVFSInit(retro_environment_t env);
struct VFile* VFileOpenLibretro(const char* path, int flags);

#endif
