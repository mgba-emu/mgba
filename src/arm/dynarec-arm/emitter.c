/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <assert.h>

#include "arm/decoder.h"
#include "arm/dynarec.h"
#include "arm/isa-thumb.h"
#include "arm/dynarec-arm/emitter.h"

#define OP_I       0x02000000
#define OP_S       0x00100000
#define ADDR1_ASRI 0x00000040
#define ADDR1_LSLI 0x00000000
#define ADDR1_LSRI 0x00000020
#define ADDR1_RORI 0x00000060
#define ADDR1_ASR  0x00000050
#define ADDR1_LSL  0x00000010
#define ADDR1_LSR  0x00000030
#define ADDR1_ROR  0x00000070

#define OP_ADC     0x00A00000
#define OP_ADD     0x00800000
#define OP_AND     0x00000000
#define OP_BIC     0x01C00000
#define OP_CMN     0x01700000
#define OP_CMP     0x01500000
#define OP_EOR     0x00200000
#define OP_MOV     0x01A00000
#define OP_MOVT    0x03400000
#define OP_MOVW    0x03000000
#define OP_MVN     0x01E00000
#define OP_ORR     0x01800000
#define OP_RSB     0x00600000
#define OP_RSC     0x00E00000
#define OP_SBC     0x00C00000
#define OP_SUB     0x00400000
#define OP_TEQ     0x01300000
#define OP_TST     0x01100000

#define OP_LDMIA   0x08900000
#define OP_LDRI    0x05100000
#define OP_POP     0x08BD0000
#define OP_PUSH    0x092D0000
#define OP_STMIA   0x08800000
#define OP_STRI    0x05000000
#define OP_STRBI   0x05400000

#define OP_B       0x0A000000
#define OP_BL      0x0B000000

#define OP_MRS   0x010F0000
#define OP_MSR   0x0120F000

static uint32_t calculateAddrMode1(unsigned imm) {
	if (imm < 0x100) {
		return imm;
	}
	int i;
	for (i = 0; i < 16; ++i) {
		unsigned t = ROR(imm, i * 2);
		if (t < 0x100) {
			return t | ((16 - i) << 8);
		}
	}
	abort();
}

#define SHIFT(MN, S, I, ARGS, PREBODY, BODY) \
	uint32_t emit##MN##S##_ASR##I ARGS { PREBODY; return OP_##MN | ADDR1_ASR##I | BODY; } \
	uint32_t emit##MN##S##_LSL##I ARGS { PREBODY; return OP_##MN | ADDR1_LSL##I | BODY; } \
	uint32_t emit##MN##S##_LSR##I ARGS { PREBODY; return OP_##MN | ADDR1_LSR##I | BODY; } \
	uint32_t emit##MN##S##_ROR##I ARGS { PREBODY; return OP_##MN | ADDR1_ROR##I | BODY; }

#define DEFINE_ALU3_EMITTER(MN) \
	uint32_t emit##MN(unsigned dst, unsigned src, unsigned op2) { \
		assert(dst < 16 && src < 16 && op2 < 16); \
		return OP_##MN | (dst << 12) | (src << 16) | op2; \
	} \
	uint32_t emit##MN##I(unsigned dst, unsigned src, unsigned imm) { \
		assert(dst < 16 && src < 16); \
		return OP_##MN | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16); \
	} \
	uint32_t emit##MN##S(unsigned dst, unsigned src, unsigned op2) { \
		assert(dst < 16 && src < 16 && op2 < 16); \
		return OP_##MN | OP_S | (dst << 12) | (src << 16) | op2; \
	} \
	uint32_t emit##MN##SI(unsigned dst, unsigned src, unsigned imm) { \
		assert(dst < 16 && src < 16); \
		return OP_##MN | OP_S | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16); \
	} \
	SHIFT(MN,  ,  , (unsigned dst, unsigned src1, unsigned src2, unsigned src3), \
		assert(dst < 16 && src1 < 16 && src2 < 16 && src3 < 16), \
		(       (dst << 12) | (src1 << 16) | (src3 << 8) | src2)) \
	SHIFT(MN, S,  , (unsigned dst, unsigned src1, unsigned src2, unsigned src3), \
		assert(dst < 16 && src1 < 16 && src2 < 16 && src3 < 16), \
		(OP_S | (dst << 12) | (src1 << 16) | (src3 << 8) | src2)) \
	SHIFT(MN,  , I, (unsigned dst, unsigned src1, unsigned src2, unsigned imm), \
		assert(dst < 16 && src1 < 16 && src2 < 16), \
		(       (dst << 12) | (src1 << 16) | ((imm & 0x1F) << 7) | src2)) \
	SHIFT(MN, S, I, (unsigned dst, unsigned src1, unsigned src2, unsigned imm), \
		assert(dst < 16 && src1 < 16 && src2 < 16), \
		(OP_S | (dst << 12) | (src1 << 16) | ((imm & 0x1F) << 7) | src2))

