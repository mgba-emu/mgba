/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ISA_SM83_H
#define ISA_SM83_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct SM83Core;

typedef void (*SM83Instruction)(struct SM83Core*);
extern const SM83Instruction _sm83InstructionTable[0x100];

CXX_GUARD_END

#endif
