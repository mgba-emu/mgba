#ifndef ISA_ARM_H
#define ISA_ARM_H

#include <stdint.h>

#define ARM_PREFETCH_CYCLES (1 + cpu->memory->activePrefetchCycles32)

struct ARMCore;

typedef void (*ARMInstruction)(struct ARMCore*, uint32_t opcode);
const ARMInstruction _armTable[0x1000];


#endif
