/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SHARKPORT_H
#define GBA_SHARKPORT_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct GBA;
struct VFile;

bool GBASavedataImportSharkPort(struct GBA* gba, struct VFile* vf, bool testChecksum);
bool GBASavedataExportSharkPort(const struct GBA* gba, struct VFile* vf);

CXX_GUARD_END

#endif
