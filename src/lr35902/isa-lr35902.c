/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "isa-lr35902.h"

#include "lr35902/emitter-lr35902.h"
#include "lr35902/lr35902.h"

#define DEFINE_INSTRUCTION_LR35902(NAME, BODY) \
	static void _LR35902Instruction ## NAME (struct LR35902Core* cpu) { \
		UNUSED(cpu); \
		BODY; \
	}

DEFINE_INSTRUCTION_LR35902(NOP,);

#define DEFINE_CONDITIONAL_INSTRUCTION_LR35902(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(, true) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(C, cpu->f.c) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(Z, cpu->f.z) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(NC, !cpu->f.c) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(NZ, !cpu->f.z)

DEFINE_INSTRUCTION_LR35902(JPFinish,
	if (cpu->condition) {
		cpu->pc = (cpu->bus << 8) | cpu->index;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		// TODO: Stall properly
		cpu->cycles += 4;
	})

DEFINE_INSTRUCTION_LR35902(JPDelay,
	cpu->executionState = LR35902_CORE_READ_PC;
	cpu->instruction = _LR35902InstructionJPFinish;
	cpu->index = cpu->bus;)

#define DEFINE_JP_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_LR35902(JP ## CONDITION_NAME, \
		cpu->executionState = LR35902_CORE_READ_PC; \
		cpu->instruction = _LR35902InstructionJPDelay; \
		cpu->condition = CONDITION;)

DEFINE_CONDITIONAL_INSTRUCTION_LR35902(JP);

DEFINE_INSTRUCTION_LR35902(JRFinish,
	if (cpu->condition) {
		cpu->pc += (int8_t) cpu->bus;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		// TODO: Stall properly
		cpu->cycles += 4;
	})

#define DEFINE_JR_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_LR35902(JR ## CONDITION_NAME, \
		cpu->executionState = LR35902_CORE_READ_PC; \
		cpu->instruction = _LR35902InstructionJRFinish; \
		cpu->condition = CONDITION;)

DEFINE_CONDITIONAL_INSTRUCTION_LR35902(JR);

DEFINE_INSTRUCTION_LR35902(CALLFinish,
	if (cpu->condition) {
		cpu->pc = (cpu->bus << 8) | cpu->index;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		// TODO: Stall properly
		cpu->cycles += 4;
	})

DEFINE_INSTRUCTION_LR35902(CALLUpdatePC,
	cpu->executionState = LR35902_CORE_READ_PC;
	cpu->index = cpu->bus;
	cpu->instruction = _LR35902InstructionCALLFinish;)

DEFINE_INSTRUCTION_LR35902(CALLUpdateSPL,
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionCALLUpdatePC;)

DEFINE_INSTRUCTION_LR35902(CALLUpdateSPH,
	cpu->index = cpu->sp + 1;
	cpu->bus = (cpu->pc + 2) >> 8;
	cpu->executionState = LR35902_CORE_MEMORY_STORE;
	cpu->instruction = _LR35902InstructionCALLUpdateSPL;)

#define DEFINE_CALL_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_LR35902(CALL ## CONDITION_NAME, \
		cpu->condition = CONDITION; \
		if (CONDITION) { \
			cpu->sp -= 2; \
			cpu->index = cpu->sp; \
			cpu->bus = cpu->pc + 2; \
			cpu->executionState = LR35902_CORE_MEMORY_STORE; \
			cpu->instruction = _LR35902InstructionCALLUpdateSPH; \
		} else { \
			cpu->executionState = LR35902_CORE_READ_PC; \
			cpu->instruction = _LR35902InstructionCALLUpdatePC; \
		})

DEFINE_CONDITIONAL_INSTRUCTION_LR35902(CALL)

DEFINE_INSTRUCTION_LR35902(RETUpdateSPL,
	cpu->pc |= cpu->bus << 8;
	cpu->sp += 2;
	cpu->memory.setActiveRegion(cpu, cpu->pc);
	// TODO: Stall properly
	cpu->cycles += 4;)

DEFINE_INSTRUCTION_LR35902(RETUpdateSPH,
	if (cpu->condition) {
		cpu->index = cpu->sp + 1;
		cpu->pc = cpu->bus;
		cpu->executionState = LR35902_CORE_MEMORY_LOAD;
		cpu->instruction = _LR35902InstructionRETUpdateSPL;
	})

#define DEFINE_RET_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_LR35902(RET ## CONDITION_NAME, \
		cpu->condition = CONDITION; \
		cpu->index = cpu->sp; \
		cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
		cpu->instruction = _LR35902InstructionRETUpdateSPH;)

DEFINE_CONDITIONAL_INSTRUCTION_LR35902(RET)

