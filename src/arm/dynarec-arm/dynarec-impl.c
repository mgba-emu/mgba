/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "arm/decoder.h"
#include "arm/dynarec.h"
#include "arm/isa-thumb.h"

#define OP_ADDI  0x02800000
#define OP_BL    0x0B000000
#define OP_CMP   0x01500000
#define OP_LDMIA 0x08900000
#define OP_LDRI  0x05100000
#define OP_MOV   0x01A00000
#define OP_MOVT  0x03400000
#define OP_MOVW  0x03000000
#define OP_POP   0x08BD0000
#define OP_PUSH  0x092D0000
#define OP_STMIA 0x08800000
#define OP_STRI  0x05000000
#define OP_SUBS  0x00500000

#define COND_EQ 0x00000000
#define COND_NE 0x10000000
#define COND_MI 0x40000000
#define COND_LE 0xD0000000
#define COND_AL 0xE0000000

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

static uint32_t emitADDI(unsigned dst, unsigned src, unsigned imm) {
	return OP_ADDI | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

static uint32_t emitBL(void* base, void* target) {
	uint32_t diff = (intptr_t) target - (intptr_t) base - WORD_SIZE_ARM * 2;
	diff >>= 2;
	diff &= 0x00FFFFFF;
	return OP_BL | diff;
}

static uint32_t emitCMP(unsigned src1, unsigned src2) {
	return OP_CMP | src2 | (src1 << 16);
}

static uint32_t emitLDMIA(unsigned base, unsigned mask) {
	return OP_LDMIA | (base << 16) | mask;
}

static uint32_t emitLDRI(unsigned reg, unsigned base, int offset) {
	uint32_t op = OP_LDRI | (base << 16) | (reg << 12);
	if (offset > 0) {
		op |= 0x00800000 | offset;
	} else {
		op |= -offset & 0xFFF;
	}
	return op;
}

static uint32_t emitMOV(unsigned dst, unsigned src) {
	return OP_MOV | (dst << 12) | src;
}

static uint32_t emitMOVT(unsigned dst, uint16_t value) {
	return OP_MOVT | (dst << 12) | ((value & 0xF000) << 4) | (value & 0x0FFF);
}

static uint32_t emitMOVW(unsigned dst, uint16_t value) {
	return OP_MOVW | (dst << 12) | ((value & 0xF000) << 4) | (value & 0x0FFF);
}

static uint32_t emitPOP(unsigned mask) {
	return OP_POP | mask;
}

static uint32_t emitPUSH(unsigned mask) {
	return OP_PUSH | mask;
}

static uint32_t emitSTMIA(unsigned base, unsigned mask) {
	return OP_STMIA | (base << 16) | mask;
}

static uint32_t emitSTRI(unsigned reg, unsigned base, int offset) {
	uint32_t op = OP_STRI | (base << 16) | (reg << 12);
	if (offset > 0) {
		op |= 0x00800000 | offset;
	} else {
		op |= -offset & 0xFFF;
	}
	return op;
}

static uint32_t emitSUBS(unsigned dst, unsigned src1, unsigned src2) {
	return OP_SUBS | (dst << 12) | (src1 << 16) | src2;
}

static uint32_t* updatePC(uint32_t* code, uint32_t address) {
	*code++ = emitMOVW(5, address) | COND_AL;
	*code++ = emitMOVT(5, address >> 16) | COND_AL;
	*code++ = emitSTRI(5, 4, ARM_PC * sizeof(uint32_t)) | COND_AL;
	return code;
}

static uint32_t* updateEvents(uint32_t* code, struct ARMCore* cpu) {
	*code++ = emitADDI(0, 4, offsetof(struct ARMCore, cycles)) | COND_AL;
	*code++ = emitLDMIA(0, 6) | COND_AL;
	*code++ = emitSUBS(0, 2, 1) | COND_AL;
	*code++ = emitMOV(0, 4) | COND_AL;
	*code = emitBL(code, cpu->irqh.processEvents) | COND_LE;
	++code;
	*code++ = emitLDRI(1, 4, ARM_PC * sizeof(uint32_t)) | COND_AL;
	*code++ = emitCMP(1, 5) | COND_AL;
	*code++ = emitPOP(0x8030) | COND_NE;
	return code;
}

static uint32_t* flushPrefetch(uint32_t* code, uint32_t op0, uint32_t op1) {
	*code++ = emitMOVW(1, op0) | COND_EQ;
	if (op0 >= 0x10000) {
		*code++ = emitMOVT(1, op0 >> 16) | COND_EQ;
	}
	*code++ = emitMOVW(2, op1) | COND_EQ;
	if (op1 >= 0x10000) {
		*code++ = emitMOVT(2, op1 >> 16) | COND_EQ;
	}
	*code++ = emitADDI(0, 4, offsetof(struct ARMCore, prefetch)) | COND_EQ;
	*code++ = emitSTMIA(0, 6) | COND_EQ;
	return code;
}

static bool needsUpdateEvents(struct ARMInstructionInfo* info) {
	if (info->operandFormat & ARM_OPERAND_MEMORY) {
		return true;
	}
	if (info->branchType || info->traps) {
		return true;
	}
	return false;
}

static bool needsUpdatePC(struct ARMInstructionInfo* info) {
	if (needsUpdateEvents(info)) {
		return true;
	}
	if (info->operandFormat & ARM_OPERAND_REGISTER_1 && info->op1.reg == ARM_PC) {
		return true;
	}
	if (info->operandFormat & ARM_OPERAND_REGISTER_2 && info->op2.reg == ARM_PC) {
		return true;
	}
	if (info->operandFormat & ARM_OPERAND_REGISTER_3 && info->op3.reg == ARM_PC) {
		return true;
	}
	if (info->operandFormat & ARM_OPERAND_REGISTER_4 && info->op4.reg == ARM_PC) {
		return true;
	}
	if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_1 && info->op1.shifterReg == ARM_PC) {
		return true;
	}
	if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_2 && info->op2.shifterReg == ARM_PC) {
		return true;
	}
	if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_3 && info->op3.shifterReg == ARM_PC) {
		return true;
	}
	if (info->operandFormat & ARM_OPERAND_SHIFT_REGISTER_4 && info->op4.shifterReg == ARM_PC) {
		return true;
	}
	return false;
}

