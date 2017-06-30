/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/lr35902/decoder.h>

#include <mgba/internal/lr35902/emitter-lr35902.h>
#include <mgba/internal/lr35902/lr35902.h>

typedef size_t (*LR35902Decoder)(uint8_t opcode, struct LR35902InstructionInfo* info);

#define DEFINE_DECODER_LR35902(NAME, BODY) \
	static size_t _LR35902Decode ## NAME (uint8_t opcode, struct LR35902InstructionInfo* info) { \
		UNUSED(opcode); \
		info->mnemonic = LR35902_MN_RST; \
		BODY; \
		return 0; \
	}

DEFINE_DECODER_LR35902(NOP, info->mnemonic = LR35902_MN_NOP;)

#define DEFINE_LD_DECODER_LR35902_NOHL(NAME) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _A, \
		info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		info->op2.reg = LR35902_REG_A) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _B, \
		info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		info->op2.reg = LR35902_REG_B) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _C, \
		info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		info->op2.reg = LR35902_REG_C) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _D, \
		info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		info->op2.reg = LR35902_REG_D) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _E, \
		info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		info->op2.reg = LR35902_REG_E) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _H, \
		info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		info->op2.reg = LR35902_REG_H) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _L, \
		info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		info->op2.reg = LR35902_REG_L)

#define DEFINE_LD_DECODER_LR35902_MEM(NAME, REG) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _ ## REG, info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		info->op2.reg = LR35902_REG_ ## REG; \
		info->op2.flags = LR35902_OP_FLAG_MEMORY;)

