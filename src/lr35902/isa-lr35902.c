/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/lr35902/isa-lr35902.h>

#include <mgba/internal/lr35902/emitter-lr35902.h>
#include <mgba/internal/lr35902/lr35902.h>

static inline uint16_t LR35902ReadHL(struct LR35902Core* cpu) {
	uint16_t hl;
	LOAD_16LE(hl, 0, &cpu->hl);
	return hl;
}

static inline void LR35902WriteHL(struct LR35902Core* cpu, uint16_t hl) {
	STORE_16LE(hl, 0, &cpu->hl);
}

static inline uint16_t LR35902ReadBC(struct LR35902Core* cpu) {
	uint16_t bc;
	LOAD_16LE(bc, 0, &cpu->bc);
	return bc;
}

static inline void LR35902WriteBC(struct LR35902Core* cpu, uint16_t bc) {
	STORE_16LE(bc, 0, &cpu->bc);
}

static inline uint16_t LR35902ReadDE(struct LR35902Core* cpu) {
	uint16_t de;
	LOAD_16LE(de, 0, &cpu->de);
	return de;
}

static inline void LR35902WriteDE(struct LR35902Core* cpu, uint16_t de) {
	STORE_16LE(de, 0, &cpu->de);
}

#define DEFINE_INSTRUCTION_LR35902(NAME, BODY) \
	static void _LR35902Instruction ## NAME (struct LR35902Core* cpu) { \
		UNUSED(cpu); \
		BODY; \
	}

DEFINE_INSTRUCTION_LR35902(NOP,);

#define DEFINE_CONDITIONAL_ONLY_INSTRUCTION_LR35902(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(C, cpu->f.c) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(Z, cpu->f.z) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(NC, !cpu->f.c) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(NZ, !cpu->f.z)

#define DEFINE_CONDITIONAL_INSTRUCTION_LR35902(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(, true) \
	DEFINE_CONDITIONAL_ONLY_INSTRUCTION_LR35902(NAME)

DEFINE_INSTRUCTION_LR35902(JPFinish,
	if (cpu->condition) {
		cpu->pc = (cpu->bus << 8) | cpu->index;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		cpu->executionState = LR35902_CORE_STALL;
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

DEFINE_INSTRUCTION_LR35902(JPHL,
	cpu->pc = LR35902ReadHL(cpu);
	cpu->memory.setActiveRegion(cpu, cpu->pc);)

DEFINE_INSTRUCTION_LR35902(JRFinish,
	if (cpu->condition) {
		cpu->pc += (int8_t) cpu->bus;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		cpu->executionState = LR35902_CORE_STALL;
	})

#define DEFINE_JR_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_LR35902(JR ## CONDITION_NAME, \
		cpu->executionState = LR35902_CORE_READ_PC; \
		cpu->instruction = _LR35902InstructionJRFinish; \
		cpu->condition = CONDITION;)

DEFINE_CONDITIONAL_INSTRUCTION_LR35902(JR);

DEFINE_INSTRUCTION_LR35902(CALLUpdateSPL,
	--cpu->index;
	cpu->bus = cpu->sp;
	cpu->sp = cpu->index;
	cpu->executionState = LR35902_CORE_MEMORY_STORE;
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(CALLUpdateSPH,
	cpu->executionState = LR35902_CORE_MEMORY_STORE;
	cpu->instruction = _LR35902InstructionCALLUpdateSPL;)

DEFINE_INSTRUCTION_LR35902(CALLUpdatePCH,
	if (cpu->condition) {
		int newPc = (cpu->bus << 8) | cpu->index;
		cpu->bus = cpu->pc >> 8;
		cpu->index = cpu->sp - 1;
		cpu->sp = cpu->pc; // GROSS
		cpu->pc = newPc;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		cpu->executionState = LR35902_CORE_OP2;
		cpu->instruction = _LR35902InstructionCALLUpdateSPH;
	})

DEFINE_INSTRUCTION_LR35902(CALLUpdatePCL,
	cpu->executionState = LR35902_CORE_READ_PC;
	cpu->index = cpu->bus;
	cpu->instruction = _LR35902InstructionCALLUpdatePCH)

#define DEFINE_CALL_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_LR35902(CALL ## CONDITION_NAME, \
		cpu->condition = CONDITION; \
		cpu->executionState = LR35902_CORE_READ_PC; \
		cpu->instruction = _LR35902InstructionCALLUpdatePCL;)

DEFINE_CONDITIONAL_INSTRUCTION_LR35902(CALL)

DEFINE_INSTRUCTION_LR35902(RETFinish,
	cpu->sp += 2;  /* TODO: Atomic incrementing? */
	cpu->pc |= cpu->bus << 8;
	cpu->memory.setActiveRegion(cpu, cpu->pc);
	cpu->executionState = LR35902_CORE_STALL;)

DEFINE_INSTRUCTION_LR35902(RETUpdateSPL,
	cpu->index = cpu->sp + 1;
	cpu->pc = cpu->bus;
	cpu->executionState = LR35902_CORE_MEMORY_LOAD;
	cpu->instruction = _LR35902InstructionRETFinish;)

DEFINE_INSTRUCTION_LR35902(RETUpdateSPH,
	if (cpu->condition) {
		cpu->index = cpu->sp;
		cpu->executionState = LR35902_CORE_MEMORY_LOAD;
		cpu->instruction = _LR35902InstructionRETUpdateSPL;
	})

#define DEFINE_RET_INSTRUCTION_LR35902(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_LR35902(RET ## CONDITION_NAME, \
		cpu->condition = CONDITION; \
		cpu->executionState = LR35902_CORE_OP2; \
		cpu->instruction = _LR35902InstructionRETUpdateSPH;)

