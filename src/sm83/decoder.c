/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/sm83/decoder.h>

#include <mgba/internal/sm83/emitter-sm83.h>
#include <mgba/internal/sm83/sm83.h>
#include <mgba-util/string.h>

typedef size_t (*SM83Decoder)(uint8_t opcode, struct SM83InstructionInfo* info);

#define DEFINE_DECODER_SM83(NAME, BODY) \
	static size_t _SM83Decode ## NAME (uint8_t opcode, struct SM83InstructionInfo* info) { \
		UNUSED(opcode); \
		info->mnemonic = SM83_MN_RST; \
		BODY; \
		return 0; \
	}

DEFINE_DECODER_SM83(NOP, info->mnemonic = SM83_MN_NOP;)

#define DEFINE_LD_DECODER_SM83_NOHL(NAME) \
	DEFINE_DECODER_SM83(LD ## NAME ## _A, \
		info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		info->op2.reg = SM83_REG_A) \
	DEFINE_DECODER_SM83(LD ## NAME ## _B, \
		info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		info->op2.reg = SM83_REG_B) \
	DEFINE_DECODER_SM83(LD ## NAME ## _C, \
		info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		info->op2.reg = SM83_REG_C) \
	DEFINE_DECODER_SM83(LD ## NAME ## _D, \
		info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		info->op2.reg = SM83_REG_D) \
	DEFINE_DECODER_SM83(LD ## NAME ## _E, \
		info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		info->op2.reg = SM83_REG_E) \
	DEFINE_DECODER_SM83(LD ## NAME ## _H, \
		info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		info->op2.reg = SM83_REG_H) \
	DEFINE_DECODER_SM83(LD ## NAME ## _L, \
		info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		info->op2.reg = SM83_REG_L)

#define DEFINE_LD_DECODER_SM83_MEM(NAME, REG) \
	DEFINE_DECODER_SM83(LD ## NAME ## _ ## REG, info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		info->op2.reg = SM83_REG_ ## REG; \
		info->op2.flags = SM83_OP_FLAG_MEMORY;)

