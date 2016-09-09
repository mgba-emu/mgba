/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_DYNAREC_EMITTER_H
#define ARM_DYNAREC_EMITTER_H

#include "util/common.h"

#define REG_ARMCore  0
#define REG_SCRATCH0 1
#define REG_SCRATCH1 2
#define REG_SCRATCH2 3
#define REG_NZCV_TMP 3
#define REG_GUEST_R0 4
#define REG_GUEST_R1 5
#define REG_GUEST_R2 6
#define REG_GUEST_R3 7
#define REG_GUEST_R4 8
#define REG_GUEST_R5 10
#define REG_GUEST_R6 11
#define REG_GUEST_R7 12
#define REG_CYCLES   14
#define REGLIST_SAVE 0x1001
#define REGLIST_RETURN 0x8001
#define REGLIST_GUESTREGS 0x1DF0

#define COND_EQ 0x00000000
#define COND_NE 0x10000000
#define COND_CS 0x20000000
#define COND_CC 0x30000000
#define COND_MI 0x40000000
#define COND_PL 0x50000000
#define COND_VS 0x60000000
#define COND_VC 0x70000000
#define COND_HI 0x80000000
#define COND_LS 0x90000000
#define COND_GE 0xA0000000
#define COND_LT 0xB0000000
#define COND_GT 0xC0000000
#define COND_LE 0xD0000000
#define COND_AL 0xE0000000

typedef uint32_t code_t;

enum ARMDynarecNZCVLocation {
	CONTEXT_NZCV_IN_HOST,
	CONTEXT_NZCV_IN_TMPREG,
	CONTEXT_NZCV_IN_MEMORY,
};

enum ARMDynarecScratchState {
	SCRATCH_STATE_EMPTY = 0,
	SCRATCH_STATE_DEF = 1,
	SCRATCH_STATE_USE = 2,
};

struct ARMDynarecContext {
	code_t* code;

	bool cycles_register_valid;
	int32_t cycles;

	struct {
		enum ARMDynarecScratchState state;
		unsigned guest_reg;
	} scratch[3];

	bool gpr_15_flushed;
	uint32_t gpr_15; //!< The value that would be in cpu->gpr[15] if this were the interpreter.

	bool prefetch_flushed;
	uint32_t prefetch[2];

	enum ARMDynarecNZCVLocation nzcv_location;
};

#define EMIT_L(DEST, OPCODE, COND, ...) \
        do { \
		*DEST = emit ## OPCODE (__VA_ARGS__) | COND_ ## COND; \
		++DEST; \
	} while (0)

#define EMIT(CTX, OPCODE, COND, ...) EMIT_L((CTX)->code, OPCODE, COND, __VA_ARGS__)

#define EMIT_IMM(DEST, COND, REG, VALUE) \
	do { \
		EMIT(DEST, MOVW, COND, REG, VALUE); \
		if (VALUE >= 0x10000) { \
			EMIT(DEST, MOVT, COND, REG, (VALUE) >> 16); \
		} \
	} while (0)

#define DECLARE_ALU3_EMITTER(MN) \
	uint32_t emit##MN(unsigned dst, unsigned src, unsigned op2); \
	uint32_t emit##MN##I(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##S(unsigned dst, unsigned src, unsigned op2); \
	uint32_t emit##MN##SI(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##_ASR(unsigned dst, unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##_LSL(unsigned dst, unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##_LSR(unsigned dst, unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##_ROR(unsigned dst, unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##S_ASR(unsigned dst, unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##S_LSL(unsigned dst, unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##S_LSR(unsigned dst, unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##S_ROR(unsigned dst, unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##_ASRI(unsigned dst, unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##_LSLI(unsigned dst, unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##_LSRI(unsigned dst, unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##_RORI(unsigned dst, unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##S_ASRI(unsigned dst, unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##S_LSLI(unsigned dst, unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##S_LSRI(unsigned dst, unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##S_RORI(unsigned dst, unsigned src1, unsigned src2, unsigned imm);

