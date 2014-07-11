#ifndef ARM_DECODER_H
#define ARM_DECODER_H

#include <stdint.h>

// Bit 0: a register is involved with this operand
// Bit 1: an immediate is invovled with this operand
// Bit 2: a memory access is invovled with this operand
// Bit 3: the destination of this operand is affected by this opcode
// Bit 4: this operand is shifted by a register
// Bit 5: this operand is shifted by an immediate
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
	ARM_OPERAND_3 =                  0x00FF0000,

	ARM_OPERAND_REGISTER_4 =         0x01000000,
	ARM_OPERAND_IMMEDIATE_4 =        0x02000000,
	ARM_OPERAND_MEMORY_4 =           0x04000000,
	ARM_OPERAND_AFFECTED_4 =         0x08000000,
	ARM_OPERAND_SHIFT_REGISTER_4 =   0x10000000,
	ARM_OPERAND_SHIFT_IMMEDIATE_4 =  0x20000000,
	ARM_OPERAND_4 =                  0xFF000000
};

enum ARMMemoryFormat {
	ARM_MEMORY_REGISTER_BASE =    0x0001,
	ARM_MEMORY_IMMEDIATE_OFFSET = 0x0002,
	ARM_MEMORY_REGISTER_OFFSET  = 0x0004,
	ARM_MEMORY_SHIFTED_OFFSET =   0x0008,
	ARM_MEMORY_PRE_INCREMENT =    0x0010,
	ARM_MEMORY_POST_INCREMENT =   0x0020,
	ARM_MEMORY_OFFSET_SUBTRACT =  0x0040,

	ARM_MEMORY_WRITEBACK =        0x0080,
	ARM_MEMORY_DECREMENT_AFTER =  0x0000,
	ARM_MEMORY_INCREMENT_AFTER =  0x0100,
	ARM_MEMORY_DECREMENT_BEFORE = 0x0200,
	ARM_MEMORY_INCREMENT_BEFORE = 0x0300,
};

#define MEMORY_FORMAT_TO_DIRECTION(F) (((F) >> 8) & 0x7)

enum ARMCondition {
	ARM_CONDITION_EQ = 0x0,
	ARM_CONDITION_NE = 0x1,
	ARM_CONDITION_CS = 0x2,
	ARM_CONDITION_CC = 0x3,
	ARM_CONDITION_MI = 0x4,
	ARM_CONDITION_PL = 0x5,
	ARM_CONDITION_VS = 0x6,
	ARM_CONDITION_VC = 0x7,
	ARM_CONDITION_HI = 0x8,
	ARM_CONDITION_LS = 0x9,
	ARM_CONDITION_GE = 0xA,
	ARM_CONDITION_LT = 0xB,
	ARM_CONDITION_GT = 0xC,
	ARM_CONDITION_LE = 0xD,
	ARM_CONDITION_AL = 0xE,
	ARM_CONDITION_NV = 0xF
};

enum ARMShifterOperation {
	ARM_SHIFT_NONE = 0,
	ARM_SHIFT_LSL,
	ARM_SHIFT_LSR,
	ARM_SHIFT_ASR,
	ARM_SHIFT_ROR,
	ARM_SHIFT_RRX
};

union ARMOperand {
	struct {
		uint8_t reg;
		enum ARMShifterOperation shifterOp;
		union {
			uint8_t shifterReg;
			uint8_t shifterImm;
		};
	};
	int32_t immediate;
};

enum ARMMemoryAccessType {
	ARM_ACCESS_WORD = 4,
	ARM_ACCESS_HALFWORD = 2,
	ARM_ACCESS_SIGNED_HALFWORD = 10,
	ARM_ACCESS_BYTE = 1,
	ARM_ACCESS_SIGNED_BYTE = 9
};

struct ARMMemoryAccess {
	uint8_t baseReg;
	uint16_t format;
	union ARMOperand offset;
	enum ARMMemoryAccessType width;
};

enum ARMMnemonic {
	ARM_MN_ILL = 0,
	ARM_MN_ADC,
	ARM_MN_ADD,
	ARM_MN_AND,
	ARM_MN_ASR,
	ARM_MN_B,
	ARM_MN_BIC,
	ARM_MN_BKPT,
	ARM_MN_BL,
	ARM_MN_BLH,
	ARM_MN_BX,
	ARM_MN_CMN,
	ARM_MN_CMP,
	ARM_MN_EOR,
	ARM_MN_LDM,
	ARM_MN_LDR,
	ARM_MN_LSL,
	ARM_MN_LSR,
	ARM_MN_MLA,
	ARM_MN_MOV,
	ARM_MN_MUL,
	ARM_MN_MVN,
	ARM_MN_NEG,
	ARM_MN_ORR,
	ARM_MN_ROR,
	ARM_MN_RSB,
	ARM_MN_RSC,
	ARM_MN_SBC,
	ARM_MN_SMLAL,
	ARM_MN_SMULL,
	ARM_MN_STM,
	ARM_MN_STR,
	ARM_MN_SUB,
	ARM_MN_SWI,
	ARM_MN_TEQ,
	ARM_MN_TST,
	ARM_MN_UMLAL,
	ARM_MN_UMULL,

	ARM_MN_MAX
};

struct ARMInstructionInfo {
	uint32_t opcode;
	enum ARMMnemonic mnemonic;
	union ARMOperand op1;
	union ARMOperand op2;
	union ARMOperand op3;
	union ARMOperand op4;
	struct ARMMemoryAccess memory;
	int operandFormat;
	int branches;
	int traps;
	int affectsCPSR;
	int condition;
	int sDataCycles;
	int nDataCycles;
	int sInstructionCycles;
	int nInstructionCycles;
	int iCycles;
	int cCycles;
};

void ARMDecodeThumb(uint16_t opcode, struct ARMInstructionInfo* info);
int ARMDisassembleThumb(uint16_t opcode, uint32_t pc, char* buffer, int blen);

#endif
