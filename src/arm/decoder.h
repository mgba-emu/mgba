#ifndef ARM_DECODER_H
#define ARM_DECODER_H

#include <stdint.h>

// Bit 0: a register is involved with this operand
// Bit 1: an immediate is invovled with this operand
// Bit 2: a memory access is invovled with this operand
// Bit 3: the destination of this operand is affected by this opcode
// Bit 4: this operand is shifted by a register
// Bit 5: this operand is shifted by an immediate
// Bit 6: this operand is added or subtracted to the base register
enum ARMOperandFormat {
	ARM_OPERAND_NONE =               0x00000000,
	ARM_OPERAND_REGISTER_1 =         0x00000001,
	ARM_OPERAND_IMMEDIATE_1 =        0x00000002,
	ARM_OPERAND_MEMORY_1 =           0x00000004,
	ARM_OPERAND_AFFECTED_1 =         0x00000008,
	ARM_OPERAND_SHIFT_REGISTER_1 =   0x00000010,
	ARM_OPERAND_SHIFT_IMMEDIATE_1 =  0x00000020,
	ARM_OPERAND_1 =                  0x000000FF,

	ARM_OPERAND_REGISTER_2 =         0x00000100,
	ARM_OPERAND_IMMEDIATE_2 =        0x00000200,
	ARM_OPERAND_MEMORY_2 =           0x00000400,
	ARM_OPERAND_AFFECTED_2 =         0x00000800,
	ARM_OPERAND_SHIFT_REGISTER_2 =   0x00001000,
	ARM_OPERAND_SHIFT_IMMEDIATE_2 =  0x00002000,
	ARM_OPERAND_2 =                  0x0000FF00,

	ARM_OPERAND_REGISTER_3 =         0x00010000,
	ARM_OPERAND_IMMEDIATE_3 =        0x00020000,
	ARM_OPERAND_MEMORY_3 =           0x00040000,
	ARM_OPERAND_AFFECTED_3 =         0x00080000,
	ARM_OPERAND_SHIFT_REGISTER_3 =   0x00100000,
	ARM_OPERAND_SHIFT_IMMEDIATE_3 =  0x00200000,
	ARM_OPERAND_3 =                  0x00FF0000
};

enum ARMMemoryFormat {
	ARM_MEMORY_REGISTER_BASE =    0x0001,
	ARM_MEMORY_IMMEDIATE_OFFSET = 0x0002,
	ARM_MEMORY_REGISTER_OFFSET  = 0x0004,
	ARM_MEMORY_SHIFTED_OFFSET =   0x0008,
	ARM_MEMORY_PRE_INCREMENT =    0x0010,
	ARM_MEMORY_POST_INCREMENT =   0x0020,
	ARM_MEMORY_OFFSET_SUBTRACT =  0x0040
};

union ARMOperand {
	struct {
		uint8_t reg;
		uint8_t shifterOp;
		union {
			uint8_t shifterReg;
			uint8_t shifterImm;
		};
	};
	int32_t immediate;
};

struct ARMMemoryAccess {
	uint8_t baseReg;
	uint16_t format;
	union ARMOperand offset;
};

enum ThumbMnemonic {
	THUMB_MN_ILL = 0,
	THUMB_MN_ADC,
	THUMB_MN_ADD,
	THUMB_MN_AND,
	THUMB_MN_ASR,
	THUMB_MN_B,
	THUMB_MN_BIC,
	THUMB_MN_BKPT,
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
	THUMB_MN_TST,

	THUMB_MN_MAX
};

struct ThumbInstructionInfo {
	uint16_t opcode;
	enum ThumbMnemonic mnemonic;
	union ARMOperand op1;
	union ARMOperand op2;
	union ARMOperand op3;
	struct ARMMemoryAccess memory;
	int operandFormat;
	int branches;
	int traps;
	int accessesSpecialRegisters;
	int affectsCPSR;
};

void ARMDecodeThumb(uint16_t opcode, struct ThumbInstructionInfo* info);
int ARMDisassembleThumb(uint16_t opcode, char* buffer, int blen);

#endif