#define DECLARE_ALU2_EMITTER(MN) \
	uint32_t emit##MN(unsigned dst, unsigned op2); \
	uint32_t emit##MN##I(unsigned dst, unsigned imm); \
	uint32_t emit##MN##S(unsigned dst, unsigned op2); \
	uint32_t emit##MN##SI(unsigned dst, unsigned imm); \
	uint32_t emit##MN##_ASR(unsigned dst, unsigned src1, unsigned src2); \
	uint32_t emit##MN##_LSL(unsigned dst, unsigned src1, unsigned src2); \
	uint32_t emit##MN##_LSR(unsigned dst, unsigned src1, unsigned src2); \
	uint32_t emit##MN##_ROR(unsigned dst, unsigned src1, unsigned src2); \
	uint32_t emit##MN##S_ASR(unsigned dst, unsigned src1, unsigned src2); \
	uint32_t emit##MN##S_LSL(unsigned dst, unsigned src1, unsigned src2); \
	uint32_t emit##MN##S_LSR(unsigned dst, unsigned src1, unsigned src2); \
	uint32_t emit##MN##S_ROR(unsigned dst, unsigned src1, unsigned src2); \
	uint32_t emit##MN##_ASRI(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##_LSLI(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##_LSRI(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##_RORI(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##S_ASRI(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##S_LSLI(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##S_LSRI(unsigned dst, unsigned src, unsigned imm); \
	uint32_t emit##MN##S_RORI(unsigned dst, unsigned src, unsigned imm);

#define DECLARE_ALU1_EMITTER(MN) \
	uint32_t emit##MN(unsigned src, unsigned op2); \
	uint32_t emit##MN##I(unsigned src, unsigned imm); \
	uint32_t emit##MN##_ASR(unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##_LSL(unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##_LSR(unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##_ROR(unsigned src1, unsigned src2, unsigned src3); \
	uint32_t emit##MN##_ASRI(unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##_LSLI(unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##_LSRI(unsigned src1, unsigned src2, unsigned imm); \
	uint32_t emit##MN##_RORI(unsigned src1, unsigned src2, unsigned imm);

DECLARE_ALU3_EMITTER(ADC)
DECLARE_ALU3_EMITTER(ADD)
DECLARE_ALU3_EMITTER(AND)
DECLARE_ALU3_EMITTER(BIC)
DECLARE_ALU1_EMITTER(CMN)
DECLARE_ALU1_EMITTER(CMP)
DECLARE_ALU3_EMITTER(EOR)
DECLARE_ALU2_EMITTER(MOV)
DECLARE_ALU2_EMITTER(MVN)
DECLARE_ALU3_EMITTER(ORR)
DECLARE_ALU3_EMITTER(RSB)
DECLARE_ALU3_EMITTER(RSC)
DECLARE_ALU3_EMITTER(SBC)
DECLARE_ALU3_EMITTER(SUB)
DECLARE_ALU1_EMITTER(TEQ)
DECLARE_ALU1_EMITTER(TST)

#undef DECLARE_ALU3_EMITTER
#undef DECLARE_ALU2_EMITTER
#undef DECLARE_ALU1_EMITTER

uint32_t emitMOVT(unsigned dst, uint16_t value);
uint32_t emitMOVW(unsigned dst, uint16_t value);
uint32_t emitSXTB(unsigned dst, unsigned src, unsigned rotation);
uint32_t emitSXTH(unsigned dst, unsigned src, unsigned rotation);

uint32_t emitLDMIA(unsigned base, unsigned mask);
uint32_t emitLDRI(unsigned reg, unsigned base, int offset);
uint32_t emitPOP(unsigned mask);
uint32_t emitPUSH(unsigned mask);
uint32_t emitSTMIA(unsigned base, unsigned mask);
uint32_t emitSTRI(unsigned reg, unsigned base, int offset);
uint32_t emitSTRBI(unsigned reg, unsigned base, int offset);

uint32_t emitB(void* base, void* target);
uint32_t emitBL(void* base, void* target);

uint32_t emitMRS(unsigned dst);
uint32_t emitMSR(bool nzcvq, bool g, unsigned src);

#endif
