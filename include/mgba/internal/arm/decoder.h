/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_DECODER_H
#define ARM_DECODER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/arm/arm.h>

// Bit 0: a register is involved with this operand
// Bit 1: an immediate is invovled with this operand
// Bit 2: a memory access is involved with this operand
// Bit 3: the destination of this operand is affected by this opcode
// Bit 4: this operand is shifted by a register
// Bit 5: this operand is shifted by an immediate
#define ARM_OPERAND_NONE                0x00000000
#define ARM_OPERAND_REGISTER_1          0x00000001
#define ARM_OPERAND_IMMEDIATE_1         0x00000002
#define ARM_OPERAND_MEMORY_1            0x00000004
#define ARM_OPERAND_AFFECTED_1          0x00000008
#define ARM_OPERAND_SHIFT_REGISTER_1    0x00000010
#define ARM_OPERAND_SHIFT_IMMEDIATE_1   0x00000020
#define ARM_OPERAND_1                   0x000000FF

#define ARM_OPERAND_REGISTER_2          0x00000100
#define ARM_OPERAND_IMMEDIATE_2         0x00000200
#define ARM_OPERAND_MEMORY_2            0x00000400
#define ARM_OPERAND_AFFECTED_2          0x00000800
#define ARM_OPERAND_SHIFT_REGISTER_2    0x00001000
#define ARM_OPERAND_SHIFT_IMMEDIATE_2   0x00002000
#define ARM_OPERAND_2                   0x0000FF00

#define ARM_OPERAND_REGISTER_3          0x00010000
#define ARM_OPERAND_IMMEDIATE_3         0x00020000
#define ARM_OPERAND_MEMORY_3            0x00040000
#define ARM_OPERAND_AFFECTED_3          0x00080000
#define ARM_OPERAND_SHIFT_REGISTER_3    0x00100000
#define ARM_OPERAND_SHIFT_IMMEDIATE_3   0x00200000
#define ARM_OPERAND_3                   0x00FF0000

#define ARM_OPERAND_REGISTER_4          0x01000000
#define ARM_OPERAND_IMMEDIATE_4         0x02000000
#define ARM_OPERAND_MEMORY_4            0x04000000
#define ARM_OPERAND_AFFECTED_4          0x08000000
#define ARM_OPERAND_SHIFT_REGISTER_4    0x10000000
#define ARM_OPERAND_SHIFT_IMMEDIATE_4   0x20000000
#define ARM_OPERAND_4                   0xFF000000

#define ARM_OPERAND_MEMORY (ARM_OPERAND_MEMORY_1 | ARM_OPERAND_MEMORY_2 | ARM_OPERAND_MEMORY_3 | ARM_OPERAND_MEMORY_4)

#define ARM_MEMORY_REGISTER_BASE     0x0001
#define ARM_MEMORY_IMMEDIATE_OFFSET  0x0002
#define ARM_MEMORY_REGISTER_OFFSET   0x0004
#define ARM_MEMORY_SHIFTED_OFFSET    0x0008
#define ARM_MEMORY_PRE_INCREMENT     0x0010
#define ARM_MEMORY_POST_INCREMENT    0x0020
#define ARM_MEMORY_OFFSET_SUBTRACT   0x0040
#define ARM_MEMORY_WRITEBACK         0x0080
#define ARM_MEMORY_DECREMENT_AFTER   0x0000
#define ARM_MEMORY_INCREMENT_AFTER   0x0100
#define ARM_MEMORY_DECREMENT_BEFORE  0x0200
#define ARM_MEMORY_INCREMENT_BEFORE  0x0300
#define ARM_MEMORY_SPSR_SWAP         0x0400
#define ARM_MEMORY_STORE             0x1000
#define ARM_MEMORY_LOAD              0x2000
#define ARM_MEMORY_SWAP              0x3000

#define ARM_PSR_C 1
#define ARM_PSR_X 2
#define ARM_PSR_S 4
#define ARM_PSR_F 8
#define ARM_PSR_MASK 0xF

#define MEMORY_FORMAT_TO_DIRECTION(F) (((F) >> 8) & 0x3)

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
		uint8_t shifterOp;
		union {
			uint8_t shifterReg;
			uint8_t shifterImm;
			uint8_t psrBits;
		};
	};
	int32_t immediate;
};

enum ARMMemoryAccessType {
	ARM_ACCESS_WORD = 4,
	ARM_ACCESS_HALFWORD = 2,
	ARM_ACCESS_SIGNED_HALFWORD = 10,
	ARM_ACCESS_BYTE = 1,
	ARM_ACCESS_SIGNED_BYTE = 9,
	ARM_ACCESS_TRANSLATED_WORD = 20,
	ARM_ACCESS_TRANSLATED_BYTE = 17
};

enum ARMBranchType {
	ARM_BRANCH_NONE = 0,
	ARM_BRANCH = 1,
	ARM_BRANCH_INDIRECT = 2,
	ARM_BRANCH_LINKED = 4
};

struct ARMMemoryAccess {
	uint8_t baseReg;
	uint8_t width;
	uint16_t format;
	union ARMOperand offset;
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
	ARM_MN_MRS,
	ARM_MN_MSR,
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
	ARM_MN_SWP,
	ARM_MN_TEQ,
	ARM_MN_TST,
	ARM_MN_UMLAL,
	ARM_MN_UMULL,

	ARM_MN_MAX
};

enum {
	ARM_CPSR = 16,
	ARM_SPSR = 17
};

struct ARMInstructionInfo {
	uint32_t opcode;
	union ARMOperand op1;
	union ARMOperand op2;
	union ARMOperand op3;
	union ARMOperand op4;
	struct ARMMemoryAccess memory;
	int operandFormat;
	unsigned execMode : 1;
	bool traps : 1;
	bool affectsCPSR : 1;
	unsigned branchType : 3;
	unsigned condition : 4;
	unsigned mnemonic : 6;
	unsigned iCycles : 3;
	unsigned cCycles : 4;
	unsigned sInstructionCycles : 4;
	unsigned nInstructionCycles : 4;
	unsigned sDataCycles : 10;
	unsigned nDataCycles : 10;
};

void ARMDecodeARM(uint32_t opcode, struct ARMInstructionInfo* info);
void ARMDecodeThumb(uint16_t opcode, struct ARMInstructionInfo* info);
bool ARMDecodeThumbCombine(struct ARMInstructionInfo* info1, struct ARMInstructionInfo* info2,
                           struct ARMInstructionInfo* out);
uint32_t ARMResolveMemoryAccess(struct ARMInstructionInfo* info, struct ARMRegisterFile* regs, uint32_t pc);

#ifdef USE_DEBUGGERS
struct mDebuggerSymbols;
int ARMDisassemble(const struct ARMInstructionInfo* info, struct ARMCore* core, const struct mDebuggerSymbols* symbols, uint32_t pc, char* buffer, int blen);
#endif

CXX_GUARD_END

#endif
