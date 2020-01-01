/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SM83_DECODER_H
#define SM83_DECODER_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum SM83Condition {
	SM83_COND_NONE = 0x0,
	SM83_COND_C = 0x1,
	SM83_COND_Z = 0x2,
	SM83_COND_NC = 0x3,
	SM83_COND_NZ = 0x4
};

enum SM83Mnemonic {
	SM83_MN_ILL = 0,
	SM83_MN_ADC,
	SM83_MN_ADD,
	SM83_MN_AND,
	SM83_MN_BIT,
	SM83_MN_CALL,
	SM83_MN_CCF,
	SM83_MN_CP,
	SM83_MN_CPL,
	SM83_MN_DAA,
	SM83_MN_DEC,
	SM83_MN_DI,
	SM83_MN_EI,
	SM83_MN_HALT,
	SM83_MN_INC,
	SM83_MN_JP,
	SM83_MN_JR,
	SM83_MN_LD,
	SM83_MN_NOP,
	SM83_MN_OR,
	SM83_MN_POP,
	SM83_MN_PUSH,
	SM83_MN_RES,
	SM83_MN_RET,
	SM83_MN_RETI,
	SM83_MN_RL,
	SM83_MN_RLC,
	SM83_MN_RR,
	SM83_MN_RRC,
	SM83_MN_RST,
	SM83_MN_SBC,
	SM83_MN_SCF,
	SM83_MN_SET,
	SM83_MN_SLA,
	SM83_MN_SRA,
	SM83_MN_SRL,
	SM83_MN_STOP,
	SM83_MN_SUB,
	SM83_MN_SWAP,
	SM83_MN_XOR,

	SM83_MN_MAX
};

enum SM83Register {
	SM83_REG_B = 1,
	SM83_REG_C,
	SM83_REG_D,
	SM83_REG_E,
	SM83_REG_H,
	SM83_REG_L,
	SM83_REG_A,
	SM83_REG_F,
	SM83_REG_BC,
	SM83_REG_DE,
	SM83_REG_HL,
	SM83_REG_AF,

	SM83_REG_SP,
	SM83_REG_PC
};

enum {
	SM83_OP_FLAG_IMPLICIT = 1,
	SM83_OP_FLAG_MEMORY = 2,
	SM83_OP_FLAG_INCREMENT = 4,
	SM83_OP_FLAG_DECREMENT = 8,
	SM83_OP_FLAG_RELATIVE = 16,
};

struct SM83Operand {
	uint8_t reg;
	uint8_t flags;
	uint16_t immediate;
};

struct SM83InstructionInfo {
	uint8_t opcode[3];
	uint8_t opcodeSize;
	struct SM83Operand op1;
	struct SM83Operand op2;
	unsigned mnemonic;
	unsigned condition;
};

size_t SM83Decode(uint8_t opcode, struct SM83InstructionInfo* info);
int SM83Disassemble(struct SM83InstructionInfo* info, uint16_t pc, char* buffer, int blen);

CXX_GUARD_END

#endif
