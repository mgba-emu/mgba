/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "arm/decoder.h"
#include "arm/dynarec.h"
#include "arm/isa-thumb.h"

#define OP_ADDI  0x02800000
#define OP_ADDS  0x00900000
#define OP_ADDSI 0x02900000
#define OP_B     0x0A000000
#define OP_BL    0x0B000000
#define OP_CMP   0x01500000
#define OP_LDMIA 0x08900000
#define OP_LDRI  0x05100000
#define OP_MOV   0x01A00000
#define OP_MOVT  0x03400000
#define OP_MOVW  0x03000000
#define OP_MRS   0x010F0000
#define OP_POP   0x08BD0000
#define OP_PUSH  0x092D0000
#define OP_STMIA 0x08800000
#define OP_STRI  0x05000000
#define OP_STRBI 0x05400000
#define OP_SUBS  0x00500000

#define ADDR1_LSRI 0x00000020

#define COND_EQ 0x00000000
#define COND_NE 0x10000000
#define COND_MI 0x40000000
#define COND_LE 0xD0000000
#define COND_AL 0xE0000000

#define EMIT(DEST, OPCODE, COND, ...) \
	*DEST = emit ## OPCODE (__VA_ARGS__) | COND_ ## COND; \
	++DEST;

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

static uint32_t emitADDS(unsigned dst, unsigned src, unsigned op2) {
	return OP_ADDS | (dst << 12) | (src << 16) | op2;
}

static uint32_t emitADDSI(unsigned dst, unsigned src, unsigned imm) {
	return OP_ADDSI | calculateAddrMode1(imm) | (dst << 12) | (src << 16);
}