DEFINE_INSTRUCTION_LR35902(RET,
	cpu->condition = true;
	_LR35902InstructionRETUpdateSPH(cpu);)

DEFINE_INSTRUCTION_LR35902(RETI,
	cpu->condition = true;
	cpu->irqh.setInterrupts(cpu, true);
	_LR35902InstructionRETUpdateSPH(cpu);)

DEFINE_CONDITIONAL_ONLY_INSTRUCTION_LR35902(RET)

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
		cpu->f.z = !(diff & 0xFF); \
		cpu->f.h = (cpu->a & 0xF) - (OPERAND & 0xF) < 0; \
		cpu->f.c = diff < 0;)

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
		cpu->index = LR35902ReadHL(cpu); \
		cpu->executionState = LR35902_CORE_MEMORY_STORE; \
		cpu->instruction = _LR35902InstructionNOP;)

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

DEFINE_INSTRUCTION_LR35902(LDHL_SPDelay,
	int diff = (int8_t) cpu->bus;
	int sum = cpu->sp + diff;
	LR35902WriteHL(cpu, sum);
	cpu->executionState = LR35902_CORE_STALL;
	cpu->f.z = 0;
	cpu->f.n = 0;
	cpu->f.c = (diff & 0xFF) + (cpu->sp & 0xFF) >= 0x100;
	cpu->f.h = (diff & 0xF) + (cpu->sp & 0xF) >= 0x10;)

DEFINE_INSTRUCTION_LR35902(LDHL_SP,
	cpu->executionState = LR35902_CORE_READ_PC;
	cpu->instruction = _LR35902InstructionLDHL_SPDelay;)

DEFINE_INSTRUCTION_LR35902(LDSP_HL,
	cpu->sp = LR35902ReadHL(cpu);
	cpu->executionState = LR35902_CORE_STALL;)

#define DEFINE_ALU_INSTRUCTION_LR35902_MEM(NAME, REG) \
	DEFINE_INSTRUCTION_LR35902(NAME ## REG, \
		cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
		cpu->index = LR35902Read ## REG (cpu); \
		cpu->instruction = _LR35902Instruction ## NAME ## Bus;)

#define DEFINE_ALU_INSTRUCTION_LR35902(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_LR35902(Bus, cpu->bus); \
	DEFINE_ALU_INSTRUCTION_LR35902_MEM(NAME, HL) \
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

#define DEFINE_ADD_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(ADD ## NAME, \
		int diff = cpu->a + OPERAND; \
		cpu->f.n = 0; \
		cpu->f.h = (cpu->a & 0xF) + (OPERAND & 0xF) >= 0x10; \
		cpu->f.c = diff >= 0x100; \
		cpu->a = diff; \
		cpu->f.z = !cpu->a;)

