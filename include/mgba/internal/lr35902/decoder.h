/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LR35902_DECODER_H
#define LR35902_DECODER_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum LR35902Condition {
	LR35902_COND_NONE = 0x0,
	LR35902_COND_C = 0x1,
	LR35902_COND_Z = 0x2,
	LR35902_COND_NC = 0x3,
	LR35902_COND_NZ = 0x4
};

enum LR35902Mnemonic {
	LR35902_MN_ILL = 0,
	LR35902_MN_ADC,
	LR35902_MN_ADD,
	LR35902_MN_AND,
	LR35902_MN_BIT,
	LR35902_MN_CALL,
	LR35902_MN_CCF,
	LR35902_MN_CP,
	LR35902_MN_CPL,
	LR35902_MN_DAA,
	LR35902_MN_DEC,
	LR35902_MN_DI,
	LR35902_MN_EI,
	LR35902_MN_HALT,
	LR35902_MN_INC,
	LR35902_MN_JP,
	LR35902_MN_JR,
	LR35902_MN_LD,
	LR35902_MN_NOP,
	LR35902_MN_OR,
	LR35902_MN_POP,
	LR35902_MN_PUSH,
	LR35902_MN_RES,
	LR35902_MN_RET,
	LR35902_MN_RETI,
	LR35902_MN_RL,
	LR35902_MN_RLC,
	LR35902_MN_RR,
	LR35902_MN_RRC,
	LR35902_MN_RST,
	LR35902_MN_SBC,
	LR35902_MN_SCF,
	LR35902_MN_SET,
	LR35902_MN_SLA,
	LR35902_MN_SRA,
	LR35902_MN_SRL,
	LR35902_MN_STOP,
	LR35902_MN_SUB,
	LR35902_MN_SWAP,
	LR35902_MN_XOR,

	LR35902_MN_MAX
};

enum LR35902Register {
	LR35902_REG_B = 1,
	LR35902_REG_C,
	LR35902_REG_D,
	LR35902_REG_E,
	LR35902_REG_H,
	LR35902_REG_L,
	LR35902_REG_A,
	LR35902_REG_F,
	LR35902_REG_BC,
	LR35902_REG_DE,
	LR35902_REG_HL,
	LR35902_REG_AF,

	LR35902_REG_SP,
	LR35902_REG_PC
};

enum {
	LR35902_OP_FLAG_IMPLICIT = 1,
	LR35902_OP_FLAG_MEMORY = 2,
	LR35902_OP_FLAG_INCREMENT = 4,
	LR35902_OP_FLAG_DECREMENT = 8,
};

struct LR35902Operand {
	uint8_t reg;
	uint8_t flags;
	uint16_t immediate;
};

struct LR35902InstructionInfo {
	uint8_t opcode[3];
	uint8_t opcodeSize;
	struct LR35902Operand op1;
	struct LR35902Operand op2;
	unsigned mnemonic;
	unsigned condition;
};

size_t LR35902Decode(uint8_t opcode, struct LR35902InstructionInfo* info);
int LR35902Disassemble(struct LR35902InstructionInfo* info, char* buffer, int blen);

CXX_GUARD_END

#endif