#define DEFINE_ALU2_EMITTER(MN) \
	uint32_t emit##MN(unsigned dst, unsigned op2) { \
		assert(dst < 16 && op2 < 16); \
		return OP_##MN | (dst << 12) | op2; \
	} \
	uint32_t emit##MN##I(unsigned dst, unsigned imm) { \
		assert(dst < 16); \
		return OP_##MN | OP_I | calculateAddrMode1(imm) | (dst << 12); \
	} \
	uint32_t emit##MN##S(unsigned dst, unsigned op2) { \
		assert(dst < 16 && op2 < 16); \
		return OP_##MN | OP_S | (dst << 12) | op2; \
	} \
	uint32_t emit##MN##SI(unsigned dst, unsigned imm) { \
		assert(dst < 16); \
		return OP_##MN | OP_S | OP_I | calculateAddrMode1(imm) | (dst << 12); \
	} \
	SHIFT(MN,  ,  , (unsigned dst, unsigned src1, unsigned src2), \
		assert(dst < 16 && src1 < 16 && src2 < 16), \
		(       (dst << 12) | (src2 << 8) | src1)) \
	SHIFT(MN, S,  , (unsigned dst, unsigned src1, unsigned src2), \
		assert(dst < 16 && src1 < 16 && src2 < 16), \
		(OP_S | (dst << 12) | (src2 << 8) | src1)) \
	SHIFT(MN,  , I, (unsigned dst, unsigned src1, unsigned imm), \
		assert(dst < 16 && src1 < 16), \
		(       (dst << 12) | ((imm & 0x1F) << 7) | src1)) \
	SHIFT(MN, S, I, (unsigned dst, unsigned src1, unsigned imm), \
		assert(dst < 16 && src1 < 16), \
		(OP_S | (dst << 12) | ((imm & 0x1F) << 7) | src1))

#define DEFINE_ALU1_EMITTER(MN) \
	uint32_t emit##MN(unsigned src, unsigned op2) { \
		assert(src < 16 && op2 < 16); \
		return OP_##MN | (src << 16) | op2; \
	} \
	uint32_t emit##MN##I(unsigned src, unsigned imm) { \
		assert(src < 16); \
		return OP_##MN | OP_I | calculateAddrMode1(imm) | (src << 16); \
	} \
	SHIFT(MN,  ,  , (unsigned src1, unsigned src2, unsigned src3), \
		assert(src1 < 16 && src2 < 16 && src3 < 16), \
		((src1 << 16) | (src3 << 8) | src2)) \
	SHIFT(MN,  , I, (unsigned src1, unsigned src2, unsigned imm), \
		assert(src1 < 16 && src2 < 16), \
		((src1 << 16) | ((imm & 0x1F) << 7) | src2))

DEFINE_ALU3_EMITTER(ADC)
DEFINE_ALU3_EMITTER(ADD)
DEFINE_ALU3_EMITTER(AND)
DEFINE_ALU3_EMITTER(BIC)
DEFINE_ALU1_EMITTER(CMN)
DEFINE_ALU1_EMITTER(CMP)
DEFINE_ALU3_EMITTER(EOR)
DEFINE_ALU2_EMITTER(MOV)
DEFINE_ALU2_EMITTER(MVN)
DEFINE_ALU3_EMITTER(ORR)
DEFINE_ALU3_EMITTER(RSB)
DEFINE_ALU3_EMITTER(RSC)
DEFINE_ALU3_EMITTER(SBC)
DEFINE_ALU3_EMITTER(SUB)
DEFINE_ALU1_EMITTER(TEQ)
DEFINE_ALU1_EMITTER(TST)

#undef DEFINE_ALU3_EMITTER
#undef DEFINE_ALU2_EMITTER
#undef DEFINE_ALU1_EMITTER
#undef SHIFT

uint32_t emitMOVT(unsigned dst, uint16_t value) {
	assert(dst < 16);
	return OP_MOVT | (dst << 12) | ((value & 0xF000) << 4) | (value & 0x0FFF);
}

uint32_t emitMOVW(unsigned dst, uint16_t value) {
	assert(dst < 16);
	return OP_MOVW | (dst << 12) | ((value & 0xF000) << 4) | (value & 0x0FFF);
}

uint32_t emitLDMIA(unsigned base, unsigned mask) {
	assert(base < 16);
	return OP_LDMIA | (base << 16) | mask;
}

uint32_t emitLDRI(unsigned reg, unsigned base, int offset) {
	assert(reg < 16 && base < 16);
	uint32_t op = OP_LDRI | (base << 16) | (reg << 12);
	if (offset > 0) {
		op |= 0x00800000 | offset;
	} else {
		op |= -offset & 0xFFF;
	}
	return op;
}

uint32_t emitPOP(unsigned mask) {
	return OP_POP | mask;
}

uint32_t emitPUSH(unsigned mask) {
	return OP_PUSH | mask;
}

uint32_t emitSTMIA(unsigned base, unsigned mask) {
	assert(base < 16);
	return OP_STMIA | (base << 16) | mask;
}

uint32_t emitSTRI(unsigned reg, unsigned base, int offset) {
	assert(reg < 16 && base < 16);
	uint32_t op = OP_STRI | (base << 16) | (reg << 12);
	if (offset > 0) {
		op |= 0x00800000 | offset;
	} else {
		op |= -offset & 0xFFF;
	}
	return op;
}

uint32_t emitSTRBI(unsigned reg, unsigned base, int offset) {
	assert(reg < 16 && base < 16);
	uint32_t op = OP_STRBI | (base << 16) | (reg << 12);
	if (offset > 0) {
		op |= 0x00800000 | offset;
	} else {
		op |= -offset & 0xFFF;
	}
	return op;
}

uint32_t emitB(void* base, void* target) {
	uint32_t diff = (intptr_t) target - (intptr_t) base - WORD_SIZE_ARM * 2;
	diff >>= 2;
	diff &= 0x00FFFFFF;
	return OP_B | diff;
}

uint32_t emitBL(void* base, void* target) {
	uint32_t diff = (intptr_t) target - (intptr_t) base - WORD_SIZE_ARM * 2;
	diff >>= 2;
	diff &= 0x00FFFFFF;
	return OP_BL | diff;
}

uint32_t emitMRS(unsigned dst) {
	assert(dst < 16);
	return OP_MRS | (dst << 12);
}

uint32_t emitMSR(bool nzcvq, bool g, unsigned src) {
	assert(src < 16);
    return OP_MSR | (nzcvq << 19) | (g << 18) | src;
}