#define DEFINE_ADC_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(ADC ## NAME, \
		int diff = cpu->a + OPERAND + cpu->f.c; \
		cpu->f.n = 0; \
		cpu->f.h = (cpu->a & 0xF) + (OPERAND & 0xF) + cpu->f.c >= 0x10; \
		cpu->f.c = diff >= 0x100; \
		cpu->a = diff; \
		cpu->f.z = !cpu->a;)

#define DEFINE_SUB_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(SUB ## NAME, \
		int diff = cpu->a - OPERAND; \
		cpu->f.n = 1; \
		cpu->f.h = (cpu->a & 0xF) - (OPERAND & 0xF) < 0; \
		cpu->f.c = diff < 0; \
		cpu->a = diff; \
		cpu->f.z = !cpu->a;)

#define DEFINE_SBC_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(SBC ## NAME, \
		int diff = cpu->a - OPERAND - cpu->f.c; \
		cpu->f.n = 1; \
		cpu->f.h = (cpu->a & 0xF) - (OPERAND & 0xF) - cpu->f.c < 0; \
		cpu->f.c = diff < 0; \
		cpu->a = diff; \
		cpu->f.z = !cpu->a;)

DEFINE_ALU_INSTRUCTION_LR35902(LDB_);
DEFINE_ALU_INSTRUCTION_LR35902(LDC_);
DEFINE_ALU_INSTRUCTION_LR35902(LDD_);
DEFINE_ALU_INSTRUCTION_LR35902(LDE_);
DEFINE_ALU_INSTRUCTION_LR35902(LDH_);
DEFINE_ALU_INSTRUCTION_LR35902(LDL_);
DEFINE_ALU_INSTRUCTION_LR35902_NOHL(LDHL_);
DEFINE_ALU_INSTRUCTION_LR35902(LDA_);
DEFINE_ALU_INSTRUCTION_LR35902_MEM(LDA_, BC);
DEFINE_ALU_INSTRUCTION_LR35902_MEM(LDA_, DE);
DEFINE_ALU_INSTRUCTION_LR35902(ADD);
DEFINE_ALU_INSTRUCTION_LR35902(ADC);
DEFINE_ALU_INSTRUCTION_LR35902(SUB);
DEFINE_ALU_INSTRUCTION_LR35902(SBC);

DEFINE_INSTRUCTION_LR35902(ADDSPFinish,
	cpu->sp = cpu->index;
	cpu->executionState = LR35902_CORE_STALL;)

DEFINE_INSTRUCTION_LR35902(ADDSPDelay,
	int diff = (int8_t) cpu->bus;
	int sum = cpu->sp + diff;
	cpu->index = sum;
	cpu->executionState = LR35902_CORE_OP2;
	cpu->instruction = _LR35902InstructionADDSPFinish;
	cpu->f.z = 0;
	cpu->f.n = 0;
	cpu->f.c = (diff & 0xFF) + (cpu->sp & 0xFF) >= 0x100;
	cpu->f.h = (diff & 0xF) + (cpu->sp & 0xF) >= 0x10;)

DEFINE_INSTRUCTION_LR35902(ADDSP,
	cpu->executionState = LR35902_CORE_READ_PC;
	cpu->instruction = _LR35902InstructionADDSPDelay;)

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

DEFINE_INSTRUCTION_LR35902(LDA_IHL, \
	cpu->index = LR35902ReadHL(cpu); \
	LR35902WriteHL(cpu, cpu->index + 1); \
	cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
	cpu->instruction = _LR35902InstructionLDA_Bus;)

DEFINE_INSTRUCTION_LR35902(LDA_DHL, \
	cpu->index = LR35902ReadHL(cpu); \
	LR35902WriteHL(cpu, cpu->index - 1); \
	cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
	cpu->instruction = _LR35902InstructionLDA_Bus;)

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

DEFINE_INSTRUCTION_LR35902(LDISPStoreH,
	++cpu->index;
	cpu->bus = cpu->sp >> 8;
	cpu->executionState = LR35902_CORE_MEMORY_STORE;
	cpu->instruction = _LR35902InstructionNOP;)

