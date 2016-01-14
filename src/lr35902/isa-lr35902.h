/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ISA_LR35902_H
#define ISA_LR35902_H

#include "util/common.h"

struct LR35902Core;

typedef void (*LR35902Instruction)(struct LR35902Core*);
const LR35902Instruction _lr35902InstructionTable[0x100];
const LR35902Instruction _lr35902CBInstructionTable[0x100];

#endif
