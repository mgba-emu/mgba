/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_SYMBOLS_H
#define GB_SYMBOLS_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct mDebuggerSymbols;
struct VFile;
void GBLoadSymbols(struct mDebuggerSymbols*, struct VFile* vf);

CXX_GUARD_END

#endif
