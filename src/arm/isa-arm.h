#ifndef ISA_ARM_H
#define ISA_ARM_H

#include <stdint.h>

struct ARMCore;

void ARMStep(struct ARMCore* cpu);
typedef void (*ARMInstruction)(struct ARMCore*, uint32_t opcode);

#endif