#define DEFINE_LD_DECODER_LR35902_MEM_2(NAME, REG) \
	DEFINE_DECODER_LR35902(LD ## REG ## _ ## NAME, info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## REG; \
		info->op1.flags = LR35902_OP_FLAG_MEMORY; \
		info->op2.reg = LR35902_REG_ ## NAME;)

#define DEFINE_LD_DECODER_LR35902(NAME) \
	DEFINE_LD_DECODER_LR35902_MEM(NAME, HL) \
	DEFINE_LD_DECODER_LR35902_MEM_2(NAME, HL) \
	DEFINE_DECODER_LR35902(LD ## NAME ## _, info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		return 1;) \
	DEFINE_LD_DECODER_LR35902_NOHL(NAME)

#define DEFINE_LD_2_DECODER_LR35902(NAME) \
	DEFINE_DECODER_LR35902(LD ## NAME, info->mnemonic = LR35902_MN_LD; \
		info->op1.reg = LR35902_REG_ ## NAME; \
		return 2;)

DEFINE_LD_DECODER_LR35902(B);
DEFINE_LD_DECODER_LR35902(C);
DEFINE_LD_DECODER_LR35902(D);
DEFINE_LD_DECODER_LR35902(E);
DEFINE_LD_DECODER_LR35902(H);
DEFINE_LD_DECODER_LR35902(L);
DEFINE_LD_DECODER_LR35902(A);
DEFINE_LD_DECODER_LR35902_MEM(A, BC);
DEFINE_LD_DECODER_LR35902_MEM(A, DE);

DEFINE_LD_2_DECODER_LR35902(BC);
DEFINE_LD_2_DECODER_LR35902(DE);
DEFINE_LD_2_DECODER_LR35902(HL);
DEFINE_LD_2_DECODER_LR35902(SP);

DEFINE_DECODER_LR35902(LDHL_, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_HL; \
	info->op1.flags = LR35902_OP_FLAG_MEMORY; \
	return 1;)

DEFINE_DECODER_LR35902(LDHL_SP, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_HL; \
	info->op2.reg = LR35902_REG_SP; \
	return 1;)

DEFINE_DECODER_LR35902(LDSP_HL, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_SP; \
	info->op2.reg = LR35902_REG_HL;)

DEFINE_DECODER_LR35902(LDAIOC, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_A; \
	info->op2.reg = LR35902_REG_C; \
	info->op2.immediate = 0xFF00; \
	info->op2.flags = LR35902_OP_FLAG_MEMORY;)

DEFINE_DECODER_LR35902(LDIOCA, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_C; \
	info->op1.immediate = 0xFF00; \
	info->op1.flags = LR35902_OP_FLAG_MEMORY; \
	info->op2.reg = LR35902_REG_A;)

DEFINE_DECODER_LR35902(LDAIO, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_A; \
	info->op2.immediate = 0xFF00; \
	info->op2.flags = LR35902_OP_FLAG_MEMORY; \
	return 1;)

DEFINE_DECODER_LR35902(LDIOA, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.immediate = 0xFF00; \
	info->op1.flags = LR35902_OP_FLAG_MEMORY; \
	info->op2.reg = LR35902_REG_A; \
	return 1;)

#define DEFINE_ALU_DECODER_LR35902_NOHL(NAME) \
	DEFINE_DECODER_LR35902(NAME ## A, info->mnemonic = LR35902_MN_ ## NAME; info->op1.reg = LR35902_REG_A) \
	DEFINE_DECODER_LR35902(NAME ## B, info->mnemonic = LR35902_MN_ ## NAME; info->op1.reg = LR35902_REG_B) \
	DEFINE_DECODER_LR35902(NAME ## C, info->mnemonic = LR35902_MN_ ## NAME; info->op1.reg = LR35902_REG_C) \
	DEFINE_DECODER_LR35902(NAME ## D, info->mnemonic = LR35902_MN_ ## NAME; info->op1.reg = LR35902_REG_D) \
	DEFINE_DECODER_LR35902(NAME ## E, info->mnemonic = LR35902_MN_ ## NAME; info->op1.reg = LR35902_REG_E) \
	DEFINE_DECODER_LR35902(NAME ## H, info->mnemonic = LR35902_MN_ ## NAME; info->op1.reg = LR35902_REG_H) \
	DEFINE_DECODER_LR35902(NAME ## L, info->mnemonic = LR35902_MN_ ## NAME; info->op1.reg = LR35902_REG_L)

#define DEFINE_ALU_DECODER_LR35902_MEM(NAME, REG) \
	DEFINE_DECODER_LR35902(NAME ## REG, info->mnemonic = LR35902_MN_ ## NAME; \
		info->op1.reg = LR35902_REG_HL; \
		info->op1.flags = LR35902_OP_FLAG_MEMORY;)

#define DEFINE_ALU_DECODER_LR35902(NAME) \
	DEFINE_ALU_DECODER_LR35902_MEM(NAME, HL) \
	DEFINE_DECODER_LR35902(NAME, info->mnemonic = LR35902_MN_ ## NAME; \
		info->op1.reg = LR35902_REG_A; \
		info->op1.flags = LR35902_OP_FLAG_IMPLICIT; \
		return 1;) \
	DEFINE_ALU_DECODER_LR35902_NOHL(NAME)

DEFINE_ALU_DECODER_LR35902_NOHL(INC);
DEFINE_ALU_DECODER_LR35902_NOHL(DEC);
DEFINE_ALU_DECODER_LR35902(AND);
DEFINE_ALU_DECODER_LR35902(XOR);
DEFINE_ALU_DECODER_LR35902(OR);
DEFINE_ALU_DECODER_LR35902(CP);
DEFINE_ALU_DECODER_LR35902(ADD);
DEFINE_ALU_DECODER_LR35902(ADC);
DEFINE_ALU_DECODER_LR35902(SUB);
DEFINE_ALU_DECODER_LR35902(SBC);

#define DEFINE_ALU_DECODER_LR35902_ADD_HL(REG) \
	DEFINE_DECODER_LR35902(ADDHL_ ## REG, info->mnemonic = LR35902_MN_ADD; \
		info->op1.reg = LR35902_REG_HL; \
		info->op2.reg = LR35902_REG_ ## REG;)

DEFINE_ALU_DECODER_LR35902_ADD_HL(BC)
DEFINE_ALU_DECODER_LR35902_ADD_HL(DE)
DEFINE_ALU_DECODER_LR35902_ADD_HL(HL)
DEFINE_ALU_DECODER_LR35902_ADD_HL(SP)

DEFINE_DECODER_LR35902(ADDSP, info->mnemonic = LR35902_MN_ADD; \
	info->op1.reg = LR35902_REG_SP; \
	return 1;)

#define DEFINE_CONDITIONAL_DECODER_LR35902(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(, 0) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(C, LR35902_COND_C) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(Z, LR35902_COND_Z) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(NC, LR35902_COND_NC) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(NZ, LR35902_COND_NZ)

#define DEFINE_JP_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_DECODER_LR35902(JP ## CONDITION_NAME, \
		info->mnemonic = LR35902_MN_JP; \
		info->condition = CONDITION; \
		return 2;)

#define DEFINE_JR_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_DECODER_LR35902(JR ## CONDITION_NAME, \
		info->mnemonic = LR35902_MN_JR; \
		info->condition = CONDITION; \
		return 1;)

#define DEFINE_CALL_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_DECODER_LR35902(CALL ## CONDITION_NAME, \
		info->mnemonic = LR35902_MN_CALL; \
		info->condition = CONDITION; \
		return 2;)

#define DEFINE_RET_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_DECODER_LR35902(RET ## CONDITION_NAME, \
		info->mnemonic = LR35902_MN_RET; \
		info->condition = CONDITION;)

DEFINE_CONDITIONAL_DECODER_LR35902(JP);
DEFINE_CONDITIONAL_DECODER_LR35902(JR);
DEFINE_CONDITIONAL_DECODER_LR35902(CALL);
DEFINE_CONDITIONAL_DECODER_LR35902(RET);

DEFINE_DECODER_LR35902(JPHL, \
	info->mnemonic = LR35902_MN_JP; \
	info->op1.reg = LR35902_REG_HL)

DEFINE_DECODER_LR35902(RETI, info->mnemonic = LR35902_MN_RETI)

DEFINE_DECODER_LR35902(LDBC_A, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_BC; \
	info->op1.flags = LR35902_OP_FLAG_MEMORY; \
	info->op2.reg = LR35902_REG_A;)

DEFINE_DECODER_LR35902(LDDE_A, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_DE; \
	info->op1.flags = LR35902_OP_FLAG_MEMORY; \
	info->op2.reg = LR35902_REG_A;)

DEFINE_DECODER_LR35902(LDIA, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.flags = LR35902_OP_FLAG_MEMORY; \
	info->op2.reg = LR35902_REG_A; \
	return 2;)

DEFINE_DECODER_LR35902(LDAI, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_A; \
	info->op2.flags = LR35902_OP_FLAG_MEMORY; \
	return 2;)

DEFINE_DECODER_LR35902(LDISP, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.flags = LR35902_OP_FLAG_MEMORY; \
	info->op2.reg = LR35902_REG_SP; \
	return 2;)

DEFINE_DECODER_LR35902(LDIHLA, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_HL; \
	info->op1.flags = LR35902_OP_FLAG_INCREMENT | LR35902_OP_FLAG_MEMORY; \
	info->op2.reg = LR35902_REG_A;)

DEFINE_DECODER_LR35902(LDDHLA, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_HL; \
	info->op1.flags = LR35902_OP_FLAG_DECREMENT | LR35902_OP_FLAG_MEMORY; \
	info->op2.reg = LR35902_REG_A;)

DEFINE_DECODER_LR35902(LDA_IHL, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_A; \
	info->op2.reg = LR35902_REG_HL; \
	info->op2.flags = LR35902_OP_FLAG_INCREMENT | LR35902_OP_FLAG_MEMORY;)

DEFINE_DECODER_LR35902(LDA_DHL, \
	info->mnemonic = LR35902_MN_LD; \
	info->op1.reg = LR35902_REG_A; \
	info->op2.reg = LR35902_REG_HL; \
	info->op2.flags = LR35902_OP_FLAG_DECREMENT | LR35902_OP_FLAG_MEMORY;)

#define DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(REG) \
	DEFINE_DECODER_LR35902(INC ## REG, info->mnemonic = LR35902_MN_INC; info->op1.reg = LR35902_REG_ ## REG) \
	DEFINE_DECODER_LR35902(DEC ## REG, info->mnemonic = LR35902_MN_DEC; info->op1.reg = LR35902_REG_ ## REG)

DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(BC);
DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(DE);
DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(HL);
DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(SP);

DEFINE_DECODER_LR35902(INC_HL,
	info->mnemonic = LR35902_MN_INC;
	info->op1.reg = LR35902_REG_HL;
	info->op1.flags = LR35902_OP_FLAG_MEMORY;)

DEFINE_DECODER_LR35902(DEC_HL,
	info->mnemonic = LR35902_MN_DEC;
	info->op1.reg = LR35902_REG_HL;
	info->op1.flags = LR35902_OP_FLAG_MEMORY;)

DEFINE_DECODER_LR35902(SCF, info->mnemonic = LR35902_MN_SCF)
DEFINE_DECODER_LR35902(CCF, info->mnemonic = LR35902_MN_CCF)
DEFINE_DECODER_LR35902(CPL_, info->mnemonic = LR35902_MN_CPL)
DEFINE_DECODER_LR35902(DAA, info->mnemonic = LR35902_MN_DAA)

#define DEFINE_POPPUSH_DECODER_LR35902(REG) \
	DEFINE_DECODER_LR35902(POP ## REG, \
		info->mnemonic = LR35902_MN_POP; \
		info->op1.reg = LR35902_REG_ ## REG;) \
	DEFINE_DECODER_LR35902(PUSH ## REG, \
		info->mnemonic = LR35902_MN_PUSH; \
		info->op1.reg = LR35902_REG_ ## REG;) \

DEFINE_POPPUSH_DECODER_LR35902(BC);
DEFINE_POPPUSH_DECODER_LR35902(DE);
DEFINE_POPPUSH_DECODER_LR35902(HL);
DEFINE_POPPUSH_DECODER_LR35902(AF);

#define DEFINE_CB_2_DECODER_LR35902(NAME, BODY) \
	DEFINE_DECODER_LR35902(NAME ## B, info->op2.reg = LR35902_REG_B; BODY) \
	DEFINE_DECODER_LR35902(NAME ## C, info->op2.reg = LR35902_REG_C; BODY) \
	DEFINE_DECODER_LR35902(NAME ## D, info->op2.reg = LR35902_REG_D; BODY) \
	DEFINE_DECODER_LR35902(NAME ## E, info->op2.reg = LR35902_REG_E; BODY) \
	DEFINE_DECODER_LR35902(NAME ## H, info->op2.reg = LR35902_REG_H; BODY) \
	DEFINE_DECODER_LR35902(NAME ## L, info->op2.reg = LR35902_REG_L; BODY) \
	DEFINE_DECODER_LR35902(NAME ## HL, info->op2.reg = LR35902_REG_HL; info->op2.flags = LR35902_OP_FLAG_MEMORY; BODY) \
	DEFINE_DECODER_LR35902(NAME ## A, info->op2.reg = LR35902_REG_A; BODY)

#define DEFINE_CB_DECODER_LR35902(NAME, BODY) \
	DEFINE_CB_2_DECODER_LR35902(NAME ## 0, info->op1.immediate = 0; BODY) \
	DEFINE_CB_2_DECODER_LR35902(NAME ## 1, info->op1.immediate = 1; BODY) \
	DEFINE_CB_2_DECODER_LR35902(NAME ## 2, info->op1.immediate = 2; BODY) \
	DEFINE_CB_2_DECODER_LR35902(NAME ## 3, info->op1.immediate = 3; BODY) \
	DEFINE_CB_2_DECODER_LR35902(NAME ## 4, info->op1.immediate = 4; BODY) \
	DEFINE_CB_2_DECODER_LR35902(NAME ## 5, info->op1.immediate = 5; BODY) \
	DEFINE_CB_2_DECODER_LR35902(NAME ## 6, info->op1.immediate = 6; BODY) \
	DEFINE_CB_2_DECODER_LR35902(NAME ## 7, info->op1.immediate = 7; BODY)

DEFINE_CB_DECODER_LR35902(BIT, info->mnemonic = LR35902_MN_BIT)
DEFINE_CB_DECODER_LR35902(RES, info->mnemonic = LR35902_MN_RES)
DEFINE_CB_DECODER_LR35902(SET, info->mnemonic = LR35902_MN_SET)

#define DEFINE_CB_X_DECODER_LR35902(NAME) \
	DEFINE_CB_2_DECODER_LR35902(NAME, info->mnemonic = LR35902_MN_ ## NAME) \
	DEFINE_DECODER_LR35902(NAME ## A_, info->mnemonic = LR35902_MN_ ## NAME; info->op1.reg = LR35902_REG_A)

DEFINE_CB_X_DECODER_LR35902(RL)
DEFINE_CB_X_DECODER_LR35902(RLC)
DEFINE_CB_X_DECODER_LR35902(RR)
DEFINE_CB_X_DECODER_LR35902(RRC)
DEFINE_CB_2_DECODER_LR35902(SLA, info->mnemonic = LR35902_MN_SLA)
DEFINE_CB_2_DECODER_LR35902(SRA, info->mnemonic = LR35902_MN_SRA)
DEFINE_CB_2_DECODER_LR35902(SRL, info->mnemonic = LR35902_MN_SRL)
DEFINE_CB_2_DECODER_LR35902(SWAP, info->mnemonic = LR35902_MN_SWAP)

DEFINE_DECODER_LR35902(DI, info->mnemonic = LR35902_MN_DI)
DEFINE_DECODER_LR35902(EI, info->mnemonic = LR35902_MN_EI)
DEFINE_DECODER_LR35902(HALT, info->mnemonic = LR35902_MN_HALT)
DEFINE_DECODER_LR35902(ILL, info->mnemonic = LR35902_MN_ILL)
DEFINE_DECODER_LR35902(STOP, info->mnemonic = LR35902_MN_STOP; return 1)

#define DEFINE_RST_DECODER_LR35902(VEC) \
	DEFINE_DECODER_LR35902(RST ## VEC, info->op1.immediate = 0x ## VEC;)

DEFINE_RST_DECODER_LR35902(00);
DEFINE_RST_DECODER_LR35902(08);
DEFINE_RST_DECODER_LR35902(10);
DEFINE_RST_DECODER_LR35902(18);
DEFINE_RST_DECODER_LR35902(20);
DEFINE_RST_DECODER_LR35902(28);
DEFINE_RST_DECODER_LR35902(30);
DEFINE_RST_DECODER_LR35902(38);

DEFINE_DECODER_LR35902(CB, return 1)

const LR35902Decoder _lr35902DecoderTable[0x100] = {
	DECLARE_LR35902_EMITTER_BLOCK(_LR35902Decode)
};

const LR35902Decoder _lr35902CBDecoderTable[0x100] = {
	DECLARE_LR35902_CB_EMITTER_BLOCK(_LR35902Decode)
};

size_t LR35902Decode(uint8_t opcode, struct LR35902InstructionInfo* info) {
	if (info->opcodeSize == sizeof(info->opcode)) {
		return 0;
	}
	info->opcode[info->opcodeSize] = opcode;
	LR35902Decoder decoder;
	switch (info->opcodeSize) {
	case 0:
		decoder = _lr35902DecoderTable[opcode];
		break;
	case 1:
		if (info->opcode[0] == 0xCB) {
			decoder = _lr35902CBDecoderTable[opcode];
			break;
		}
	// Fall through
	case 2:
		++info->opcodeSize;
		if (info->op1.reg) {
			info->op2.immediate |= opcode << ((info->opcodeSize - 2) * 8);
		} else {
			info->op1.immediate |= opcode << ((info->opcodeSize - 2) * 8);
		}
		return 0;
	}
	++info->opcodeSize;
	return decoder(opcode, info);
}

#define ADVANCE(AMOUNT) \
	if (AMOUNT > blen) { \
		buffer[blen - 1] = '\0'; \
		return total; \
	} \
	total += AMOUNT; \
	buffer += AMOUNT; \
	blen -= AMOUNT;

static const char* _lr35902Conditions[] = {
	NULL,
	"c",
	"z",
	"nc",
	"nz",
};

static const char* _lr35902Registers[] = {
	"",
	"b",
	"c",
	"d",
	"e",
	"h",
	"l",
	"a",
	"f",
	"bc",
	"de",
	"hl",
	"af",
	"sp",
	"pc",
};

static const char* _lr35902MnemonicStrings[] = {
	"--",
	"adc",
	"add",
	"and",
	"bit",
	"call",
	"ccf",
	"cp",
	"cpl",
	"daa",
	"dec",
	"di",
	"ei",
	"halt",
	"inc",
	"jp",
	"jr",
	"ld",
	"nop",
	"or",
	"pop",
	"push",
	"res",
	"ret",
	"reti",
	"rl",
	"rlc",
	"rr",
	"rrc",
	"rst",
	"sbc",
	"scf",
	"set",
	"sla",
	"sra",
	"srl",
	"stop",
	"sub",
	"swap",
	"xor",

	"ill"
};


static int _decodeOperand(struct LR35902Operand op, char* buffer, int blen) {
	int total = 0;
	if (op.flags & LR35902_OP_FLAG_IMPLICIT) {
		return 0;
	}

	if (op.flags & LR35902_OP_FLAG_MEMORY) {
		strncpy(buffer, "[", blen - 1);
		ADVANCE(1);
	}
	if (op.reg) {
		int written = snprintf(buffer, blen - 1, "%s", _lr35902Registers[op.reg]);
		ADVANCE(written);
	} else {
		int written = snprintf(buffer, blen - 1, "$%02X", op.immediate);
		ADVANCE(written);
		if (op.reg) {
			strncpy(buffer, "+", blen - 1);
			ADVANCE(1);
		}
	}
	if (op.flags & LR35902_OP_FLAG_INCREMENT) {
		strncpy(buffer, "+", blen - 1);
		ADVANCE(1);
	}
	if (op.flags & LR35902_OP_FLAG_DECREMENT) {
		strncpy(buffer, "-", blen - 1);
		ADVANCE(1);
	}
	if (op.flags & LR35902_OP_FLAG_MEMORY) {
		strncpy(buffer, "]", blen - 1);
		ADVANCE(1);
	}
	return total;
}

int LR35902Disassemble(struct LR35902InstructionInfo* info, char* buffer, int blen) {
	const char* mnemonic = _lr35902MnemonicStrings[info->mnemonic];
	int written;
	int total = 0;
	const char* cond = _lr35902Conditions[info->condition];

	written = snprintf(buffer, blen - 1, "%s ", mnemonic);
	ADVANCE(written);

	if (cond) {
		written = snprintf(buffer, blen - 1, "%s", cond);
		ADVANCE(written);

		if (info->op1.reg || info->op1.immediate || info->op2.reg || info->op2.immediate) {
			strncpy(buffer, ", ", blen - 1);
			ADVANCE(2);
		}
	}

	if (info->op1.reg || info->op1.immediate || info->op2.reg || info->op2.immediate) {
		written = _decodeOperand(info->op1, buffer, blen);
		ADVANCE(written);
	}

	if (info->op2.reg || (!info->op1.immediate && info->opcodeSize > 1 && info->opcode[0] != 0xCB)) {
		if (written) {
			strncpy(buffer, ", ", blen - 1);
			ADVANCE(2);
		}
		written = _decodeOperand(info->op2, buffer, blen);
		ADVANCE(written);
	}

	buffer[blen - 1] = '\0';
	return total;
}
