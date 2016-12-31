/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SCE_VFS_H
#define SCE_VFS_H

#ifdef PSP2
#include <psp2/types.h>
#include <psp2/io/fcntl.h>
#else
#include <pspiofilemgr.h>
#endif

struct VFile* VFileOpenSce(const char* path, int flags, SceMode mode);

#endif
