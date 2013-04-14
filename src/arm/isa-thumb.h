#ifndef ISA_THUMB_H
#define ISA_THUMB_H

#include <stdint.h>

struct ARMCore;

void ThumbStep(struct ARMCore* cpu);
typedef void (*ThumbInstruction)(struct ARMCore*, uint16_t opcode);

#endif