#define DEFINE_LD_DECODER_SM83_MEM_2(NAME, REG) \
	DEFINE_DECODER_SM83(LD ## REG ## _ ## NAME, info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## REG; \
		info->op1.flags = SM83_OP_FLAG_MEMORY; \
		info->op2.reg = SM83_REG_ ## NAME;)

#define DEFINE_LD_DECODER_SM83(NAME) \
	DEFINE_LD_DECODER_SM83_MEM(NAME, HL) \
	DEFINE_LD_DECODER_SM83_MEM_2(NAME, HL) \
	DEFINE_DECODER_SM83(LD ## NAME ## _, info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		return 1;) \
	DEFINE_LD_DECODER_SM83_NOHL(NAME)

#define DEFINE_LD_2_DECODER_SM83(NAME) \
	DEFINE_DECODER_SM83(LD ## NAME, info->mnemonic = SM83_MN_LD; \
		info->op1.reg = SM83_REG_ ## NAME; \
		return 2;)

DEFINE_LD_DECODER_SM83(B);
DEFINE_LD_DECODER_SM83(C);
DEFINE_LD_DECODER_SM83(D);
DEFINE_LD_DECODER_SM83(E);
DEFINE_LD_DECODER_SM83(H);
DEFINE_LD_DECODER_SM83(L);
DEFINE_LD_DECODER_SM83(A);
DEFINE_LD_DECODER_SM83_MEM(A, BC);
DEFINE_LD_DECODER_SM83_MEM(A, DE);

DEFINE_LD_2_DECODER_SM83(BC);
DEFINE_LD_2_DECODER_SM83(DE);
DEFINE_LD_2_DECODER_SM83(HL);
DEFINE_LD_2_DECODER_SM83(SP);

DEFINE_DECODER_SM83(LDHL_, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_HL; \
	info->op1.flags = SM83_OP_FLAG_MEMORY; \
	return 1;)

DEFINE_DECODER_SM83(LDHL_SP, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_HL; \
	info->op2.reg = SM83_REG_SP; \
	return 1;)

DEFINE_DECODER_SM83(LDSP_HL, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_SP; \
	info->op2.reg = SM83_REG_HL;)

DEFINE_DECODER_SM83(LDAIOC, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_A; \
	info->op2.reg = SM83_REG_C; \
	info->op2.immediate = 0xFF00; \
	info->op2.flags = SM83_OP_FLAG_MEMORY;)

DEFINE_DECODER_SM83(LDIOCA, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_C; \
	info->op1.immediate = 0xFF00; \
	info->op1.flags = SM83_OP_FLAG_MEMORY; \
	info->op2.reg = SM83_REG_A;)

DEFINE_DECODER_SM83(LDAIO, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_A; \
	info->op2.immediate = 0xFF00; \
	info->op2.flags = SM83_OP_FLAG_MEMORY; \
	return 1;)

DEFINE_DECODER_SM83(LDIOA, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.immediate = 0xFF00; \
	info->op1.flags = SM83_OP_FLAG_MEMORY; \
	info->op2.reg = SM83_REG_A; \
	return 1;)

#define DEFINE_ALU_DECODER_SM83_NOHL(NAME) \
	DEFINE_DECODER_SM83(NAME ## A, info->mnemonic = SM83_MN_ ## NAME; info->op1.reg = SM83_REG_A) \
	DEFINE_DECODER_SM83(NAME ## B, info->mnemonic = SM83_MN_ ## NAME; info->op1.reg = SM83_REG_B) \
	DEFINE_DECODER_SM83(NAME ## C, info->mnemonic = SM83_MN_ ## NAME; info->op1.reg = SM83_REG_C) \
	DEFINE_DECODER_SM83(NAME ## D, info->mnemonic = SM83_MN_ ## NAME; info->op1.reg = SM83_REG_D) \
	DEFINE_DECODER_SM83(NAME ## E, info->mnemonic = SM83_MN_ ## NAME; info->op1.reg = SM83_REG_E) \
	DEFINE_DECODER_SM83(NAME ## H, info->mnemonic = SM83_MN_ ## NAME; info->op1.reg = SM83_REG_H) \
	DEFINE_DECODER_SM83(NAME ## L, info->mnemonic = SM83_MN_ ## NAME; info->op1.reg = SM83_REG_L)

#define DEFINE_ALU_DECODER_SM83_MEM(NAME, REG) \
	DEFINE_DECODER_SM83(NAME ## REG, info->mnemonic = SM83_MN_ ## NAME; \
		info->op1.reg = SM83_REG_HL; \
		info->op1.flags = SM83_OP_FLAG_MEMORY;)

#define DEFINE_ALU_DECODER_SM83(NAME) \
	DEFINE_ALU_DECODER_SM83_MEM(NAME, HL) \
	DEFINE_DECODER_SM83(NAME, info->mnemonic = SM83_MN_ ## NAME; \
		info->op1.reg = SM83_REG_A; \
		info->op1.flags = SM83_OP_FLAG_IMPLICIT; \
		return 1;) \
	DEFINE_ALU_DECODER_SM83_NOHL(NAME)

DEFINE_ALU_DECODER_SM83_NOHL(INC);
DEFINE_ALU_DECODER_SM83_NOHL(DEC);
DEFINE_ALU_DECODER_SM83(AND);
DEFINE_ALU_DECODER_SM83(XOR);
DEFINE_ALU_DECODER_SM83(OR);
DEFINE_ALU_DECODER_SM83(CP);
DEFINE_ALU_DECODER_SM83(ADD);
DEFINE_ALU_DECODER_SM83(ADC);
DEFINE_ALU_DECODER_SM83(SUB);
DEFINE_ALU_DECODER_SM83(SBC);

#define DEFINE_ALU_DECODER_SM83_ADD_HL(REG) \
	DEFINE_DECODER_SM83(ADDHL_ ## REG, info->mnemonic = SM83_MN_ADD; \
		info->op1.reg = SM83_REG_HL; \
		info->op2.reg = SM83_REG_ ## REG;)

DEFINE_ALU_DECODER_SM83_ADD_HL(BC)
DEFINE_ALU_DECODER_SM83_ADD_HL(DE)
DEFINE_ALU_DECODER_SM83_ADD_HL(HL)
DEFINE_ALU_DECODER_SM83_ADD_HL(SP)

DEFINE_DECODER_SM83(ADDSP, info->mnemonic = SM83_MN_ADD; \
	info->op1.reg = SM83_REG_SP; \
	return 1;)

#define DEFINE_CONDITIONAL_DECODER_SM83(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(, 0) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(C, SM83_COND_C) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(Z, SM83_COND_Z) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(NC, SM83_COND_NC) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(NZ, SM83_COND_NZ)

#define DEFINE_JP_INSTRUCTION_SM83(CONDITION_NAME, CONDITION) \
	DEFINE_DECODER_SM83(JP ## CONDITION_NAME, \
		info->mnemonic = SM83_MN_JP; \
		info->condition = CONDITION; \
		return 2;)

#define DEFINE_JR_INSTRUCTION_SM83(CONDITION_NAME, CONDITION) \
	DEFINE_DECODER_SM83(JR ## CONDITION_NAME, \
		info->mnemonic = SM83_MN_JR; \
		info->condition = CONDITION; \
		info->op1.flags = SM83_OP_FLAG_RELATIVE; \
		return 1;)

#define DEFINE_CALL_INSTRUCTION_SM83(CONDITION_NAME, CONDITION) \
	DEFINE_DECODER_SM83(CALL ## CONDITION_NAME, \
		info->mnemonic = SM83_MN_CALL; \
		info->condition = CONDITION; \
		return 2;)

#define DEFINE_RET_INSTRUCTION_SM83(CONDITION_NAME, CONDITION) \
	DEFINE_DECODER_SM83(RET ## CONDITION_NAME, \
		info->mnemonic = SM83_MN_RET; \
		info->condition = CONDITION;)

DEFINE_CONDITIONAL_DECODER_SM83(JP);
DEFINE_CONDITIONAL_DECODER_SM83(JR);
DEFINE_CONDITIONAL_DECODER_SM83(CALL);
DEFINE_CONDITIONAL_DECODER_SM83(RET);

DEFINE_DECODER_SM83(JPHL, \
	info->mnemonic = SM83_MN_JP; \
	info->op1.reg = SM83_REG_HL)

DEFINE_DECODER_SM83(RETI, info->mnemonic = SM83_MN_RETI)

DEFINE_DECODER_SM83(LDBC_A, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_BC; \
	info->op1.flags = SM83_OP_FLAG_MEMORY; \
	info->op2.reg = SM83_REG_A;)

DEFINE_DECODER_SM83(LDDE_A, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_DE; \
	info->op1.flags = SM83_OP_FLAG_MEMORY; \
	info->op2.reg = SM83_REG_A;)

DEFINE_DECODER_SM83(LDIA, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.flags = SM83_OP_FLAG_MEMORY; \
	info->op2.reg = SM83_REG_A; \
	return 2;)

DEFINE_DECODER_SM83(LDAI, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_A; \
	info->op2.flags = SM83_OP_FLAG_MEMORY; \
	return 2;)

DEFINE_DECODER_SM83(LDISP, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.flags = SM83_OP_FLAG_MEMORY; \
	info->op2.reg = SM83_REG_SP; \
	return 2;)

DEFINE_DECODER_SM83(LDIHLA, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_HL; \
	info->op1.flags = SM83_OP_FLAG_INCREMENT | SM83_OP_FLAG_MEMORY; \
	info->op2.reg = SM83_REG_A;)

DEFINE_DECODER_SM83(LDDHLA, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_HL; \
	info->op1.flags = SM83_OP_FLAG_DECREMENT | SM83_OP_FLAG_MEMORY; \
	info->op2.reg = SM83_REG_A;)

DEFINE_DECODER_SM83(LDA_IHL, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_A; \
	info->op2.reg = SM83_REG_HL; \
	info->op2.flags = SM83_OP_FLAG_INCREMENT | SM83_OP_FLAG_MEMORY;)

DEFINE_DECODER_SM83(LDA_DHL, \
	info->mnemonic = SM83_MN_LD; \
	info->op1.reg = SM83_REG_A; \
	info->op2.reg = SM83_REG_HL; \
	info->op2.flags = SM83_OP_FLAG_DECREMENT | SM83_OP_FLAG_MEMORY;)

#define DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(REG) \
	DEFINE_DECODER_SM83(INC ## REG, info->mnemonic = SM83_MN_INC; info->op1.reg = SM83_REG_ ## REG) \
	DEFINE_DECODER_SM83(DEC ## REG, info->mnemonic = SM83_MN_DEC; info->op1.reg = SM83_REG_ ## REG)

DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(BC);
DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(DE);
DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(HL);
DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(SP);

DEFINE_DECODER_SM83(INC_HL,
	info->mnemonic = SM83_MN_INC;
	info->op1.reg = SM83_REG_HL;
	info->op1.flags = SM83_OP_FLAG_MEMORY;)

DEFINE_DECODER_SM83(DEC_HL,
	info->mnemonic = SM83_MN_DEC;
	info->op1.reg = SM83_REG_HL;
	info->op1.flags = SM83_OP_FLAG_MEMORY;)

DEFINE_DECODER_SM83(SCF, info->mnemonic = SM83_MN_SCF)
DEFINE_DECODER_SM83(CCF, info->mnemonic = SM83_MN_CCF)
DEFINE_DECODER_SM83(CPL_, info->mnemonic = SM83_MN_CPL)
DEFINE_DECODER_SM83(DAA, info->mnemonic = SM83_MN_DAA)

#define DEFINE_POPPUSH_DECODER_SM83(REG) \
	DEFINE_DECODER_SM83(POP ## REG, \
		info->mnemonic = SM83_MN_POP; \
		info->op1.reg = SM83_REG_ ## REG;) \
	DEFINE_DECODER_SM83(PUSH ## REG, \
		info->mnemonic = SM83_MN_PUSH; \
		info->op1.reg = SM83_REG_ ## REG;) \

DEFINE_POPPUSH_DECODER_SM83(BC);
DEFINE_POPPUSH_DECODER_SM83(DE);
DEFINE_POPPUSH_DECODER_SM83(HL);
DEFINE_POPPUSH_DECODER_SM83(AF);

#define DEFINE_CB_OP_DECODER_SM83(NAME, BODY, OP) \
	DEFINE_DECODER_SM83(NAME ## B, info->OP.reg = SM83_REG_B; BODY) \
	DEFINE_DECODER_SM83(NAME ## C, info->OP.reg = SM83_REG_C; BODY) \
	DEFINE_DECODER_SM83(NAME ## D, info->OP.reg = SM83_REG_D; BODY) \
	DEFINE_DECODER_SM83(NAME ## E, info->OP.reg = SM83_REG_E; BODY) \
	DEFINE_DECODER_SM83(NAME ## H, info->OP.reg = SM83_REG_H; BODY) \
	DEFINE_DECODER_SM83(NAME ## L, info->OP.reg = SM83_REG_L; BODY) \
	DEFINE_DECODER_SM83(NAME ## HL, info->OP.reg = SM83_REG_HL; info->OP.flags = SM83_OP_FLAG_MEMORY; BODY) \
	DEFINE_DECODER_SM83(NAME ## A, info->OP.reg = SM83_REG_A; BODY)

#define DEFINE_CB_2_DECODER_SM83(NAME, BODY) \
	DEFINE_CB_OP_DECODER_SM83(NAME, BODY, op2)

#define DEFINE_CB_1_DECODER_SM83(NAME, BODY) \
	DEFINE_CB_OP_DECODER_SM83(NAME, BODY, op1)

#define DEFINE_CB_DECODER_SM83(NAME, BODY) \
	DEFINE_CB_2_DECODER_SM83(NAME ## 0, info->op1.immediate = 0; BODY) \
	DEFINE_CB_2_DECODER_SM83(NAME ## 1, info->op1.immediate = 1; BODY) \
	DEFINE_CB_2_DECODER_SM83(NAME ## 2, info->op1.immediate = 2; BODY) \
	DEFINE_CB_2_DECODER_SM83(NAME ## 3, info->op1.immediate = 3; BODY) \
	DEFINE_CB_2_DECODER_SM83(NAME ## 4, info->op1.immediate = 4; BODY) \
	DEFINE_CB_2_DECODER_SM83(NAME ## 5, info->op1.immediate = 5; BODY) \
	DEFINE_CB_2_DECODER_SM83(NAME ## 6, info->op1.immediate = 6; BODY) \
	DEFINE_CB_2_DECODER_SM83(NAME ## 7, info->op1.immediate = 7; BODY)

DEFINE_CB_DECODER_SM83(BIT, info->mnemonic = SM83_MN_BIT)
DEFINE_CB_DECODER_SM83(RES, info->mnemonic = SM83_MN_RES)
DEFINE_CB_DECODER_SM83(SET, info->mnemonic = SM83_MN_SET)

#define DEFINE_CB_X_DECODER_SM83(NAME) \
	DEFINE_CB_1_DECODER_SM83(NAME, info->mnemonic = SM83_MN_ ## NAME) \
	DEFINE_DECODER_SM83(NAME ## A_, info->mnemonic = SM83_MN_ ## NAME; \
		info->op1.flags = SM83_OP_FLAG_IMPLICIT; \
		info->op1.reg = SM83_REG_A;)

DEFINE_CB_X_DECODER_SM83(RL)
DEFINE_CB_X_DECODER_SM83(RLC)
DEFINE_CB_X_DECODER_SM83(RR)
DEFINE_CB_X_DECODER_SM83(RRC)
DEFINE_CB_1_DECODER_SM83(SLA, info->mnemonic = SM83_MN_SLA)
DEFINE_CB_1_DECODER_SM83(SRA, info->mnemonic = SM83_MN_SRA)
DEFINE_CB_1_DECODER_SM83(SRL, info->mnemonic = SM83_MN_SRL)
DEFINE_CB_1_DECODER_SM83(SWAP, info->mnemonic = SM83_MN_SWAP)

DEFINE_DECODER_SM83(DI, info->mnemonic = SM83_MN_DI)
DEFINE_DECODER_SM83(EI, info->mnemonic = SM83_MN_EI)
DEFINE_DECODER_SM83(HALT, info->mnemonic = SM83_MN_HALT)
DEFINE_DECODER_SM83(ILL, info->mnemonic = SM83_MN_ILL)
DEFINE_DECODER_SM83(STOP, info->mnemonic = SM83_MN_STOP)

#define DEFINE_RST_DECODER_SM83(VEC) \
	DEFINE_DECODER_SM83(RST ## VEC, info->op1.immediate = 0x ## VEC;)

DEFINE_RST_DECODER_SM83(00);
DEFINE_RST_DECODER_SM83(08);
DEFINE_RST_DECODER_SM83(10);
DEFINE_RST_DECODER_SM83(18);
DEFINE_RST_DECODER_SM83(20);
DEFINE_RST_DECODER_SM83(28);
DEFINE_RST_DECODER_SM83(30);
DEFINE_RST_DECODER_SM83(38);

DEFINE_DECODER_SM83(CB, return 1)

const SM83Decoder _sm83DecoderTable[0x100] = {
	DECLARE_SM83_EMITTER_BLOCK(_SM83Decode)
};

const SM83Decoder _sm83CBDecoderTable[0x100] = {
	DECLARE_SM83_CB_EMITTER_BLOCK(_SM83Decode)
};

size_t SM83Decode(uint8_t opcode, struct SM83InstructionInfo* info) {
	if (info->opcodeSize == sizeof(info->opcode)) {
		return 0;
	}
	info->opcode[info->opcodeSize] = opcode;
	SM83Decoder decoder;
	switch (info->opcodeSize) {
	case 0:
		decoder = _sm83DecoderTable[opcode];
		break;
	case 1:
		if (info->opcode[0] == 0xCB) {
			decoder = _sm83CBDecoderTable[opcode];
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
	default:
		// Should never be reached
		abort();
	}
	++info->opcodeSize;
	return decoder(opcode, info);
}

#define ADVANCE(AMOUNT) \
	if (AMOUNT >= blen) { \
		buffer[blen - 1] = '\0'; \
		return total; \
	} \
	total += AMOUNT; \
	buffer += AMOUNT; \
	blen -= AMOUNT;

static const char* _sm83Conditions[] = {
	NULL,
	"c",
	"z",
	"nc",
	"nz",
};

static const char* _sm83Registers[] = {
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

static const char* _sm83MnemonicStrings[] = {
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


static int _decodeOperand(struct SM83Operand op, uint16_t pc, char* buffer, int blen) {
	int total = 0;
	if (op.flags & SM83_OP_FLAG_IMPLICIT) {
		return 0;
	}

	strlcpy(buffer, " ", blen);
	ADVANCE(1);

	if (op.flags & SM83_OP_FLAG_MEMORY) {
		strlcpy(buffer, "[", blen);
		ADVANCE(1);
	}
	if (op.reg) {
		int written = snprintf(buffer, blen, "%s", _sm83Registers[op.reg]);
		ADVANCE(written);
	} else {
		int written;
		if (op.flags & SM83_OP_FLAG_RELATIVE) {
			written = snprintf(buffer, blen, "$%04X", pc + (int8_t) op.immediate);
		} else {
			written = snprintf(buffer, blen, "$%02X", op.immediate);
		}
		ADVANCE(written);
		if (op.reg) {
			strlcpy(buffer, "+", blen);
			ADVANCE(1);
		}
	}
	if (op.flags & SM83_OP_FLAG_INCREMENT) {
		strlcpy(buffer, "+", blen);
		ADVANCE(1);
	}
	if (op.flags & SM83_OP_FLAG_DECREMENT) {
		strlcpy(buffer, "-", blen);
		ADVANCE(1);
	}
	if (op.flags & SM83_OP_FLAG_MEMORY) {
		strlcpy(buffer, "]", blen);
		ADVANCE(1);
	}
	return total;
}

int SM83Disassemble(struct SM83InstructionInfo* info, uint16_t pc, char* buffer, int blen) {
	const char* mnemonic = _sm83MnemonicStrings[info->mnemonic];
	int written;
	int total = 0;
	const char* cond = _sm83Conditions[info->condition];

	written = snprintf(buffer, blen, "%s", mnemonic);
	ADVANCE(written);

	if (cond) {
		written = snprintf(buffer, blen, " %s", cond);
		ADVANCE(written);

		if (info->op1.reg || info->op1.immediate || info->op2.reg || info->op2.immediate) {
			strlcpy(buffer, ",", blen);
			ADVANCE(1);
		}
	}

	if (info->op1.reg || info->op1.immediate || info->op2.reg || info->op2.immediate) {
		written = _decodeOperand(info->op1, pc, buffer, blen);
		ADVANCE(written);
	}

	if (info->op2.reg || (!info->op1.immediate && info->opcodeSize > 1 && info->opcode[0] != 0xCB)) {
		if (written) {
			strlcpy(buffer, ",", blen);
			ADVANCE(1);
		}
		written = _decodeOperand(info->op2, pc, buffer, blen);
		ADVANCE(written);
	}

	buffer[blen - 1] = '\0';
	return total;
}
