#ifndef ARM_DECOMPILER_H
#define ARM_DECOMPILER_H

#include <stdint.h>

union ARMOperand {
	uint8_t reg;
	int32_t immediate;
};

struct ARMMemoryAccess {
	uint8_t baseRegister;
	union ARMOperand offset;
};

enum ARMOperandFormat {
	ARM_OPERAND_NONE =               0x00000000,
	ARM_OPERAND_REGISTER_1 =         0x00000001,
	ARM_OPERAND_IMMEDIATE_1 =        0x00000002,
	ARM_OPERAND_MEMORY_REGISTER_1 =  0x00000005,
	ARM_OPERAND_MEMORY_IMMEDIATE_1 = 0x00000006,
	ARM_OPERAND_MEMORY_OFFSET_1 =    0x00000007,
	ARM_OPERAND_AFFECTED_1 =         0x00000080,
	ARM_OPERAND_MEMORY_POST_INCR_1 = 0x00000097,
	ARM_OPERAND_MEMORY_PRE_INCR_1 =  0x000000A7,
};

enum ThumbMnemonic {
	THUMB_MN_ILL = 0,
	THUMB_MN_ADC,
	THUMB_MN_ADD,
	THUMB_MN_AND,
	THUMB_MN_ASR,
	THUMB_MN_B,
	THUMB_MN_BIC,
	THUMB_MN_BL,
	THUMB_MN_BLH,
	THUMB_MN_BX,
	THUMB_MN_CMN,
	THUMB_MN_CMP,
	THUMB_MN_EOR,
	THUMB_MN_LDMIA,
	THUMB_MN_LDR,
	THUMB_MN_LDRB,
	THUMB_MN_LDRH,
	THUMB_MN_LDRSB,
	THUMB_MN_LDRSH,
	THUMB_MN_LSL,
	THUMB_MN_LSR,
	THUMB_MN_MOV,
	THUMB_MN_MUL,
	THUMB_MN_MVN,
	THUMB_MN_NEG,
	THUMB_MN_ORR,
	THUMB_MN_POP,
	THUMB_MN_PUSH,
	THUMB_MN_ROR,
	THUMB_MN_SBC,
	THUMB_MN_STMIA,
	THUMB_MN_STR,
	THUMB_MN_STRB,
	THUMB_MN_STRH,
	THUMB_MN_SUB,
	THUMB_MN_SWI,
	THUMB_MN_TST
};

struct ThumbInstructionInfo {
	uint16_t opcode;
	enum ThumbMnemonic mnemonic;
	union ARMOperand op1;
	union ARMOperand op2;
	union ARMOperand op3;
	struct ARMMemoryAccess memory;
	int immediateFormat;
	int operandFormat;
	int branches;
	int accessesMemory;
	int accessesHighRegisters;
};

void ARMDecodeThumb(uint16_t opcode, struct ThumbInstructionInfo* info);

#endif
