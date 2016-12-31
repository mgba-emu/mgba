/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ISA_ARM_H
#define ISA_ARM_H

#include <mgba-util/common.h>

CXX_GUARD_START

#define ARM_PREFETCH_CYCLES (1 + cpu->memory.activeSeqCycles32)

struct ARMCore;

typedef void (*ARMInstruction)(struct ARMCore*, uint32_t opcode);
extern const ARMInstruction _armTable[0x1000];

CXX_GUARD_END

#endif
