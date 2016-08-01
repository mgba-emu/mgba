/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "arm/decoder.h"
#include "arm/dynarec.h"
#include "arm/isa-thumb.h"
#include "arm/dynarec-arm/emitter.h"

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

#define RECOMPILE_ALU(MN) \
	if (info.operandFormat & ARM_OPERAND_REGISTER_2) { \
		loadReg(&ctx, info.op2.reg, rn); \
	} \
	if (info.operandFormat & ARM_OPERAND_REGISTER_3) { \
		loadReg(&ctx, info.op3.reg, rm); \
	} \
	switch (info.operandFormat & (ARM_OPERAND_2 | ARM_OPERAND_3)) { \
	case ARM_OPERAND_REGISTER_2 | ARM_OPERAND_REGISTER_3: \
		EMIT(&ctx, MN ## S, AL, rd, rn, rm); \
		break; \
	case ARM_OPERAND_REGISTER_2 | ARM_OPERAND_IMMEDIATE_3: \
		EMIT(&ctx, MN ## SI, AL, rd, rn, info.op3.immediate); \
		break; \
	case ARM_OPERAND_IMMEDIATE_2: \
		loadReg(&ctx, info.op1.reg, rd); \
		EMIT(&ctx, MN ## SI, AL, rd, rd, info.op2.immediate); \
		break; \
	case ARM_OPERAND_REGISTER_2: \
		loadReg(&ctx, info.op1.reg, rd); \
		EMIT(&ctx, MN ## S, AL, rd, rd, rn); \
		break; \
	default: \
		abort(); \
	} \
	flushReg(&ctx, info.op1.reg, rd); \
	ctx.cycles += 1 + info.iCycles; \
	ctx.cycles += info.sInstructionCycles * cpu->memory.activeSeqCycles16; \
	ctx.cycles += info.nInstructionCycles * cpu->memory.activeNonseqCycles16; \
	if (info.affectsCPSR) { \
		EMIT(&ctx, MRS, AL, 1); \
		EMIT(&ctx, MOV_LSRI, AL, 1, 1, 24); \
		EMIT(&ctx, STRBI, AL, 1, 4, 16 * sizeof(uint32_t) + 3); \
	}

void ARMDynarecRecompileTrace(struct ARMCore* cpu, struct ARMDynarecTrace* trace) {
#ifndef NDEBUG
	printf("%08X (%c)\n", trace->start, trace->mode == MODE_THUMB ? 'T' : 'A');
#endif
	struct ARMDynarecContext ctx = {
		.code = cpu->dynarec.buffer,
		.address = trace->start,
		.cycles = 0,
	};
	if (trace->mode == MODE_ARM) {
		return;
	} else {
		trace->entry = (void (*)(struct ARMCore*)) ctx.code;
		EMIT(&ctx, PUSH, AL, 0x4030);
		EMIT(&ctx, MOV, AL, 4, 0);
		EMIT(&ctx, LDRI, AL, 5, 0, ARM_PC * sizeof(uint32_t));
		__attribute__((aligned(64))) struct ARMInstructionInfo info;
		while (true) {
			uint16_t instruction = cpu->memory.load16(cpu, ctx.address, 0);
			ARMDecodeThumb(instruction, &info);
			ctx.address += WORD_SIZE_THUMB;
			if (needsUpdatePC(&info)) {
				updatePC(&ctx, ctx.address + WORD_SIZE_THUMB);
			}
			if (needsUpdatePrefetch(&info)) {
				flushPrefetch(&ctx, cpu->memory.load16(cpu, ctx.address, 0), cpu->memory.load16(cpu, ctx.address + WORD_SIZE_THUMB, 0));
				flushCycles(&ctx);
			}

			unsigned rd = 0;
			unsigned rn = 1;
			unsigned rm = 2;
			switch (info.mnemonic) {
			case ARM_MN_ADD:
				RECOMPILE_ALU(ADD);
				break;
			case ARM_MN_AND:
				RECOMPILE_ALU(AND);
				break;
			case ARM_MN_BIC:
				RECOMPILE_ALU(BIC);
				break;
			case ARM_MN_EOR:
				RECOMPILE_ALU(EOR);
				break;
			case ARM_MN_ORR:
				RECOMPILE_ALU(ORR);
				break;
			case ARM_MN_SUB:
				RECOMPILE_ALU(SUB);
				break;
			default:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
				EMIT_IMM(&ctx, AL, 1, instruction);
#pragma GCC diagnostic pop
				EMIT(&ctx, MOV, AL, 0, 4);
				EMIT(&ctx, BL, AL, ctx.code, _thumbTable[instruction >> 6]);
				break;
			}
			if (needsUpdateEvents(&info)) {
				updateEvents(&ctx, cpu);
			}
			if (info.branchType >= ARM_BRANCH || info.traps) {
				break;
			}
		}
		flushPrefetch(&ctx, cpu->memory.load16(cpu, ctx.address, 0), cpu->memory.load16(cpu, ctx.address + WORD_SIZE_THUMB, 0));
		flushCycles(&ctx);
		EMIT(&ctx, POP, AL, 0x8030);
	}
	__clear_cache(trace->entry, ctx.code);
	cpu->dynarec.buffer = ctx.code;
}