static uint32_t emitB(void* base, void* target) {
	uint32_t diff = (intptr_t) target - (intptr_t) base - WORD_SIZE_ARM * 2;
	diff >>= 2;
	diff &= 0x00FFFFFF;
	return OP_B | diff;
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

static uint32_t emitMOV_LSRI(unsigned dst, unsigned src, unsigned imm) {
	return OP_MOV | ADDR1_LSRI | (dst << 12) | ((imm  & 0x1F) << 7) | src;
}

static uint32_t emitMOVT(unsigned dst, uint16_t value) {
	return OP_MOVT | (dst << 12) | ((value & 0xF000) << 4) | (value & 0x0FFF);
}

static uint32_t emitMOVW(unsigned dst, uint16_t value) {
	return OP_MOVW | (dst << 12) | ((value & 0xF000) << 4) | (value & 0x0FFF);
}

static uint32_t emitMRS(unsigned dst) {
	return OP_MRS | (dst << 12);
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

static uint32_t emitSTRBI(unsigned reg, unsigned base, int offset) {
	uint32_t op = OP_STRBI | (base << 16) | (reg << 12);
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

#define EMIT_IMM(DEST, COND, REG, VALUE) \
	EMIT(DEST, MOVW, COND, REG, VALUE); \
	if (VALUE >= 0x10000) { \
		EMIT(DEST, MOVT, COND, REG, (VALUE) >> 16); \
	}

static uint32_t* updatePC(uint32_t* code, uint32_t address) {
	EMIT_IMM(code, AL, 5, address);
	EMIT(code, STRI, AL, 5, 4, ARM_PC * sizeof(uint32_t));
	return code;
}

static uint32_t* updateEvents(uint32_t* code, struct ARMCore* cpu) {
	EMIT(code, ADDI, AL, 0, 4, offsetof(struct ARMCore, cycles));
	EMIT(code, LDMIA, AL, 0, 6);
	EMIT(code, SUBS, AL, 0, 2, 1);
	EMIT(code, MOV, AL, 0, 4);
	EMIT(code, BL, LE, code, cpu->irqh.processEvents);
	EMIT(code, LDRI, AL, 1, 4, ARM_PC * sizeof(uint32_t));
	EMIT(code, CMP, AL, 1, 5);
	EMIT(code, POP, NE, 0x8030);
	return code;
}

static uint32_t* flushPrefetch(uint32_t* code, uint32_t op0, uint32_t op1) {
	EMIT_IMM(code, AL, 1, op0);
	EMIT_IMM(code, AL, 2, op1);
	EMIT(code, ADDI, AL, 0, 4, offsetof(struct ARMCore, prefetch));
	EMIT(code, STMIA, AL, 0, 6);
	return code;
}

static uint32_t* loadReg(uint32_t* code, unsigned emureg, unsigned sysreg) {
	EMIT(code, LDRI, AL, sysreg, 4, emureg * sizeof(uint32_t)); 
	return code;
}

static uint32_t* flushReg(uint32_t* code, unsigned emureg, unsigned sysreg) {
	EMIT(code, STRI, AL, sysreg, 4, emureg * sizeof(uint32_t)); 
	return code;
}

static bool needsUpdatePrefetch(struct ARMInstructionInfo* info) {
	if ((info->operandFormat & (ARM_OPERAND_MEMORY_1 | ARM_OPERAND_AFFECTED_1)) == ARM_OPERAND_MEMORY_1) {
		return true;
	}
	if ((info->operandFormat & (ARM_OPERAND_MEMORY_2 | ARM_OPERAND_AFFECTED_2)) == ARM_OPERAND_MEMORY_2) {
		return true;
	}
	if ((info->operandFormat & (ARM_OPERAND_MEMORY_3 | ARM_OPERAND_AFFECTED_3)) == ARM_OPERAND_MEMORY_3) {
		return true;
	}
	if ((info->operandFormat & (ARM_OPERAND_MEMORY_4 | ARM_OPERAND_AFFECTED_4)) == ARM_OPERAND_MEMORY_4) {
		return true;
	}
	return false;
}

static bool needsUpdateEvents(struct ARMInstructionInfo* info) {
	if ((info->operandFormat & (ARM_OPERAND_MEMORY_1 | ARM_OPERAND_AFFECTED_1)) == (ARM_OPERAND_MEMORY_1 | ARM_OPERAND_AFFECTED_1)) {
		return true;
	}
	if ((info->operandFormat & (ARM_OPERAND_MEMORY_2 | ARM_OPERAND_AFFECTED_2)) == (ARM_OPERAND_MEMORY_2 | ARM_OPERAND_AFFECTED_2)) {
		return true;
	}
	if ((info->operandFormat & (ARM_OPERAND_MEMORY_3 | ARM_OPERAND_AFFECTED_3)) == (ARM_OPERAND_MEMORY_3 | ARM_OPERAND_AFFECTED_3)) {
		return true;
	}
	if ((info->operandFormat & (ARM_OPERAND_MEMORY_4 | ARM_OPERAND_AFFECTED_4)) == (ARM_OPERAND_MEMORY_4 | ARM_OPERAND_AFFECTED_4)) {
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
	if (info->operandFormat & ARM_OPERAND_MEMORY && info->memory.format & ARM_MEMORY_REGISTER_BASE && info->memory.baseReg == ARM_PC) {
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
	struct Label {
		uint32_t* code;
		uint32_t pc;
	}* labels = cpu->dynarec.temporaryMemory;
	if (trace->mode == MODE_ARM) {
		return;
	} else {
		trace->entry = (void (*)(struct ARMCore*)) code;
		EMIT(code, PUSH, AL, 0x4030);
		EMIT(code, MOV, AL, 4, 0);
		EMIT(code, LDRI, AL, 5, 0, ARM_PC * sizeof(uint32_t));
		struct ARMInstructionInfo info;
		while (true) {
			uint16_t instruction = cpu->memory.load16(cpu, address, 0);
			struct Label* label = &labels[(address - trace->start) >> 1];
			ARMDecodeThumb(instruction, &info);
			address += WORD_SIZE_THUMB;
			label->code = code;
			label->pc = address + WORD_SIZE_THUMB;
			if (needsUpdatePC(&info)) {
				code = updatePC(code, address + WORD_SIZE_THUMB);
			}
			if (needsUpdatePrefetch(&info)) {
				code = flushPrefetch(code, cpu->memory.load16(cpu, address, 0), cpu->memory.load16(cpu, address + WORD_SIZE_THUMB, 0));
			}

			unsigned rd = 0;
			unsigned rn = 1;
			unsigned rm = 2;
			switch (info.mnemonic) {
			case ARM_MN_ADD:
				if (info.operandFormat & ARM_OPERAND_REGISTER_2) {
					code = loadReg(code, info.op2.reg, rn);
				}
				if (info.operandFormat & ARM_OPERAND_REGISTER_3) {
					code = loadReg(code, info.op3.reg, rm);
				}
				switch (info.operandFormat & (ARM_OPERAND_2 | ARM_OPERAND_3)) {
				case ARM_OPERAND_REGISTER_2 | ARM_OPERAND_REGISTER_3:
					EMIT(code, ADDS, AL, rd, rn, rm);
					break;
				case ARM_OPERAND_REGISTER_2 | ARM_OPERAND_IMMEDIATE_3:
					EMIT(code, ADDSI, AL, rd, rn, info.op3.immediate);
					break;
				case ARM_OPERAND_IMMEDIATE_2:
					code = loadReg(code, info.op1.reg, rd);
					EMIT(code, ADDSI, AL, rd, rd, info.op2.immediate);
					break;
				case ARM_OPERAND_REGISTER_2:
					code = loadReg(code, info.op1.reg, rd);
					EMIT(code, ADDS, AL, rd, rd, rn);
					break;
				default:
					abort();
				}
				code = flushReg(code, info.op1.reg, rd);
				if (info.affectsCPSR) {
					EMIT(code, MRS, AL, 1);
					EMIT(code, MOV_LSRI, AL, 1, 1, 28);
					EMIT(code, STRBI, AL, 1, 4, 16 * sizeof(uint32_t) + 3);
				}
				break;
			default:
				EMIT(code, MOVW, AL, 1, instruction);
				EMIT(code, MOV, AL, 0, 4);
				EMIT(code, BL, AL, code, _thumbTable[instruction >> 6]);
				break;
			}
			if (info.branchType == ARM_BRANCH) {
				struct Label* label = NULL;
				uint32_t base = address + info.op1.immediate + WORD_SIZE_THUMB;
				if (info.op1.immediate <= 0) {
					if (base > trace->start) {
						label = &labels[(base - trace->start) >> 1];
					}
				}
				// Assume branch not taken
				if (info.condition == ARM_CONDITION_AL) {
					code = updateEvents(code, cpu);
					break;
				}
				EMIT(code, MOVW, AL, 5, address + WORD_SIZE_THUMB);
				EMIT(code, MOVT, AL, 5, (address + WORD_SIZE_THUMB) >> 16);
				EMIT(code, LDRI, AL, 1, 4, ARM_PC * sizeof(uint32_t));
				EMIT(code, CMP, AL, 1, 5);
				if (!label || !label->code) {
					EMIT(code, POP, NE, 0x8030);
				} else {
					uint32_t* l2 = code;
					++code;
					EMIT(code, MOV, AL, 5, 1);
					code = updateEvents(code, cpu);
					EMIT(code, B, AL, code, label->code);
					EMIT(l2, B, EQ, l2, code);
				}
			} else if (needsUpdateEvents(&info)) {
				code = updateEvents(code, cpu);
			}
			if (info.branchType > ARM_BRANCH || info.traps) {
				break;
			}
		}
		memset(labels, 0, sizeof(struct Label) * ((address - trace->start) >> 1));
		code = flushPrefetch(code, cpu->memory.load16(cpu, address, 0), cpu->memory.load16(cpu, address + WORD_SIZE_THUMB, 0));
		EMIT(code, POP, AL, 0x8030);
	}
	__clear_cache(trace->entry, code);
	cpu->dynarec.buffer = code;
}
