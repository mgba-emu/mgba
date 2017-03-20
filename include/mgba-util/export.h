/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef EXPORT_H
#define EXPORT_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct VFile;

bool exportPaletteRIFF(struct VFile* vf, size_t entries, const uint16_t* colors);
bool exportPaletteACT(struct VFile* vf, size_t entries, const uint16_t* colors);

CXX_GUARD_END

#endif