DEFINE_INSTRUCTION_LR35902(LDISPStoreL,
	cpu->index |= cpu->bus << 8;
	cpu->bus = cpu->sp;
	cpu->executionState = LR35902_CORE_MEMORY_STORE;
	cpu->instruction = _LR35902InstructionLDISPStoreH;)

DEFINE_INSTRUCTION_LR35902(LDISPReadAddr,
	cpu->index = cpu->bus;
	cpu->executionState = LR35902_CORE_READ_PC;
	cpu->instruction = _LR35902InstructionLDISPStoreL;)

DEFINE_INSTRUCTION_LR35902(LDISP,
	cpu->executionState = LR35902_CORE_READ_PC;
	cpu->instruction = _LR35902InstructionLDISPReadAddr;)

#define DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(REG) \
	DEFINE_INSTRUCTION_LR35902(INC ## REG, \
		uint16_t reg = LR35902Read ## REG (cpu); \
		LR35902Write ## REG (cpu, reg + 1); \
		cpu->executionState = LR35902_CORE_STALL;) \
	DEFINE_INSTRUCTION_LR35902(DEC ## REG, \
		uint16_t reg = LR35902Read ## REG (cpu); \
		LR35902Write ## REG (cpu, reg - 1); \
		cpu->executionState = LR35902_CORE_STALL;)

DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(BC);
DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(DE);
DEFINE_INCDEC_WIDE_INSTRUCTION_LR35902(HL);

#define DEFINE_ADD_HL_INSTRUCTION_LR35902(REG, L, H) \
	DEFINE_INSTRUCTION_LR35902(ADDHL_ ## REG ## Finish, \
		int diff = H + cpu->h + cpu->f.c; \
		cpu->f.n = 0; \
		cpu->f.h = (H & 0xF) + (cpu->h & 0xF) + cpu->f.c >= 0x10; \
		cpu->f.c = diff >= 0x100; \
		cpu->h = diff;) \
	DEFINE_INSTRUCTION_LR35902(ADDHL_ ## REG, \
		int diff = L + cpu->l; \
		cpu->l = diff; \
		cpu->f.c = diff >= 0x100; \
		cpu->executionState = LR35902_CORE_OP2; \
		cpu->instruction = _LR35902InstructionADDHL_ ## REG ## Finish;)

DEFINE_ADD_HL_INSTRUCTION_LR35902(BC, cpu->c, cpu->b);
DEFINE_ADD_HL_INSTRUCTION_LR35902(DE, cpu->e, cpu->d);
DEFINE_ADD_HL_INSTRUCTION_LR35902(HL, cpu->l, cpu->h);
DEFINE_ADD_HL_INSTRUCTION_LR35902(SP, (cpu->sp & 0xFF), (cpu->sp >> 8));


#define DEFINE_INC_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(INC ## NAME, \
		int diff = OPERAND + 1; \
		cpu->f.h = (OPERAND & 0xF) == 0xF; \
		OPERAND = diff; \
		cpu->f.n = 0; \
		cpu->f.z = !OPERAND;)

#define DEFINE_DEC_INSTRUCTION_LR35902(NAME, OPERAND) \
	DEFINE_INSTRUCTION_LR35902(DEC ## NAME, \
		int diff = OPERAND - 1; \
		cpu->f.h = (OPERAND & 0xF) == 0x0; \
		OPERAND = diff; \
		cpu->f.n = 1; \
		cpu->f.z = !OPERAND;)

DEFINE_ALU_INSTRUCTION_LR35902_NOHL(INC);
DEFINE_ALU_INSTRUCTION_LR35902_NOHL(DEC);

DEFINE_INSTRUCTION_LR35902(INC_HLDelay,
	int diff = cpu->bus + 1;
	cpu->f.n = 0;
	cpu->f.h = (cpu->bus & 0xF) == 0xF;
	cpu->bus = diff;
	cpu->f.z = !cpu->bus;
	cpu->instruction = _LR35902InstructionNOP;
	cpu->executionState = LR35902_CORE_MEMORY_STORE;)

