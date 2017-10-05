/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CRC32_H
#define CRC32_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct VFile;

#ifndef HAVE_CRC32
uint32_t crc32(uint32_t crc, const void* buf, size_t size);
#else
#include <zlib.h>
#endif

uint32_t doCrc32(const void* buf, size_t size);
uint32_t fileCrc32(struct VFile* file, size_t endOffset);

CXX_GUARD_END

#endif
