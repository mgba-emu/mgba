/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ISA_THUMB_H
#define ISA_THUMB_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct ARMCore;

typedef void (*ThumbInstruction)(struct ARMCore*, unsigned opcode);
extern const ThumbInstruction _thumbTable[0x400];

CXX_GUARD_END

#endif