DEFINE_INSTRUCTION_LR35902(INC_HL,
	cpu->index = LR35902ReadHL(cpu);
	cpu->instruction = _LR35902InstructionINC_HLDelay;
	cpu->executionState = LR35902_CORE_MEMORY_LOAD;)

DEFINE_INSTRUCTION_LR35902(DEC_HLDelay,
	int diff = cpu->bus - 1;
	cpu->f.n = 1;
	cpu->f.h = (cpu->bus & 0xF) == 0;
	cpu->bus = diff;
	cpu->f.z = !cpu->bus;
	cpu->instruction = _LR35902InstructionNOP;
	cpu->executionState = LR35902_CORE_MEMORY_STORE;)

DEFINE_INSTRUCTION_LR35902(DEC_HL,
	cpu->index = LR35902ReadHL(cpu);
	cpu->instruction = _LR35902InstructionDEC_HLDelay;
	cpu->executionState = LR35902_CORE_MEMORY_LOAD;)

DEFINE_INSTRUCTION_LR35902(INCSP,
	++cpu->sp;
	cpu->executionState = LR35902_CORE_STALL;)

DEFINE_INSTRUCTION_LR35902(DECSP,
	--cpu->sp;
	cpu->executionState = LR35902_CORE_STALL;)

DEFINE_INSTRUCTION_LR35902(SCF,
	cpu->f.c = 1;
	cpu->f.h = 0;
	cpu->f.n = 0;)

DEFINE_INSTRUCTION_LR35902(CCF,
	cpu->f.c ^= 1;
	cpu->f.h = 0;
	cpu->f.n = 0;)

DEFINE_INSTRUCTION_LR35902(CPL_,
	cpu->a ^= 0xFF;
	cpu->f.h = 1;
	cpu->f.n = 1;)

DEFINE_INSTRUCTION_LR35902(DAA,
	if (cpu->f.n) {
		if (cpu->f.h) {
			cpu->a += 0xFA;
		}
		if (cpu->f.c) {
			cpu->a += 0xA0;
		}
	} else {
		int a = cpu->a;
		if ((cpu->a & 0xF) > 0x9 || cpu->f.h) {
			a += 0x6;
		}
		if ((a & 0x1F0) > 0x90 || cpu->f.c) {
			a += 0x60;
			cpu->f.c = 1;
		} else {
			cpu->f.c = 0;
		}
		cpu->a = a;
	}
	cpu->f.h = 0;
	cpu->f.z = !cpu->a;)

#define DEFINE_POPPUSH_INSTRUCTION_LR35902(REG, HH, H, L) \
	DEFINE_INSTRUCTION_LR35902(POP ## REG ## Delay, \
		cpu-> L = cpu->bus; \
		cpu->f.packed &= 0xF0; \
		cpu->index = cpu->sp; \
		++cpu->sp; \
		cpu->instruction = _LR35902InstructionLD ## HH ## _Bus; \
		cpu->executionState = LR35902_CORE_MEMORY_LOAD;) \
	DEFINE_INSTRUCTION_LR35902(POP ## REG, \
		cpu->index = cpu->sp; \
		++cpu->sp; \
		cpu->instruction = _LR35902InstructionPOP ## REG ## Delay; \
		cpu->executionState = LR35902_CORE_MEMORY_LOAD;) \
	DEFINE_INSTRUCTION_LR35902(PUSH ## REG ## Finish, \
		cpu->executionState = LR35902_CORE_STALL;) \
	DEFINE_INSTRUCTION_LR35902(PUSH ## REG ## Delay, \
		--cpu->sp; \
		cpu->index = cpu->sp; \
		cpu->bus = cpu-> L; \
		cpu->instruction = _LR35902InstructionPUSH ## REG ## Finish; \
		cpu->executionState = LR35902_CORE_MEMORY_STORE;) \
	DEFINE_INSTRUCTION_LR35902(PUSH ## REG, \
		--cpu->sp; \
		cpu->index = cpu->sp; \
		cpu->bus = cpu-> H; \
		cpu->instruction = _LR35902InstructionPUSH ## REG ## Delay; \
		cpu->executionState = LR35902_CORE_MEMORY_STORE;)

