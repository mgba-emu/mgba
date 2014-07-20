#ifndef ISA_THUMB_H
#define ISA_THUMB_H

#include "common.h"

struct ARMCore;

typedef void (*ThumbInstruction)(struct ARMCore*, uint16_t opcode);
const ThumbInstruction _thumbTable[0x400];

#endif
