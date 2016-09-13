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
#include "arm/dynarec-arm/patch.h"
#include "arm/macros.h"
#include "arm/emitter-thumb.h"

typedef bool (*ThumbCompiler)(struct ARMCore*, struct ARMDynarecContext*, uint16_t opcode);
extern const ThumbCompiler _thumbCompilerTable[0x400];

void ARMDynarecExecuteTrace(struct ARMCore* cpu, struct ARMDynarecTrace* trace) {
	if (!trace->entry) return;
	assert(cpu->executionMode == MODE_THUMB);
	assert((uint32_t)cpu->gprs[15] == trace->start + WORD_SIZE_THUMB);

	cpu->dynarec.execute(cpu, trace->entry);

	if (cpu->cycles >= cpu->nextEvent) {
		cpu->irqh.processEvents(cpu);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Get Temporary

static unsigned allocTemp(struct ARMDynarecContext* ctx) {
	// For very short term use. Do not hold onto it.
	for (unsigned index = 0; index < 3; index++) {
		if (ctx->scratch[index].state == SCRATCH_STATE_EMPTY) {
			return REG_SCRATCH0 + index;
		}
	}
	assert(!"Ran out of scratch registers");
	abort();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NZCV management

static bool isNZCVInScratch(struct ARMDynarecContext* ctx) {
	return ctx->scratch[REG_NZCV_TMP - REG_SCRATCH0].state == SCRATCH_STATE_CONTAINS_NZCV;
}

static void loadNZCV(struct ARMDynarecContext* ctx) {
	if (!ctx->is_nzcv_in_host_nzcv) {
		if (isNZCVInScratch(ctx)) {
			EMIT(ctx, MSR, AL, true, false, REG_NZCV_TMP);
			ctx->scratch[REG_NZCV_TMP - REG_SCRATCH0].state = SCRATCH_STATE_EMPTY;
		} else {
			assert(!ctx->is_reglist_save_pushed);
			unsigned tmp = allocTemp(ctx);
			EMIT(ctx, LDRI, AL, tmp, REG_ARMCore, offsetof(struct ARMCore, cpsr));
			EMIT(ctx, MSR, AL, true, false, tmp);
		}
		ctx->is_nzcv_in_host_nzcv = true;
	}
}

static void saveNZCV(struct ARMDynarecContext* ctx) {
	ctx->is_nzcv_in_host_nzcv = true;
	if (isNZCVInScratch(ctx)) {
		ctx->scratch[REG_NZCV_TMP - REG_SCRATCH0].state = SCRATCH_STATE_EMPTY;
	}
}

static void flushNZCVToScratch(struct ARMDynarecContext* ctx) {
	if (ctx->is_nzcv_in_host_nzcv) {
		assert(ctx->scratch[REG_NZCV_TMP - REG_SCRATCH0].state == SCRATCH_STATE_EMPTY);
		EMIT(ctx, MRS, AL, REG_NZCV_TMP);
		ctx->scratch[REG_NZCV_TMP - REG_SCRATCH0].state = SCRATCH_STATE_CONTAINS_NZCV;
		ctx->is_nzcv_in_host_nzcv = false;
	}
}

static void flushNZCVFully(struct ARMDynarecContext* ctx) {
	if (ctx->is_nzcv_in_host_nzcv) {
		assert(!isNZCVInScratch(ctx) && !ctx->is_reglist_save_pushed);
		unsigned tmp = allocTemp(ctx);
		EMIT(ctx, MRS, AL, tmp);
		EMIT(ctx, MOV_LSRI, AL, tmp, tmp, 24);
		EMIT(ctx, STRBI, AL, tmp, REG_ARMCore, (int)offsetof(struct ARMCore, cpsr) + 3);
		ctx->is_nzcv_in_host_nzcv = false;
	} else if (ctx->scratch[REG_NZCV_TMP - REG_SCRATCH0].state == SCRATCH_STATE_CONTAINS_NZCV) {
		assert(!ctx->is_reglist_save_pushed);
		EMIT(ctx, MOV_LSRI, AL, REG_NZCV_TMP, REG_NZCV_TMP, 24);
		EMIT(ctx, STRBI, AL, REG_NZCV_TMP, REG_ARMCore, (int)offsetof(struct ARMCore, cpsr) + 3);
		ctx->scratch[REG_NZCV_TMP - REG_SCRATCH0].state = SCRATCH_STATE_EMPTY;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Register allocation

static void flushRegisterCache(struct ARMDynarecContext* ctx) {
	assert(!ctx->is_reglist_save_pushed && ctx->reg_cache_state != REG_CACHE_R7_ON_STACK);
	if (ctx->reg_cache_state == REG_CACHE_LOADED) {
		EMIT(ctx, STMIA, AL, REG_ARMCore, REGLIST_GUESTREGS);
		ctx->reg_cache_state = REG_CACHE_NOT_LOADED;
	}
}

static void loadRegValueNoVerify(struct ARMDynarecContext* ctx, unsigned guest_reg, unsigned host_reg) {
	assert(!ctx->is_reglist_save_pushed && ctx->reg_cache_state != REG_CACHE_R7_ON_STACK);
	if (ctx->reg_cache_state == REG_CACHE_LOADED) {
		switch (guest_reg) {
		case 0: EMIT(ctx, MOV, AL, host_reg, REG_GUEST_R0); return;
		case 1: EMIT(ctx, MOV, AL, host_reg, REG_GUEST_R1); return;
		case 2: EMIT(ctx, MOV, AL, host_reg, REG_GUEST_R2); return;
		case 3: EMIT(ctx, MOV, AL, host_reg, REG_GUEST_R3); return;
		case 4: EMIT(ctx, MOV, AL, host_reg, REG_GUEST_R4); return;
		case 5: EMIT(ctx, MOV, AL, host_reg, REG_GUEST_R5); return;
		case 6: EMIT(ctx, MOV, AL, host_reg, REG_GUEST_R6); return;
		case 7: EMIT(ctx, MOV, AL, host_reg, REG_GUEST_R7); return;
		}
	}
	if (guest_reg == 15) {
		EMIT_IMM(ctx, AL, host_reg, ctx->gpr_15);
	} else {
		EMIT(ctx, LDRI, AL, host_reg, REG_ARMCore, offsetof(struct ARMCore, gprs) + guest_reg * sizeof(uint32_t));
	}
}

static void loadRegValue(struct ARMDynarecContext* ctx, unsigned guest_reg, unsigned host_reg) {
	assert(host_reg >= REG_SCRATCH0 && host_reg <= REG_SCRATCH2);
	assert(ctx->scratch[host_reg - REG_SCRATCH0].state == SCRATCH_STATE_EMPTY);
	loadRegValueNoVerify(ctx, guest_reg, host_reg);
}

static unsigned findIndexOfGuestOrFreeReg(struct ARMDynarecContext* ctx, unsigned guest_reg) {
	for (unsigned index = 0; index < 3; index++) {
		if (ctx->scratch[index].guest_reg == guest_reg) {
			if (ctx->scratch[index].state & (SCRATCH_STATE_USE | SCRATCH_STATE_DEF)) {
				return index;
			}
		}
	}
	for (unsigned index = 0; index < 3; index++) {
		if (ctx->scratch[index].state == SCRATCH_STATE_EMPTY) {
			return index;
		}
	}
	if (isNZCVInScratch(ctx)) {
		flushNZCVFully(ctx);
		return REG_NZCV_TMP - REG_SCRATCH0;
	}
	assert(!"Ran out of scratch registers");
	abort();
}

static unsigned cachedRegHelper(struct ARMDynarecContext* ctx, bool is_use, unsigned guest_reg, unsigned* host_reg) {
	UNUSED(is_use);

	if (guest_reg > 7)
		return false;

	switch (guest_reg) {
	case 0: *host_reg = REG_GUEST_R0; break;
	case 1: *host_reg = REG_GUEST_R1; break;
	case 2: *host_reg = REG_GUEST_R2; break;
	case 3: *host_reg = REG_GUEST_R3; break;
	case 4: *host_reg = REG_GUEST_R4; break;
	case 5: *host_reg = REG_GUEST_R5; break;
	case 6: *host_reg = REG_GUEST_R6; break;
	case 7: *host_reg = REG_GUEST_R7; break;
	default: abort();
	}

	if (!ctx->is_reglist_save_pushed) {
		switch (ctx->reg_cache_state) {
		case REG_CACHE_NOT_LOADED:
			EMIT(ctx, LDMIA, AL, REG_ARMCore, REGLIST_GUESTREGS);
			ctx->reg_cache_state = REG_CACHE_LOADED;
		case REG_CACHE_LOADED:
			return true;
		case REG_CACHE_R7_ON_STACK:
		default:
			abort();
		}
	} else {
		switch (ctx->reg_cache_state) {
		case REG_CACHE_NOT_LOADED:
			return false;
		case REG_CACHE_R7_ON_STACK:
			assert(!is_use); // There's no reason to do a useReg in this state.
			return guest_reg != 7;
		case REG_CACHE_LOADED:
		default:
			abort();
		}
	}
}

static unsigned defReg(struct ARMDynarecContext* ctx, unsigned guest_reg) {
	unsigned host_reg;
	if (cachedRegHelper(ctx, false, guest_reg, &host_reg))
		return host_reg;
	unsigned index = findIndexOfGuestOrFreeReg(ctx, guest_reg);
	host_reg = REG_SCRATCH0 + index;
	ctx->scratch[index].state |= SCRATCH_STATE_DEF;
	ctx->scratch[index].guest_reg = guest_reg;
	return host_reg;
}

static unsigned useReg(struct ARMDynarecContext* ctx, unsigned guest_reg) {
	unsigned host_reg;
	if (cachedRegHelper(ctx, true, guest_reg, &host_reg))
		return host_reg;
	unsigned index = findIndexOfGuestOrFreeReg(ctx, guest_reg);
	host_reg = REG_SCRATCH0 + index;
	if (ctx->scratch[index].state & SCRATCH_STATE_USE) {
		// Value's already been loaded.
		assert(ctx->scratch[index].guest_reg == guest_reg);
		return host_reg;
	}
	loadRegValueNoVerify(ctx, guest_reg, host_reg);
	ctx->scratch[index].state |= SCRATCH_STATE_USE;
	ctx->scratch[index].guest_reg = guest_reg;
	return host_reg;
}

static unsigned usedefReg(struct ARMDynarecContext* ctx, unsigned guest_reg) {
	unsigned host_reg = useReg(ctx, guest_reg);
	unsigned host_reg_2 = defReg(ctx, guest_reg);
	UNUSED(host_reg_2);
	assert(host_reg == host_reg_2);
	return host_reg;
}

static void saveRegs(struct ARMDynarecContext* ctx) {
	assert(!ctx->is_reglist_save_pushed);
	assert(ctx->reg_cache_state != REG_CACHE_R7_ON_STACK);
	for (unsigned index = 0; index < 3; index++) {
		if (ctx->scratch[index].state & SCRATCH_STATE_DEF) {
			unsigned host_reg = REG_SCRATCH0 + index;
			unsigned guest_reg = ctx->scratch[index].guest_reg;
			if (ctx->reg_cache_state == REG_CACHE_LOADED && guest_reg <= 7) {
				switch (guest_reg) {
				case 7: EMIT(ctx, MOV, AL, REG_GUEST_R7, host_reg); break;
				default: abort();
				}
			} else {
				EMIT(ctx, STRI, AL, host_reg, REG_ARMCore, offsetof(struct ARMCore, gprs) + guest_reg * sizeof(uint32_t));
			}
			if (guest_reg == 15) {
				ctx->gpr_15_invalid = true;
			}
			ctx->scratch[index].state = SCRATCH_STATE_EMPTY;
		}
		if (ctx->scratch[index].state & SCRATCH_STATE_USE) {
			ctx->scratch[index].state = SCRATCH_STATE_EMPTY;
		}
	}
}

static void assertNoAssignedRegs(struct ARMDynarecContext* ctx) {
	UNUSED(ctx);
	for (unsigned index = 0; index < 3; index++) {
		assert((ctx->scratch[index].state & (SCRATCH_STATE_DEF | SCRATCH_STATE_USE)) == 0);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PC + Prefetch

static void flushPC(struct ARMDynarecContext* ctx) {
	assert(!ctx->gpr_15_invalid);
	if (!ctx->gpr_15_flushed) {
		assert(!ctx->is_reglist_save_pushed);
		unsigned tmp = allocTemp(ctx);
		EMIT_IMM(ctx, AL, tmp, ctx->gpr_15);
		EMIT(ctx, STRI, AL, tmp, REG_ARMCore, offsetof(struct ARMCore, gprs) + 15 * sizeof(uint32_t));
		ctx->gpr_15_flushed = true;
	}
}

static void flushPrefetch(struct ARMDynarecContext* ctx) {
	if (!ctx->prefetch_flushed) {
		assert(!ctx->is_reglist_save_pushed);
		unsigned tmp = allocTemp(ctx);
		EMIT_IMM(ctx, AL, tmp, ctx->prefetch[0]);
		EMIT(ctx, STRI, AL, tmp, REG_ARMCore, offsetof(struct ARMCore, prefetch) + 0 * sizeof(uint32_t));
		EMIT_IMM(ctx, AL, tmp, ctx->prefetch[1]);
		EMIT(ctx, STRI, AL, tmp, REG_ARMCore, offsetof(struct ARMCore, prefetch) + 1 * sizeof(uint32_t));
		ctx->prefetch_flushed = true;
	}
}

static void flushCycles(struct ARMDynarecContext* ctx) {
	assert(!ctx->is_reglist_save_pushed);
	if (ctx->cycles_register_valid) {
		unsigned tmp = allocTemp(ctx);
		EMIT(ctx, LDRI, AL, tmp, REG_ARMCore, offsetof(struct ARMCore, nextEvent));
		if (ctx->cycles == 0) {
			EMIT(ctx, SUBI, AL, REG_CYCLES, REG_CYCLES, 1);
		} else if (ctx->cycles != 1) {
			EMIT(ctx, ADDI, AL, REG_CYCLES, REG_CYCLES, ctx->cycles - 1);
		}
		ctx->cycles = 0;
		EMIT(ctx, ADD, AL, REG_CYCLES, REG_CYCLES, tmp);
		EMIT(ctx, STRI, AL, REG_CYCLES, REG_ARMCore, offsetof(struct ARMCore, cycles));
		ctx->cycles_register_valid = false;
	} else if (ctx->cycles) {
		unsigned tmp = allocTemp(ctx);
		EMIT(ctx, LDRI, AL, tmp, REG_ARMCore, offsetof(struct ARMCore, cycles));
		EMIT(ctx, ADDI, AL, tmp, tmp, ctx->cycles);
		EMIT(ctx, STRI, AL, tmp, REG_ARMCore, offsetof(struct ARMCore, cycles));
		ctx->cycles = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ARMDynarecEmitPrelude(struct ARMCore* cpu) {
	code_t* code = (code_t*) cpu->dynarec.buffer;

	cpu->dynarec.execute = (void (*)(struct ARMCore*, void*)) code;
	EMIT_L(code, PUSH, AL, 0x4DF0);
	EMIT_L(code, LDMIA, AL, REG_ARMCore, REGLIST_GUESTREGS);
	EMIT_L(code, MOV, AL, 15, 1);

	cpu->dynarec.flushNZCVAndRegsAndEpilogue = (void*) code;
	EMIT_L(code, STMIA, AL, REG_ARMCore, REGLIST_GUESTREGS);
	cpu->dynarec.flushNZCVAndEpilogue = (void*) code;
	EMIT_L(code, MOV_LSRI, AL, REG_NZCV_TMP, REG_NZCV_TMP, 24);
	EMIT_L(code, STRBI, AL, REG_NZCV_TMP, REG_ARMCore, (int)offsetof(struct ARMCore, cpsr) + 3);
	cpu->dynarec.epilogue = (void*) code;
	EMIT_L(code, POP, AL, 0x8DF0);

	cpu->dynarec.flushRegsAndEpilogue = (void*) code;
	EMIT_L(code, STMIA, AL, REG_ARMCore, REGLIST_GUESTREGS);
	EMIT_L(code, POP, AL, 0x8DF0);

	// CYCLE_EXIT needs to do four things:
	// 1. Store the correct PC back into cpu->gprs[15].
	// 1. Store the correct value of cpu->cycles.
	// 3. Update cpu->prefetch.
	// 4. Branch to the correct epilogue.
	#define DEFINE_CYCLE_EXIT(NAME, EPILOGUE) \
		cpu->dynarec.NAME = (void*) code;                                                                             \
		/* At this point REG_SCRATCH1 contains our new PC. */                                                         \
		EMIT_L(code, STRI, AL, REG_SCRATCH1, REG_ARMCore, offsetof(struct ARMCore, gprs) + 15 * sizeof(uint32_t));    \
		/* REG_CYCLES contains the expression cycles - nextEvent + 1 */                                               \
		EMIT_L(code, LDRI, AL, REG_SCRATCH0, REG_ARMCore, offsetof(struct ARMCore, nextEvent));                       \
		EMIT_L(code, SUBI, AL, REG_CYCLES, REG_CYCLES, 1);                                                            \
		EMIT_L(code, ADD, AL, REG_CYCLES, REG_CYCLES, REG_SCRATCH0);                                                  \
		EMIT_L(code, STRI, AL, REG_CYCLES, REG_ARMCore, offsetof(struct ARMCore, cycles));                            \
		/* REG_SCRATCH0, REG_SCRATCH1, and REG_CYCLES are free to use. */                                             \
		EMIT_L(code, LDRI, AL, REG_CYCLES, REG_ARMCore, offsetof(struct ARMCore, memory) + offsetof(struct ARMMemory, activeMask)); \
		EMIT_L(code, AND, AL, REG_SCRATCH0, REG_SCRATCH1, REG_CYCLES);                                                \
		EMIT_L(code, LDRI, AL, REG_CYCLES, REG_ARMCore, offsetof(struct ARMCore, memory) + offsetof(struct ARMMemory, activeRegion)); \
		EMIT_L(code, ADD, AL, REG_SCRATCH0, REG_SCRATCH0, REG_CYCLES);                                                \
		EMIT_L(code, LDRHI, AL, REG_SCRATCH1, REG_SCRATCH0, 0);                                                       \
		EMIT_L(code, STRI, AL, REG_SCRATCH1, REG_ARMCore, offsetof(struct ARMCore, prefetch) + 1 * sizeof(uint32_t)); \
		EMIT_L(code, SUBI, AL, REG_SCRATCH0, REG_SCRATCH0, WORD_SIZE_THUMB);                                          \
		EMIT_L(code, LDRHI, AL, REG_SCRATCH1, REG_SCRATCH0, 0);                                                       \
		EMIT_L(code, STRI, AL, REG_SCRATCH1, REG_ARMCore, offsetof(struct ARMCore, prefetch) + 0 * sizeof(uint32_t)); \
		EMIT_L(code, B, AL, code, cpu->dynarec.EPILOGUE);

	DEFINE_CYCLE_EXIT(cycleExitAndEpilogue, epilogue);
	DEFINE_CYCLE_EXIT(cycleExitAndFlushNZCVAndRegsAndEpilogue, flushNZCVAndRegsAndEpilogue);
	DEFINE_CYCLE_EXIT(cycleExitAndFlushNZCVAndEpilogue, flushNZCVAndEpilogue);
	DEFINE_CYCLE_EXIT(cycleExitAndFlushRegsAndEpilogue, flushRegsAndEpilogue);

	#undef DEFINE_CYCLE_EXIT

	cpu->dynarec.buffer = code;
	__clear_cache(cpu->dynarec.execute, code);
}

static void* selectEpilogue(struct ARMCore* cpu, struct ARMDynarecContext* ctx) {
	if (ctx->reg_cache_state == REG_CACHE_LOADED && isNZCVInScratch(ctx)) {
		return cpu->dynarec.flushNZCVAndRegsAndEpilogue;
	} else if (ctx->reg_cache_state == REG_CACHE_LOADED) {
		return cpu->dynarec.flushRegsAndEpilogue;
	} else if (isNZCVInScratch(ctx)) {
		return cpu->dynarec.flushNZCVAndEpilogue;
	} else {
		return cpu->dynarec.epilogue;
	}
}

static void* selectCycleExit(struct ARMCore* cpu, struct ARMDynarecContext* ctx) {
	if (ctx->prefetch_flushed || ctx->gpr_15_flushed) {
		// Correctness: A function that we call can modify the PC. We don't want to overwrite this!
		assert(ctx->prefetch_flushed && ctx->gpr_15_flushed);
		return selectEpilogue(cpu, ctx);
	}
	assert(!ctx->gpr_15_invalid);
	if (ctx->reg_cache_state == REG_CACHE_LOADED && isNZCVInScratch(ctx)) {
		return cpu->dynarec.cycleExitAndFlushNZCVAndRegsAndEpilogue;
	} else if (ctx->reg_cache_state == REG_CACHE_LOADED) {
		return cpu->dynarec.cycleExitAndFlushRegsAndEpilogue;
	} else if (isNZCVInScratch(ctx)) {
		return cpu->dynarec.cycleExitAndFlushNZCVAndEpilogue;
	} else {
		return cpu->dynarec.cycleExitAndEpilogue;
	}
}

void ARMDynarecRecompileTrace(struct ARMCore* cpu, struct ARMDynarecTrace* trace) {
#ifndef NDEBUG
//	printf("%08X (%c)\n", trace->start, trace->mode == MODE_THUMB ? 'T' : 'A');
//	printf("%u\n", cpu->nextEvent - cpu->cycles);
#endif

	if (trace->mode == MODE_ARM) {
		return;
	} else {
		struct ARMDynarecContext ctx = {
			.code = cpu->dynarec.buffer,
			.gpr_15 = trace->start + 1 * WORD_SIZE_THUMB,
			.gpr_15_flushed = false,
			.gpr_15_invalid = false,
			.cycles_register_valid = false,
			.cycles = 0,
			.prefetch_flushed = false,
			.is_nzcv_in_host_nzcv = false,
			.is_reglist_save_pushed = false,
			.reg_cache_state = REG_CACHE_LOADED,
		};
		LOAD_16(ctx.prefetch[0], (trace->start + 0 * WORD_SIZE_THUMB) & cpu->memory.activeMask, cpu->memory.activeRegion);
		LOAD_16(ctx.prefetch[1], (trace->start + 1 * WORD_SIZE_THUMB) & cpu->memory.activeMask, cpu->memory.activeRegion);

		trace->entry = (void*) ctx.code;

		bool continue_compilation = true;
		while (continue_compilation) {
			// emit: if (cpu->cycles >= cpu->nextEvent) return;
			assert(!ctx.is_reglist_save_pushed && !ctx.gpr_15_invalid);
			flushNZCVToScratch(&ctx);
			assertNoAssignedRegs(&ctx);
			if (!ctx.cycles_register_valid) {
				EMIT(&ctx, LDRI, AL, REG_SCRATCH0, REG_ARMCore, offsetof(struct ARMCore, cycles));
				EMIT(&ctx, LDRI, AL, REG_SCRATCH1, REG_ARMCore, offsetof(struct ARMCore, nextEvent));
				ctx.cycles++; //< there is the +1.
				// REG_CYCLES contains the expression cycles - nextEvent + 1
				// This allows us to avoid an unnecessary CMP instruction.
				EMIT(&ctx, SUB, AL, REG_CYCLES, REG_SCRATCH0, REG_SCRATCH1);
				ctx.cycles_register_valid = true;
			}
			EMIT(&ctx, ADDSI, AL, REG_CYCLES, REG_CYCLES, ctx.cycles);
			ctx.cycles = 0;
			EMIT_IMM(&ctx, PL, REG_SCRATCH1, ctx.gpr_15);
			EMIT(&ctx, B, PL, ctx.code, selectCycleExit(cpu, &ctx));

			// ThumbStep
			uint32_t opcode = ctx.prefetch[0];
			ctx.prefetch[0] = ctx.prefetch[1];
			ctx.gpr_15 += WORD_SIZE_THUMB;
			LOAD_16(ctx.prefetch[1], ctx.gpr_15 & cpu->memory.activeMask, cpu->memory.activeRegion);
			ctx.prefetch_flushed = false;
			ctx.gpr_15_flushed = false;
			ThumbCompiler instruction = _thumbCompilerTable[opcode >> 6];
			continue_compilation = instruction(cpu, &ctx, opcode);
		}
		flushCycles(&ctx);
		EMIT(&ctx, B, AL, ctx.code, selectEpilogue(cpu, &ctx));

		ARMDynarecPerformPatching(cpu, trace);

		__clear_cache(trace->entry, ctx.code);
		cpu->dynarec.buffer = ctx.code;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define THUMB_PREFETCH_CYCLES (1 + cpu->memory.activeSeqCycles16)

#define THUMB_LOAD_POST_BODY \
	currentCycles += cpu->memory.activeNonseqCycles16 - cpu->memory.activeSeqCycles16;

#define THUMB_STORE_POST_BODY \
	currentCycles += cpu->memory.activeNonseqCycles16 - cpu->memory.activeSeqCycles16;

#define FLUSH_HOST_NZCV \
	flushNZCVToScratch(ctx);

#define PREPARE_FOR_BL \
	flushCycles(ctx); \
	flushNZCVFully(ctx);

#define PUSH_REGLIST_SAVE \
	do { \
		assert(!ctx->is_reglist_save_pushed); \
		EMIT(ctx, PUSH, AL, REGLIST_SAVE); \
		ctx->is_reglist_save_pushed = true; \
		assert(ctx->reg_cache_state != REG_CACHE_R7_ON_STACK); \
		if (ctx->reg_cache_state == REG_CACHE_LOADED) \
			ctx->reg_cache_state = REG_CACHE_R7_ON_STACK; \
		assertNoAssignedRegs(ctx); \
	} while (0)

#define POP_REGLIST_SAVE \
	do { \
		assert(ctx->is_reglist_save_pushed); \
		EMIT(ctx, POP, AL, REGLIST_SAVE); \
		ctx->is_reglist_save_pushed = false; \
		assert(ctx->reg_cache_state != REG_CACHE_LOADED); \
		if (ctx->reg_cache_state == REG_CACHE_R7_ON_STACK) \
			ctx->reg_cache_state = REG_CACHE_LOADED; \
	} while (0)

static void thumbWritePcCallback(struct ARMCore* cpu) {
	cpu->gprs[ARM_PC] = (cpu->gprs[ARM_PC] & -WORD_SIZE_THUMB);
	cpu->memory.setActiveRegion(cpu, cpu->gprs[ARM_PC]);
	LOAD_16(cpu->prefetch[0], cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion);
	cpu->gprs[ARM_PC] += WORD_SIZE_THUMB;
	LOAD_16(cpu->prefetch[1], cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion);
	cpu->cycles += 2 + cpu->memory.activeNonseqCycles16 + cpu->memory.activeSeqCycles16;
}

#define DEFINE_INSTRUCTION_THUMB(NAME, BODY) \
	static bool _ThumbCompiler ## NAME (struct ARMCore* cpu, struct ARMDynarecContext* ctx, uint16_t opcode) {  \
		bool continue_compilation = true; \
		int currentCycles = THUMB_PREFETCH_CYCLES; \
		BODY; \
		assert(!ctx->is_reglist_save_pushed); \
		ctx->cycles += currentCycles; \
		return continue_compilation; \
	}

#define DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int immediate = (opcode >> 6) & 0x001F; \
		int rd = opcode & 0x0007; \
		int rm = (opcode >> 3) & 0x0007; \
		BODY;)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(LSL1,
	loadNZCV(ctx);
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rm = useReg(ctx, rm);
	EMIT(ctx, MOVS_LSLI, AL, reg_rd, reg_rm, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(LSR1,
	loadNZCV(ctx);
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rm = useReg(ctx, rm);
	EMIT(ctx, MOVS_LSRI, AL, reg_rd, reg_rm, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(ASR1,
	loadNZCV(ctx);
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rm = useReg(ctx, rm);
	EMIT(ctx, MOVS_ASRI, AL, reg_rd, reg_rm, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)

#define DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB_MEMORY_LOAD(MN, FUNC, SCALE) \
	DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(MN, \
		FLUSH_HOST_NZCV \
		PREPARE_FOR_BL \
		flushPC(ctx); \
		flushPrefetch(ctx); \
		loadRegValue(ctx, rm, 1); \
		EMIT(ctx, ADDI, AL, 1, 1, immediate * SCALE); \
		EMIT(ctx, ADDI, AL, 2, REG_ARMCore, offsetof(struct ARMCore, cycles)); \
		PUSH_REGLIST_SAVE; \
		EMIT(ctx, BL, AL, ctx->code, FUNC); \
		unsigned reg_rd = defReg(ctx, rd); \
		EMIT(ctx, MOV, AL, reg_rd, 0); \
		POP_REGLIST_SAVE; \
		saveRegs(ctx); \
		THUMB_LOAD_POST_BODY;)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB_MEMORY_LOAD(LDR1, cpu->memory.load32, 4)
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB_MEMORY_LOAD(LDRB1, cpu->memory.load8, 1)
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB_MEMORY_LOAD(LDRH1, cpu->memory.load16, 2)

#define DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB_MEMORY_STORE(MN, FUNC, SCALE) \
	DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB(MN, \
		FLUSH_HOST_NZCV \
		PREPARE_FOR_BL \
		flushPC(ctx); \
		flushPrefetch(ctx); \
		loadRegValue(ctx, rm, 1); \
		EMIT(ctx, ADDI, AL, 1, 1, immediate * SCALE); \
		loadRegValue(ctx, rd, 2); \
		EMIT(ctx, ADDI, AL, 3, REG_ARMCore, offsetof(struct ARMCore, cycles)); \
		PUSH_REGLIST_SAVE; \
		EMIT(ctx, BL, AL, ctx->code, FUNC); \
		POP_REGLIST_SAVE; \
		saveRegs(ctx); \
		THUMB_STORE_POST_BODY;)

DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB_MEMORY_STORE(STR1, cpu->memory.store32, 4)
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB_MEMORY_STORE(STRB1, cpu->memory.store8, 1)
DEFINE_IMMEDIATE_5_INSTRUCTION_THUMB_MEMORY_STORE(STRH1, cpu->memory.store16, 2)

#define DEFINE_DATA_FORM_1_INSTRUCTION_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rm = (opcode >> 6) & 0x0007; \
		int rd = opcode & 0x0007; \
		int rn = (opcode >> 3) & 0x0007; \
		BODY;)

DEFINE_DATA_FORM_1_INSTRUCTION_THUMB(ADD3,
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	unsigned reg_rm = useReg(ctx, rm);
	EMIT(ctx, ADDS, AL, reg_rd, reg_rn, reg_rm);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_1_INSTRUCTION_THUMB(SUB3,
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	unsigned reg_rm = useReg(ctx, rm);
	EMIT(ctx, SUBS, AL, reg_rd, reg_rn, reg_rm);
	saveRegs(ctx);
	saveNZCV(ctx);)

#define DEFINE_DATA_FORM_2_INSTRUCTION_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int immediate = (opcode >> 6) & 0x0007; \
		int rd = opcode & 0x0007; \
		int rn = (opcode >> 3) & 0x0007; \
		BODY;)

DEFINE_DATA_FORM_2_INSTRUCTION_THUMB(ADD1,
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, ADDSI, AL, reg_rd, reg_rn, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_2_INSTRUCTION_THUMB(SUB1,
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, SUBSI, AL, reg_rd, reg_rn, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)

#define DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rd = (opcode >> 8) & 0x0007; \
		int immediate = opcode & 0x00FF; \
		BODY;)

DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(MOV1,
	loadNZCV(ctx);
	unsigned reg_rd = defReg(ctx, rd);
	EMIT(ctx, MOVSI, AL, reg_rd, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(CMP1,
	unsigned reg_rd = useReg(ctx, rd);
	EMIT(ctx, CMPI, AL, reg_rd, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(ADD2,
	unsigned reg_rd = usedefReg(ctx, rd);
	EMIT(ctx, ADDSI, AL, reg_rd, reg_rd, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_3_INSTRUCTION_THUMB(SUB2,
	unsigned reg_rd = usedefReg(ctx, rd);
	EMIT(ctx, SUBSI, AL, reg_rd, reg_rd, immediate);
	saveRegs(ctx);
	saveNZCV(ctx);)

#define DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rd = opcode & 0x0007; \
		int rn = (opcode >> 3) & 0x0007; \
		BODY;)

DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(AND,
	loadNZCV(ctx);
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, ANDS, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(EOR,
	loadNZCV(ctx);
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, EORS, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(LSL2,
	loadNZCV(ctx);
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, MOVS_LSL, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(LSR2,
	loadNZCV(ctx);
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, MOVS_LSR, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(ASR2,
	loadNZCV(ctx);
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, MOVS_ASR, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(ADC,
	loadNZCV(ctx); // reads C flag
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, ADCS, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(SBC,
	loadNZCV(ctx); // reads C flag
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, SBCS, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(ROR,
	loadNZCV(ctx);
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, MOVS_ROR, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(TST,
	loadNZCV(ctx);
	unsigned reg_rd = useReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, TST, AL, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(NEG,
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, RSBSI, AL, reg_rd, reg_rn, 0);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(CMP2,
	unsigned reg_rd = useReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, CMP, AL, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(CMN,
	unsigned reg_rd = useReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, CMN, AL, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(ORR,
	loadNZCV(ctx);
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, ORRS, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_INSTRUCTION_THUMB(MUL,
	// Just interpret this.
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	flushRegisterCache(ctx);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
	EMIT_IMM(ctx, AL, 1, opcode);
#pragma GCC diagnostic pop
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, _thumbTable[opcode >> 6]);
	POP_REGLIST_SAVE;
	return true;)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(BIC,
	loadNZCV(ctx);
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, BICS, AL, reg_rd, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_DATA_FORM_5_INSTRUCTION_THUMB(MVN,
	loadNZCV(ctx);
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rn = useReg(ctx, rn);
	EMIT(ctx, MVNS, AL, reg_rd, reg_rn);
	saveRegs(ctx);
	saveNZCV(ctx);)

#define DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME, H1, H2, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rd = (opcode & 0x0007) | H1; \
		int rm = ((opcode >> 3) & 0x0007) | H2; \
		BODY;)

#define DEFINE_INSTRUCTION_WITH_HIGH_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME ## 00, 0, 0, BODY) \
	DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME ## 01, 0, 8, BODY) \
	DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME ## 10, 8, 0, BODY) \
	DEFINE_INSTRUCTION_WITH_HIGH_EX_THUMB(NAME ## 11, 8, 8, BODY)

DEFINE_INSTRUCTION_WITH_HIGH_THUMB(ADD4,
	unsigned reg_rd = usedefReg(ctx, rd);
	unsigned reg_rm = useReg(ctx, rm);
	EMIT(ctx, ADD, AL, reg_rd, reg_rd, reg_rm);
	saveRegs(ctx);
	if (rd == ARM_PC) {
		FLUSH_HOST_NZCV
		PREPARE_FOR_BL
		flushPrefetch(ctx);
		PUSH_REGLIST_SAVE;
		EMIT(ctx, BL, AL, ctx->code, &thumbWritePcCallback);
		POP_REGLIST_SAVE;
		continue_compilation = false;
	})
DEFINE_INSTRUCTION_WITH_HIGH_THUMB(CMP3,
	unsigned reg_rd = useReg(ctx, rd);
	unsigned reg_rm = useReg(ctx, rm);
	EMIT(ctx, CMP, AL, reg_rd, reg_rm);
	saveRegs(ctx);
	saveNZCV(ctx);)
DEFINE_INSTRUCTION_WITH_HIGH_THUMB(MOV3,
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_rm = useReg(ctx, rm);
	EMIT(ctx, MOV, AL, reg_rd, reg_rm);
	saveRegs(ctx);
	if (rd == ARM_PC) {
		FLUSH_HOST_NZCV
		PREPARE_FOR_BL
		flushPrefetch(ctx);
		PUSH_REGLIST_SAVE;
		EMIT(ctx, BL, AL, ctx->code, &thumbWritePcCallback);
		POP_REGLIST_SAVE;
		continue_compilation = false;
	})

#define DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rd = (opcode >> 8) & 0x0007; \
		int immediate = (opcode & 0x00FF) << 2; \
		BODY;)

DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(LDR3,
	// TODO(merry): Look into the possibility of inlining this without having to call load32.
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	EMIT_IMM(ctx, AL, 1, (ctx->gpr_15 & 0xFFFFFFFC) + immediate);
	EMIT(ctx, ADDI, AL, 2, REG_ARMCore, offsetof(struct ARMCore, cycles));
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, cpu->memory.load32);
	unsigned reg_rd = defReg(ctx, rd);
	EMIT(ctx, MOV, AL, reg_rd, 0);
	POP_REGLIST_SAVE;
	saveRegs(ctx);
	THUMB_LOAD_POST_BODY;)
DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(LDR4,
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	loadRegValue(ctx, ARM_SP, 1);
	EMIT(ctx, ADDI, AL, 1, 1, immediate);
	EMIT(ctx, ADDI, AL, 2, REG_ARMCore, offsetof(struct ARMCore, cycles));
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, cpu->memory.load32);
	unsigned reg_rd = defReg(ctx, rd);
	EMIT(ctx, MOV, AL, reg_rd, 0);
	POP_REGLIST_SAVE;
	saveRegs(ctx);
	THUMB_LOAD_POST_BODY;)
DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(STR3,
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	loadRegValue(ctx, ARM_SP, 1);
	EMIT(ctx, ADDI, AL, 1, 1, immediate);
	loadRegValue(ctx, rd, 2);
	EMIT(ctx, ADDI, AL, 3, REG_ARMCore, offsetof(struct ARMCore, cycles));
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, cpu->memory.store32);
	POP_REGLIST_SAVE;
	saveRegs(ctx);
	THUMB_STORE_POST_BODY;)

DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(ADD5,
	uint32_t to_write = (ctx->gpr_15 & 0xFFFFFFFC) + immediate;
	unsigned reg_rd = defReg(ctx, rd);
	EMIT_IMM(ctx, AL, reg_rd, to_write);
	saveRegs(ctx);)
DEFINE_IMMEDIATE_WITH_REGISTER_THUMB(ADD6,
	unsigned reg_rd = defReg(ctx, rd);
	unsigned reg_sp = useReg(ctx, ARM_SP);
	EMIT(ctx, ADDI, AL, reg_rd, reg_sp, immediate);
	saveRegs(ctx);)

#define DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(NAME, BODY) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		int rm = (opcode >> 6) & 0x0007; \
		int rd = opcode & 0x0007; \
		int rn = (opcode >> 3) & 0x0007; \
		BODY;)

#define DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_LOAD(NAME, FUNC, FINAL_INST) \
	DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(NAME, \
		FLUSH_HOST_NZCV \
		PREPARE_FOR_BL \
		flushPC(ctx); \
		flushPrefetch(ctx); \
		loadRegValue(ctx, rn, 1); \
		loadRegValue(ctx, rm, 2); \
		EMIT(ctx, ADD, AL, 1, 1, 2); \
		EMIT(ctx, ADDI, AL, 2, REG_ARMCore, offsetof(struct ARMCore, cycles)); \
		PUSH_REGLIST_SAVE; \
		EMIT(ctx, BL, AL, ctx->code, FUNC); \
		unsigned reg_rd = defReg(ctx, rd); \
		FINAL_INST; \
		POP_REGLIST_SAVE; \
		saveRegs(ctx); \
		THUMB_LOAD_POST_BODY;)

DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_LOAD(LDR2, cpu->memory.load32, EMIT(ctx, MOV, AL, reg_rd, 0))
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_LOAD(LDRB2, cpu->memory.load8, EMIT(ctx, MOV, AL, reg_rd, 0))
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_LOAD(LDRH2, cpu->memory.load16, EMIT(ctx, MOV, AL, reg_rd, 0))
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_LOAD(LDRSB, cpu->memory.load8, EMIT(ctx, SXTB, AL, reg_rd, 0, 0))

DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(LDRSH,
	// TODO(merry): Improve
	// Interpreter:
	//     rm = cpu->gprs[rn] + cpu->gprs[rm];
	//     cpu->gprs[rd] = rm & 1
	//                     ? ARM_SXT_8(cpu->memory.load16(cpu, rm, &currentCycles))
	//                     : ARM_SXT_16(cpu->memory.load16(cpu, rm, &currentCycles));
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	loadRegValue(ctx, rn, 1);
	loadRegValue(ctx, rm, 2);
	EMIT(ctx, ADD, AL, 1, 1, 2);
	EMIT(ctx, ADDI, AL, 2, REG_ARMCore, offsetof(struct ARMCore, cycles));
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, cpu->memory.load16);
	// TODO(merry): Is there an alternative to remateralizing here?
	EMIT(ctx, MOV, AL, 3, 0);
	POP_REGLIST_SAVE;
	loadRegValue(ctx, rn, 1);
	loadRegValue(ctx, rm, 2);
	EMIT(ctx, ADD, AL, 1, 1, 2);
	EMIT(ctx, TSTI, AL, 1, 1);
	unsigned reg_rd = defReg(ctx, rd);
	EMIT(ctx, SXTB, NE, reg_rd, 3, 0);
	EMIT(ctx, SXTH, EQ, reg_rd, 3, 0);
	saveRegs(ctx);
	THUMB_LOAD_POST_BODY;)

#define DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_STORE(NAME, FUNC) \
	DEFINE_LOAD_STORE_WITH_REGISTER_THUMB(NAME, \
		FLUSH_HOST_NZCV \
		PREPARE_FOR_BL \
		flushPC(ctx); \
		flushPrefetch(ctx); \
		loadRegValue(ctx, rn, 1); \
		loadRegValue(ctx, rm, 3); \
		EMIT(ctx, ADD, AL, 1, 1, 3); \
		loadRegValue(ctx, rd, 2); \
		EMIT(ctx, ADDI, AL, 3, REG_ARMCore, offsetof(struct ARMCore, cycles)); \
		PUSH_REGLIST_SAVE; \
		EMIT(ctx, BL, AL, ctx->code, FUNC); \
		POP_REGLIST_SAVE; \
		saveRegs(ctx); \
		THUMB_STORE_POST_BODY;)

DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_STORE(STR2, cpu->memory.store32)
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_STORE(STRB2, cpu->memory.store8)
DEFINE_LOAD_STORE_WITH_REGISTER_THUMB_STORE(STRH2, cpu->memory.store16)

#define DEFINE_LOAD_STORE_MULTIPLE_THUMB(NAME, RN, LS, DIRECTION, PRE_BODY, WRITEBACK) \
	DEFINE_INSTRUCTION_THUMB(NAME, \
		FLUSH_HOST_NZCV \
		PREPARE_FOR_BL \
		flushPC(ctx); \
		flushPrefetch(ctx); \
		flushRegisterCache(ctx); \
		int rn = RN; \
		int rs = opcode & 0xFF; \
		PRE_BODY; \
		loadRegValue(ctx, rn, 1); \
		EMIT_IMM(ctx, AL, 2, rs); \
		PUSH_REGLIST_SAVE; /* needs to be here due to stack manipulation */  \
		EMIT(ctx, SUBI, AL, ARM_SP, ARM_SP, 8); \
		/* here we load the &cycles argument: */ \
		EMIT(ctx, ADDI, AL, 3, REG_ARMCore, offsetof(struct ARMCore, cycles)); \
		EMIT(ctx, STRI, AL, 3, ARM_SP, 0); \
		/* arguments: (cpu, address, rs, LSM_ ## DIRECTION, &cycles) */ \
		EMIT_IMM(ctx, AL, 3, LSM_ ## DIRECTION); \
		EMIT(ctx, BL, AL, ctx->code, cpu->memory.CONCAT2(LS, Multiple)); \
		EMIT(ctx, ADDI, AL, ARM_SP, ARM_SP, 8); \
		WRITEBACK;)

DEFINE_LOAD_STORE_MULTIPLE_THUMB(LDMIA,
	(opcode >> 8) & 0x0007,
	load,
	IA,
	,
	THUMB_LOAD_POST_BODY;
	if (!((1 << rn) & rs)) {
		unsigned reg_rd = defReg(ctx, rn);
		EMIT(ctx, MOV, AL, reg_rd, 0);
	}
	POP_REGLIST_SAVE;
	saveRegs(ctx);)

DEFINE_LOAD_STORE_MULTIPLE_THUMB(STMIA,
	(opcode >> 8) & 0x0007,
	store,
	IA,
	,
	THUMB_STORE_POST_BODY;
	unsigned reg_rd = defReg(ctx, rn);
	EMIT(ctx, MOV, AL, reg_rd, 0);
	POP_REGLIST_SAVE;
	saveRegs(ctx);)

#define DEFINE_CONDITIONAL_BRANCH_THUMB(COND) \
	DEFINE_INSTRUCTION_THUMB(B ## COND, \
		int32_t offset = (int32_t)((int8_t)opcode) << 1; \
		uint32_t pass_pc = ctx->gpr_15 + offset; \
		uint32_t fail_pc = ctx->gpr_15; \
		struct ARMDynarecTrace* pass_trace = ARMDynarecFindTrace(cpu, pass_pc, MODE_THUMB); \
		struct ARMDynarecTrace* fail_trace = ARMDynarecFindTrace(cpu, fail_pc, MODE_THUMB); \
		FLUSH_HOST_NZCV \
		PREPARE_FOR_BL \
		flushPrefetch(ctx); \
		loadNZCV(ctx); \
		unsigned reg_pc = defReg(ctx, ARM_PC); \
		EMIT_IMM(ctx, AL, reg_pc, fail_pc); \
		EMIT_IMM(ctx, COND, reg_pc, pass_pc); \
		saveRegs(ctx); \
		EMIT_IMM(ctx, AL, REG_SCRATCH0, (uint32_t)fail_trace); \
		EMIT_IMM(ctx, COND, REG_SCRATCH0, (uint32_t)pass_trace); \
		EMIT(ctx, STRI, AL, REG_SCRATCH0, REG_ARMCore, offsetof(struct ARMCore, dynarec) + offsetof(struct ARMDynarec, tracePrediction)); \
		PUSH_REGLIST_SAVE; \
		EMIT(ctx, BL, COND, ctx->code, &thumbWritePcCallback); \
		POP_REGLIST_SAVE; \
		continue_compilation = false;)

DEFINE_CONDITIONAL_BRANCH_THUMB(EQ)
DEFINE_CONDITIONAL_BRANCH_THUMB(NE)
DEFINE_CONDITIONAL_BRANCH_THUMB(CS)
DEFINE_CONDITIONAL_BRANCH_THUMB(CC)
DEFINE_CONDITIONAL_BRANCH_THUMB(MI)
DEFINE_CONDITIONAL_BRANCH_THUMB(PL)
DEFINE_CONDITIONAL_BRANCH_THUMB(VS)
DEFINE_CONDITIONAL_BRANCH_THUMB(VC)
DEFINE_CONDITIONAL_BRANCH_THUMB(LS)
DEFINE_CONDITIONAL_BRANCH_THUMB(HI)
DEFINE_CONDITIONAL_BRANCH_THUMB(GE)
DEFINE_CONDITIONAL_BRANCH_THUMB(LT)
DEFINE_CONDITIONAL_BRANCH_THUMB(GT)
DEFINE_CONDITIONAL_BRANCH_THUMB(LE)

DEFINE_INSTRUCTION_THUMB(ADD7,
	unsigned reg_sp = usedefReg(ctx, ARM_SP);
	EMIT(ctx, ADDI, AL, reg_sp, reg_sp, (opcode & 0x7F) << 2);
	saveRegs(ctx);)
DEFINE_INSTRUCTION_THUMB(SUB4,
	unsigned reg_sp = usedefReg(ctx, ARM_SP);
	EMIT(ctx, SUBI, AL, reg_sp, reg_sp, (opcode & 0x7F) << 2);
	saveRegs(ctx);)

DEFINE_LOAD_STORE_MULTIPLE_THUMB(POP,
	ARM_SP,
	load,
	IA,
	,
	THUMB_LOAD_POST_BODY;
	unsigned reg_sp = defReg(ctx, ARM_SP);
	EMIT(ctx, MOV, AL, reg_sp, 0);
	POP_REGLIST_SAVE;
	saveRegs(ctx);)

DEFINE_LOAD_STORE_MULTIPLE_THUMB(POPR,
	ARM_SP,
	load,
	IA,
	rs |= 1 << ARM_PC,
	THUMB_LOAD_POST_BODY;
	unsigned reg_sp = defReg(ctx, ARM_SP);
	EMIT(ctx, MOV, AL, reg_sp, 0);
	POP_REGLIST_SAVE;
	saveRegs(ctx);
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, &thumbWritePcCallback);
	POP_REGLIST_SAVE;
	continue_compilation = false;)

DEFINE_LOAD_STORE_MULTIPLE_THUMB(PUSH,
	ARM_SP,
	store,
	DB,
	,
	THUMB_STORE_POST_BODY;
	unsigned reg_sp = defReg(ctx, ARM_SP);
	EMIT(ctx, MOV, AL, reg_sp, 0);
	POP_REGLIST_SAVE;
	saveRegs(ctx);)

DEFINE_LOAD_STORE_MULTIPLE_THUMB(PUSHR,
	ARM_SP,
	store,
	DB,
	rs |= 1 << ARM_LR,
	THUMB_STORE_POST_BODY;
	unsigned reg_sp = defReg(ctx, ARM_SP);
	EMIT(ctx, MOV, AL, reg_sp, 0);
	POP_REGLIST_SAVE;
	saveRegs(ctx);)

DEFINE_INSTRUCTION_THUMB(ILL,
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	flushRegisterCache(ctx);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
	EMIT_IMM(ctx, AL, 1, opcode);
#pragma GCC diagnostic pop
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, cpu->irqh.hitIllegal);
	POP_REGLIST_SAVE;
	continue_compilation = false;)
DEFINE_INSTRUCTION_THUMB(BKPT,
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	flushRegisterCache(ctx);
	EMIT_IMM(ctx, AL, 1, opcode & 0xFF);
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, cpu->irqh.bkpt16);
	POP_REGLIST_SAVE;
	continue_compilation = false;)
DEFINE_INSTRUCTION_THUMB(B,
	int16_t immediate = (opcode & 0x07FF) << 5;
	uint32_t new_pc = ctx->gpr_15 + (((int32_t) immediate) >> 4);
	struct ARMDynarecTrace* new_trace = ARMDynarecFindTrace(cpu, new_pc, MODE_THUMB);
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPrefetch(ctx);
	unsigned reg_pc = defReg(ctx, ARM_PC);
	EMIT_IMM(ctx, AL, reg_pc, new_pc);
	saveRegs(ctx);
	EMIT_IMM(ctx, AL, REG_SCRATCH0, (uint32_t)new_trace);
	EMIT(ctx, STRI, AL, REG_SCRATCH0, REG_ARMCore, offsetof(struct ARMCore, dynarec) + offsetof(struct ARMDynarec, tracePrediction));
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, &thumbWritePcCallback);
	POP_REGLIST_SAVE;
	continue_compilation = false;)

DEFINE_INSTRUCTION_THUMB(BL1,
	// TODO(merry): Block linking
	int16_t immediate = (opcode & 0x07FF) << 5;
	uint32_t new_lr = ctx->gpr_15 + (((int32_t) immediate) << 7);
	unsigned reg_lr = defReg(ctx, ARM_LR);
	EMIT_IMM(ctx, AL, reg_lr, new_lr);
	saveRegs(ctx);)
DEFINE_INSTRUCTION_THUMB(BL2,
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPrefetch(ctx);
	uint16_t immediate = (opcode & 0x07FF) << 1;
	unsigned reg_pc = defReg(ctx, ARM_PC);
	unsigned reg_lr = usedefReg(ctx, ARM_LR);
	EMIT_IMM(ctx, AL, reg_pc, immediate);
	EMIT(ctx, ADD, AL, reg_pc, reg_pc, reg_lr);
	EMIT_IMM(ctx, AL, reg_lr, ctx->gpr_15 - 1);
	saveRegs(ctx);
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, &thumbWritePcCallback);
	POP_REGLIST_SAVE;
	continue_compilation = false;)

DEFINE_INSTRUCTION_THUMB(BX,
	// Just interpret this.
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	flushRegisterCache(ctx);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
	EMIT_IMM(ctx, AL, 1, opcode);
#pragma GCC diagnostic pop
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, _thumbTable[opcode >> 6]);
	POP_REGLIST_SAVE;
	EMIT(ctx, B, AL, ctx->code, selectEpilogue(cpu, ctx));
	return false;)

DEFINE_INSTRUCTION_THUMB(SWI,
	FLUSH_HOST_NZCV
	PREPARE_FOR_BL
	flushPC(ctx);
	flushPrefetch(ctx);
	flushRegisterCache(ctx);
	EMIT_IMM(ctx, AL, 1, opcode & 0xFF);
	PUSH_REGLIST_SAVE;
	EMIT(ctx, BL, AL, ctx->code, cpu->irqh.swi16);
	POP_REGLIST_SAVE;
	continue_compilation = false;)

const ThumbCompiler _thumbCompilerTable[0x400] = {
		DECLARE_THUMB_EMITTER_BLOCK(_ThumbCompiler)
};