#define DEFINE_AND_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(AND ## NAME, \
		cpu->a &= OPERAND; \
		cpu->f.z = !cpu->a; \
		cpu->f.n = 0; \
		cpu->f.c = 0; \
		cpu->f.h = 1;)

#define DEFINE_XOR_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(XOR ## NAME, \
		cpu->a ^= OPERAND; \
		cpu->f.z = !cpu->a; \
		cpu->f.n = 0; \
		cpu->f.c = 0; \
		cpu->f.h = 0;)

#define DEFINE_OR_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(OR ## NAME, \
		cpu->a |= OPERAND; \
		cpu->f.z = !cpu->a; \
		cpu->f.n = 0; \
		cpu->f.c = 0; \
		cpu->f.h = 0;)

#define DEFINE_CP_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(CP ## NAME, \
		int diff = cpu->a - OPERAND; \
		cpu->f.n = 1; \
		cpu->f.z = !diff; \
		cpu->f.c = diff < 0; \
		/* TODO: Find explanation of H flag */)

#define DEFINE_LDB__INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(LDB_ ## NAME, \
		cpu->b = OPERAND;)

#define DEFINE_LDC__INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(LDC_ ## NAME, \
		cpu->c = OPERAND;)

#define DEFINE_LDD__INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(LDD_ ## NAME, \
		cpu->d = OPERAND;)

#define DEFINE_LDE__INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(LDE_ ## NAME, \
		cpu->e = OPERAND;)

#define DEFINE_LDH__INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(LDH_ ## NAME, \
		cpu->h = OPERAND;)

#define DEFINE_LDL__INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(LDL_ ## NAME, \
		cpu->l = OPERAND;)

#define DEFINE_LDHL__INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(LDHL_ ## NAME, \
		cpu->bus = OPERAND; \
		cpu->executionState = LR35902_CORE_MEMORY_STORE; \
		cpu->instruction = _LR35902InstructionLDHL_Bus;)

#define DEFINE_LDA__INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(LDA_ ## NAME, \
		cpu->a = OPERAND;)

#define DEFINE_ALU_INSTRUCTION_LR35902_NOHL(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(A, cpu->a); \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(B, cpu->b); \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(C, cpu->c); \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(D, cpu->d); \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(E, cpu->e); \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(H, cpu->h); \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(L, cpu->l);

DEFINE_INSTRUCTION_LR35902(LDHL_Bus, \
	cpu->index = LR35902ReadHL(cpu); \
	cpu->executionState = LR35902_CORE_MEMORY_STORE; \
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDHL_, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDHL_Bus;)