DEFINE_POPPUSH_INSTRUCTION_LR35902(BC, B, b, c);
DEFINE_POPPUSH_INSTRUCTION_LR35902(DE, D, d, e);
DEFINE_POPPUSH_INSTRUCTION_LR35902(HL, H, h, l);
DEFINE_POPPUSH_INSTRUCTION_LR35902(AF, A, a, f.packed);

#define DEFINE_CB_2_INSTRUCTION_LR35902(NAME, WB, BODY) \
	DEFINE_INSTRUCTION_LR35902(NAME ## B, uint8_t reg = cpu->b; BODY; cpu->b = reg) \
	DEFINE_INSTRUCTION_LR35902(NAME ## C, uint8_t reg = cpu->c; BODY; cpu->c = reg) \
	DEFINE_INSTRUCTION_LR35902(NAME ## D, uint8_t reg = cpu->d; BODY; cpu->d = reg) \
	DEFINE_INSTRUCTION_LR35902(NAME ## E, uint8_t reg = cpu->e; BODY; cpu->e = reg) \
	DEFINE_INSTRUCTION_LR35902(NAME ## H, uint8_t reg = cpu->h; BODY; cpu->h = reg) \
	DEFINE_INSTRUCTION_LR35902(NAME ## L, uint8_t reg = cpu->l; BODY; cpu->l = reg) \
	DEFINE_INSTRUCTION_LR35902(NAME ## HLDelay, \
		uint8_t reg = cpu->bus; \
		BODY; \
		cpu->bus = reg; \
		cpu->executionState = WB; \
		cpu->instruction = _LR35902InstructionNOP;) \
	DEFINE_INSTRUCTION_LR35902(NAME ## HL, \
		cpu->index = LR35902ReadHL(cpu); \
		cpu->executionState = LR35902_CORE_MEMORY_LOAD; \
		cpu->instruction = _LR35902Instruction ## NAME ## HLDelay;) \
	DEFINE_INSTRUCTION_LR35902(NAME ## A, uint8_t reg = cpu->a; BODY; cpu->a = reg)

#define DEFINE_CB_INSTRUCTION_LR35902(NAME, WB, BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME ## 0, WB, uint8_t bit = 1; BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME ## 1, WB, uint8_t bit = 2; BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME ## 2, WB, uint8_t bit = 4; BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME ## 3, WB, uint8_t bit = 8; BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME ## 4, WB, uint8_t bit = 16; BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME ## 5, WB, uint8_t bit = 32; BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME ## 6, WB, uint8_t bit = 64; BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME ## 7, WB, uint8_t bit = 128; BODY)

DEFINE_CB_INSTRUCTION_LR35902(BIT, LR35902_CORE_FETCH, cpu->f.n = 0; cpu->f.h = 1; cpu->f.z = !(reg & bit))
DEFINE_CB_INSTRUCTION_LR35902(RES, LR35902_CORE_MEMORY_STORE, reg &= ~bit)
DEFINE_CB_INSTRUCTION_LR35902(SET, LR35902_CORE_MEMORY_STORE, reg |= bit)

#define DEFINE_CB_ALU_INSTRUCTION_LR35902(NAME, BODY) \
	DEFINE_CB_2_INSTRUCTION_LR35902(NAME, LR35902_CORE_MEMORY_STORE, \
		BODY; \
		cpu->f.n = 0; \
		cpu->f.h = 0; \
		cpu->f.z = !reg;)

DEFINE_CB_ALU_INSTRUCTION_LR35902(RL, int wide = (reg << 1) | cpu->f.c; reg = wide; cpu->f.c = wide >> 8)
DEFINE_CB_ALU_INSTRUCTION_LR35902(RLC, reg = (reg << 1) | (reg >> 7); cpu->f.c = reg & 1)
DEFINE_CB_ALU_INSTRUCTION_LR35902(RR, int low = reg & 1; reg = (reg >> 1) | (cpu->f.c << 7); cpu->f.c = low)
DEFINE_CB_ALU_INSTRUCTION_LR35902(RRC, int low = reg & 1; reg = (reg >> 1) | (low << 7); cpu->f.c = low)
DEFINE_CB_ALU_INSTRUCTION_LR35902(SLA, cpu->f.c = reg >> 7; reg <<= 1)
DEFINE_CB_ALU_INSTRUCTION_LR35902(SRA, cpu->f.c = reg & 1; reg = ((int8_t) reg) >> 1)
DEFINE_CB_ALU_INSTRUCTION_LR35902(SRL, cpu->f.c = reg & 1; reg >>= 1)
DEFINE_CB_ALU_INSTRUCTION_LR35902(SWAP, reg = (reg << 4) | (reg >> 4); cpu->f.c = 0)

DEFINE_INSTRUCTION_LR35902(RLA_,
	int wide = (cpu->a << 1) | cpu->f.c;
	cpu->a = wide;
	cpu->f.z = 0;
	cpu->f.h = 0;
	cpu->f.n = 0;
	cpu->f.c = wide >> 8;)

DEFINE_INSTRUCTION_LR35902(RLCA_,
	cpu->a = (cpu->a << 1) | (cpu->a >> 7);
	cpu->f.z = 0;
	cpu->f.h = 0;
	cpu->f.n = 0;
	cpu->f.c = cpu->a & 1;)

DEFINE_INSTRUCTION_LR35902(RRA_,
	int low = cpu->a & 1;
	cpu->a = (cpu->a >> 1) | (cpu->f.c << 7);
	cpu->f.z = 0;
	cpu->f.h = 0;
	cpu->f.n = 0;
	cpu->f.c = low;)

DEFINE_INSTRUCTION_LR35902(RRCA_,
	int low = cpu->a & 1;
	cpu->a = (cpu->a >> 1) | (low << 7);
	cpu->f.z = 0;
	cpu->f.h = 0;
	cpu->f.n = 0;
	cpu->f.c = low;)

DEFINE_INSTRUCTION_LR35902(DI, cpu->irqh.setInterrupts(cpu, false));
DEFINE_INSTRUCTION_LR35902(EI, cpu->irqh.setInterrupts(cpu, true));
DEFINE_INSTRUCTION_LR35902(HALT, cpu->irqh.halt(cpu));

#define DEFINE_RST_INSTRUCTION_LR35902(VEC) \
	DEFINE_INSTRUCTION_LR35902(RST ## VEC ## UpdateSPL, \
		--cpu->sp; \
		cpu->index = cpu->sp; \
		cpu->bus = cpu->pc; \
		cpu->pc = 0x ## VEC; \
		cpu->memory.setActiveRegion(cpu, cpu->pc); \
		cpu->executionState = LR35902_CORE_MEMORY_STORE; \
		cpu->instruction = _LR35902InstructionNOP;) \
	DEFINE_INSTRUCTION_LR35902(RST ## VEC ## UpdateSPH, \
		--cpu->sp;\
		cpu->index = cpu->sp; \
		cpu->bus = cpu->pc >> 8; \
		cpu->executionState = LR35902_CORE_MEMORY_STORE; \
		cpu->instruction = _LR35902InstructionRST ## VEC ## UpdateSPL;) \
	DEFINE_INSTRUCTION_LR35902(RST ## VEC, \
		cpu->executionState = LR35902_CORE_OP2; \
		cpu->instruction = _LR35902InstructionRST ## VEC ## UpdateSPH;)

DEFINE_RST_INSTRUCTION_LR35902(00);
DEFINE_RST_INSTRUCTION_LR35902(08);
DEFINE_RST_INSTRUCTION_LR35902(10);
DEFINE_RST_INSTRUCTION_LR35902(18);
DEFINE_RST_INSTRUCTION_LR35902(20);
DEFINE_RST_INSTRUCTION_LR35902(28);
DEFINE_RST_INSTRUCTION_LR35902(30);
DEFINE_RST_INSTRUCTION_LR35902(38);

DEFINE_INSTRUCTION_LR35902(ILL, cpu->irqh.hitIllegal(cpu));

DEFINE_INSTRUCTION_LR35902(STOP2, cpu->irqh.stop(cpu));

DEFINE_INSTRUCTION_LR35902(STOP, \
	cpu->executionState = LR35902_CORE_READ_PC; \
	cpu->instruction = _LR35902InstructionSTOP2;)

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