void ARMDynarecRecompileTrace(struct ARMCore* cpu, struct ARMDynarecTrace* trace) {
#ifndef NDEBUG
	printf("%08X (%c)\n", trace->start, trace->mode == MODE_THUMB ? 'T' : 'A');
#endif
	uint32_t* code = cpu->dynarec.buffer;
	uint32_t address = trace->start;
	if (trace->mode == MODE_ARM) {
		return;
	} else {
		trace->entry = (void (*)(struct ARMCore*)) code;
		*code++ = emitPUSH(0x4030) | COND_AL;
		*code++ = emitMOV(4, 0) | COND_AL;
		*code++ = emitLDRI(5, 0, ARM_PC * sizeof(uint32_t)) | COND_AL;
		struct ARMInstructionInfo info;
		while (true) {
			uint16_t instruction = cpu->memory.load16(cpu, address, 0);
			ARMDecodeThumb(instruction, &info);
			address += WORD_SIZE_THUMB;
			if (needsUpdatePC(&info)) {
				code = updatePC(code, address + WORD_SIZE_THUMB);
			}
			*code++ = emitMOVW(1, instruction) | COND_AL;
			*code = emitBL(code, _thumbTable[instruction >> 6]) | COND_AL;
			++code;
			if (info.branchType == ARM_BRANCH) {
				// Assume branch not taken
				if (info.condition == ARM_CONDITION_AL) {
					code = updateEvents(code, cpu);
					break;
				}
				*code++ = emitMOVW(5, address + WORD_SIZE_THUMB) | COND_AL;
				*code++ = emitMOVT(5, (address + WORD_SIZE_THUMB) >> 16) | COND_AL;
				code = updateEvents(code, cpu);
			} else if (needsUpdateEvents(&info)) {
				code = updateEvents(code, cpu);
			}
			if (info.branchType > ARM_BRANCH || info.traps) {
				break;
			}
			*code++ = emitMOV(0, 4) | COND_AL;
		}
		code = flushPrefetch(code, cpu->memory.load16(cpu, address, 0), cpu->memory.load16(cpu, address + WORD_SIZE_THUMB, 0));
		*code++ = emitPOP(0x8030) | COND_AL;
	}
	__clear_cache(trace->entry, code);
	cpu->dynarec.buffer = code;
}