#define DEFINE_ALU_INSTRUCTION_LR35902(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(Bus, cpu->bus); \
	DEFINE_INSTRUCTION_LR35902(NAME ## HL, \
		cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
		cpu->index = LR35902ReadHL(cpu); \
		cpu->instruction = _LR35902Instruction ## NAME ## Bus;) \
	DEFINE_INSTRUCTION_LR35902(NAME, \
		cpu->executionState = LR35902_CORE_READ_PC; \
		cpu->instruction = _LR35902Instruction ## NAME ## Bus;) \
	DEFINE_ALU_INSTRUCTION_LR35902_NOHL(NAME)

DEFINE_ALU_INSTRUCTION_LR35902(AND);
DEFINE_ALU_INSTRUCTION_LR35902(XOR);
DEFINE_ALU_INSTRUCTION_LR35902(OR);
DEFINE_ALU_INSTRUCTION_LR35902(CP);

static void _LR35902InstructionLDB_Bus(struct LR35902Core*);
static void _LR35902InstructionLDC_Bus(struct LR35902Core*);
static void _LR35902InstructionLDD_Bus(struct LR35902Core*);
static void _LR35902InstructionLDE_Bus(struct LR35902Core*);
static void _LR35902InstructionLDH_Bus(struct LR35902Core*);
static void _LR35902InstructionLDL_Bus(struct LR35902Core*);
static void _LR35902InstructionLDHL_Bus(struct LR35902Core*);
static void _LR35902InstructionLDA_Bus(struct LR35902Core*);

DEFINE_ALU_INSTRUCTION_LR35902(LDB_);
DEFINE_ALU_INSTRUCTION_LR35902(LDC_);
DEFINE_ALU_INSTRUCTION_LR35902(LDD_);
DEFINE_ALU_INSTRUCTION_LR35902(LDE_);
DEFINE_ALU_INSTRUCTION_LR35902(LDH_);
DEFINE_ALU_INSTRUCTION_LR35902(LDL_);
DEFINE_ALU_INSTRUCTION_LR35902_NOHL(LDHL_);
DEFINE_ALU_INSTRUCTION_LR35902(LDA_);

DEFINE_INSTRUCTION_LR35902(LDBCDelay, \
	cpu->c = cpu->bus; \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDB_Bus;)

DEFINE_INSTRUCTION_LR35902(LDBC, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDBCDelay;)

DEFINE_INSTRUCTION_LR35902(LDBC_A, \
	cpu->index = LR35902ReadBC(cpu); \
	cpu->bus = cpu->a; \
	cpu->executionState = LR35902_CORE_MEMORY_STORE; \
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDDEDelay, \
	cpu->e = cpu->bus; \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDD_Bus;)

DEFINE_INSTRUCTION_LR35902(LDDE, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDDEDelay;)

DEFINE_INSTRUCTION_LR35902(LDDE_A, \
	cpu->index = LR35902ReadDE(cpu); \
	cpu->bus = cpu->a; \
	cpu->executionState = LR35902_CORE_MEMORY_STORE; \
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDHLDelay, \
	cpu->l = cpu->bus; \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDH_Bus;)

DEFINE_INSTRUCTION_LR35902(LDHL, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDHLDelay;)

DEFINE_INSTRUCTION_LR35902(LDSPFinish, cpu->sp |= cpu->bus << 8;)

DEFINE_INSTRUCTION_LR35902(LDSPDelay, \
	cpu->sp = cpu->bus; \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDSPFinish;)

DEFINE_INSTRUCTION_LR35902(LDSP, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDSPDelay;)

DEFINE_INSTRUCTION_LR35902(LDIHLA, \
	cpu->index = LR35902ReadHL(cpu); \
	LR35902WriteHL(cpu, cpu->index + 1); \
	cpu->bus = cpu->a; \
	cpu->executionState = LR35902_CORE_MEMORY_STORE; \
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDDHLA, \
	cpu->index = LR35902ReadHL(cpu); \
	LR35902WriteHL(cpu, cpu->index - 1); \
	cpu->bus = cpu->a; \
	cpu->executionState = LR35902_CORE_MEMORY_STORE; \
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDIAFinish, \
	cpu->index |= cpu->bus << 8;
	cpu->bus = cpu->a; \
	cpu->executionState = LR35902_CORE_MEMORY_STORE; \
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDIADelay, \
	cpu->index = cpu->bus;
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDIAFinish;)

DEFINE_INSTRUCTION_LR35902(LDIA, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDIADelay;)

DEFINE_INSTRUCTION_LR35902(LDAIFinish, \
	cpu->index |= cpu->bus << 8;
	cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
	cpu->instruction = _LR35902InstructionLDA_Bus;)

DEFINE_INSTRUCTION_LR35902(LDAIDelay, \
	cpu->index = cpu->bus;
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDAIFinish;)

DEFINE_INSTRUCTION_LR35902(LDAI, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDAIDelay;)

DEFINE_INSTRUCTION_LR35902(LDAIOC, \
	cpu->index = 0xFF00 | cpu->c; \
	cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
	cpu->instruction = _LR35902InstructionLDA_Bus;)

DEFINE_INSTRUCTION_LR35902(LDIOCA, \
	cpu->index = 0xFF00 | cpu->c; \
	cpu->bus = cpu->a; \
	cpu->executionState = LR35902_CORE_MEMORY_STORE; \
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDAIODelay, \
	cpu->index = 0xFF00 | cpu->bus; \
	cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
	cpu->instruction = _LR35902InstructionLDA_Bus;)

DEFINE_INSTRUCTION_LR35902(LDAIO, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDAIODelay;)

DEFINE_INSTRUCTION_LR35902(LDIOADelay, \
	cpu->index = 0xFF00 | cpu->bus; \
	cpu->bus = cpu->a; \
	cpu->executionState = LR35902_CORE_MEMORY_STORE; \
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDIOA, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionLDIOADelay;)

DEFINE_INSTRUCTION_LR35902(DI, cpu->irqh.setInterrupts(cpu, false));
DEFINE_INSTRUCTION_LR35902(EI, cpu->irqh.setInterrupts(cpu, true));

DEFINE_INSTRUCTION_LR35902(STUB, cpu->irqh.hitStub(cpu));

static const LR35902Instruction _lr35902CBInstructionTable[0x100] = {
	DECLARE_LR35902_CB_EMITTER_BLOCK(_LR35902Instruction)
};

DEFINE_INSTRUCTION_LR35902(CBDelegate, _lr35902CBInstructionTable[cpu->bus](cpu))

DEFINE_INSTRUCTION_LR35902(CB, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionCBDelegate;)

const LR35902Instruction _lr35902InstructionTable[0x100] = {
	DECLARE_LR35902_EMITTER_BLOCK(_LR35902Instruction)
};
