#ifndef ARM_DECOMPILER_H
#define ARM_DECOMPILER_H

#include <stdint.h>

struct ThumbInstructionInfo {
	uint16_t opcode;
};

void ARMDecodeThumb(uint16_t opcode, struct ThumbInstructionInfo* info);

#endif
