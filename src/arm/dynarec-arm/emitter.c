/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "arm/decoder.h"
#include "arm/dynarec.h"
#include "arm/isa-thumb.h"
#include "arm/dynarec-arm/emitter.h"

#define OP_I     0x02000000
#define OP_S     0x00100000
#define OP_ADD   0x00800000
#define OP_AND   0x00000000
#define OP_B     0x0A000000
#define OP_BIC   0x01C00000
#define OP_BL    0x0B000000
#define OP_CMP   0x01500000
#define OP_EOR   0x00200000
#define OP_LDMIA 0x08900000
#define OP_LDRI  0x05100000
#define OP_MOV   0x01A00000
#define OP_MOVT  0x03400000
#define OP_MOVW  0x03000000
#define OP_MRS   0x010F0000
#define OP_ORR   0x01800000
#define OP_POP   0x08BD0000
#define OP_PUSH  0x092D0000
#define OP_STMIA 0x08800000
#define OP_STRI  0x05000000
#define OP_STRBI 0x05400000
#define OP_SUB   0x00400000

#define ADDR1_LSRI 0x00000020

uint32_t calculateAddrMode1(unsigned imm) {
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

uint32_t emitADDI(unsigned dst, unsigned src, unsigned imm) {
	return OP_ADD | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitADDS(unsigned dst, unsigned src, unsigned op2) {
	return OP_ADD | OP_S | (dst << 12) | (src << 16) | op2;
}

uint32_t emitADDSI(unsigned dst, unsigned src, unsigned imm) {
	return OP_ADD | OP_S | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitANDI(unsigned dst, unsigned src, unsigned imm) {
	return OP_AND | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitANDS(unsigned dst, unsigned src, unsigned op2) {
	return OP_AND | OP_S | (dst << 12) | (src << 16) | op2;
}

uint32_t emitANDSI(unsigned dst, unsigned src, unsigned imm) {
	return OP_AND | OP_S | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitBICI(unsigned dst, unsigned src, unsigned imm) {
	return OP_BIC | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitBICS(unsigned dst, unsigned src, unsigned op2) {
	return OP_BIC | OP_S | (dst << 12) | (src << 16) | op2;
}

uint32_t emitBICSI(unsigned dst, unsigned src, unsigned imm) {
	return OP_BIC | OP_S | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
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

uint32_t emitCMP(unsigned src1, unsigned src2) {
	return OP_CMP | src2 | (src1 << 16);
}

uint32_t emitEORI(unsigned dst, unsigned src, unsigned imm) {
	return OP_EOR | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitEORS(unsigned dst, unsigned src, unsigned op2) {
	return OP_EOR | OP_S | (dst << 12) | (src << 16) | op2;
}

uint32_t emitEORSI(unsigned dst, unsigned src, unsigned imm) {
	return OP_EOR | OP_S | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitLDMIA(unsigned base, unsigned mask) {
	return OP_LDMIA | (base << 16) | mask;
}

uint32_t emitLDRI(unsigned reg, unsigned base, int offset) {
	uint32_t op = OP_LDRI | (base << 16) | (reg << 12);
	if (offset > 0) {
		op |= 0x00800000 | offset;
	} else {
		op |= -offset & 0xFFF;
	}
	return op;
}

uint32_t emitMOV(unsigned dst, unsigned src) {
	return OP_MOV | (dst << 12) | src;
}

uint32_t emitMOV_LSRI(unsigned dst, unsigned src, unsigned imm) {
	return OP_MOV | ADDR1_LSRI | (dst << 12) | ((imm  & 0x1F) << 7) | src;
}

uint32_t emitMOVT(unsigned dst, uint16_t value) {
	return OP_MOVT | (dst << 12) | ((value & 0xF000) << 4) | (value & 0x0FFF);
}

uint32_t emitMOVW(unsigned dst, uint16_t value) {
	return OP_MOVW | (dst << 12) | ((value & 0xF000) << 4) | (value & 0x0FFF);
}

uint32_t emitMRS(unsigned dst) {
	return OP_MRS | (dst << 12);
}

uint32_t emitORRI(unsigned dst, unsigned src, unsigned imm) {
	return OP_ORR | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitORRS(unsigned dst, unsigned src, unsigned op2) {
	return OP_ORR | OP_S | (dst << 12) | (src << 16) | op2;
}

uint32_t emitORRSI(unsigned dst, unsigned src, unsigned imm) {
	return OP_ORR | OP_S | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitPOP(unsigned mask) {
	return OP_POP | mask;
}

uint32_t emitPUSH(unsigned mask) {
	return OP_PUSH | mask;
}

uint32_t emitSTMIA(unsigned base, unsigned mask) {
	return OP_STMIA | (base << 16) | mask;
}

uint32_t emitSTRI(unsigned reg, unsigned base, int offset) {
	uint32_t op = OP_STRI | (base << 16) | (reg << 12);
	if (offset > 0) {
		op |= 0x00800000 | offset;
	} else {
		op |= -offset & 0xFFF;
	}
	return op;
}

uint32_t emitSTRBI(unsigned reg, unsigned base, int offset) {
	uint32_t op = OP_STRBI | (base << 16) | (reg << 12);
	if (offset > 0) {
		op |= 0x00800000 | offset;
	} else {
		op |= -offset & 0xFFF;
	}
	return op;
}

uint32_t emitSUBI(unsigned dst, unsigned src, unsigned imm) {
	return OP_SUB | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

uint32_t emitSUBS(unsigned dst, unsigned src1, unsigned src2) {
	return OP_SUB | OP_S | (dst << 12) | (src1 << 16) | src2;
}

uint32_t emitSUBSI(unsigned dst, unsigned src, unsigned imm) {
	return OP_SUB | OP_S | OP_I | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

void updatePC(struct ARMDynarecContext* ctx, uint32_t address) {
	EMIT_IMM(ctx, AL, 5, address);
	EMIT(ctx, STRI, AL, 5, 4, ARM_PC * sizeof(uint32_t));
}

void updateEvents(struct ARMDynarecContext* ctx, struct ARMCore* cpu) {
	EMIT(ctx, ADDI, AL, 0, 4, offsetof(struct ARMCore, cycles));
	EMIT(ctx, LDMIA, AL, 0, 6);
	EMIT(ctx, SUBS, AL, 0, 2, 1);
	EMIT(ctx, MOV, AL, 0, 4);
	EMIT(ctx, BL, LE, ctx->code, cpu->irqh.processEvents);
	EMIT(ctx, LDRI, AL, 1, 4, ARM_PC * sizeof(uint32_t));
	EMIT(ctx, CMP, AL, 1, 5);
	EMIT(ctx, POP, NE, 0x8030);
}

void flushPrefetch(struct ARMDynarecContext* ctx, uint32_t op0, uint32_t op1) {
	EMIT_IMM(ctx, AL, 1, op0);
	EMIT_IMM(ctx, AL, 2, op1);
	EMIT(ctx, ADDI, AL, 0, 4, offsetof(struct ARMCore, prefetch));
	EMIT(ctx, STMIA, AL, 0, 6);
}

void loadReg(struct ARMDynarecContext* ctx, unsigned emureg, unsigned sysreg) {
	EMIT(ctx, LDRI, AL, sysreg, 4, emureg * sizeof(uint32_t)); 
}

void flushReg(struct ARMDynarecContext* ctx, unsigned emureg, unsigned sysreg) {
	EMIT(ctx, STRI, AL, sysreg, 4, emureg * sizeof(uint32_t)); 
}

void flushCycles(struct ARMDynarecContext* ctx) {
	if (ctx->cycles == 0) {
		return;
	}
	EMIT(ctx, LDRI, AL, 0, 4, offsetof(struct ARMCore, cycles)); 
	EMIT(ctx, ADDI, AL, 0, 0, ctx->cycles);
	EMIT(ctx, STRI, AL, 0, 4, offsetof(struct ARMCore, cycles));
	ctx->cycles = 0;
}
