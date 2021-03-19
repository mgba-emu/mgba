/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/sm83/isa-sm83.h>

#include <mgba/internal/sm83/emitter-sm83.h>
#include <mgba/internal/sm83/sm83.h>

static inline uint16_t SM83ReadHL(struct SM83Core* cpu) {
	return cpu->hl;
}

static inline void SM83WriteHL(struct SM83Core* cpu, uint16_t hl) {
	cpu->hl = hl;
}

static inline uint16_t SM83ReadBC(struct SM83Core* cpu) {
	return cpu->bc;
}

static inline void SM83WriteBC(struct SM83Core* cpu, uint16_t bc) {
	cpu->bc = bc;
}

static inline uint16_t SM83ReadDE(struct SM83Core* cpu) {
	return cpu->de;
}

static inline void SM83WriteDE(struct SM83Core* cpu, uint16_t de) {
	cpu->de = de;
}

#define DEFINE_INSTRUCTION_SM83(NAME, BODY) \
	static void _SM83Instruction ## NAME (struct SM83Core* cpu) { \
		UNUSED(cpu); \
		BODY; \
	}

DEFINE_INSTRUCTION_SM83(NOP,);

#define DEFINE_CONDITIONAL_ONLY_INSTRUCTION_SM83(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(C, cpu->f.c) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(Z, cpu->f.z) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(NC, !cpu->f.c) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(NZ, !cpu->f.z)

#define DEFINE_CONDITIONAL_INSTRUCTION_SM83(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(, true) \
	DEFINE_CONDITIONAL_ONLY_INSTRUCTION_SM83(NAME)

DEFINE_INSTRUCTION_SM83(JPFinish,
	if (cpu->condition) {
		cpu->pc = (cpu->bus << 8) | cpu->index;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		cpu->executionState = SM83_CORE_STALL;
	})

DEFINE_INSTRUCTION_SM83(JPDelay,
	cpu->executionState = SM83_CORE_READ_PC;
	cpu->instruction = _SM83InstructionJPFinish;
	cpu->index = cpu->bus;)

#define DEFINE_JP_INSTRUCTION_SM83(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_SM83(JP ## CONDITION_NAME, \
		cpu->executionState = SM83_CORE_READ_PC; \
		cpu->instruction = _SM83InstructionJPDelay; \
		cpu->condition = CONDITION;)

DEFINE_CONDITIONAL_INSTRUCTION_SM83(JP);

DEFINE_INSTRUCTION_SM83(JPHL,
	cpu->pc = cpu->hl;
	cpu->memory.setActiveRegion(cpu, cpu->pc);)

DEFINE_INSTRUCTION_SM83(JRFinish,
	if (cpu->condition) {
		cpu->pc += (int8_t) cpu->bus;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		cpu->executionState = SM83_CORE_STALL;
	})

#define DEFINE_JR_INSTRUCTION_SM83(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_SM83(JR ## CONDITION_NAME, \
		cpu->executionState = SM83_CORE_READ_PC; \
		cpu->instruction = _SM83InstructionJRFinish; \
		cpu->condition = CONDITION;)

DEFINE_CONDITIONAL_INSTRUCTION_SM83(JR);

DEFINE_INSTRUCTION_SM83(CALLUpdateSPL,
	--cpu->index;
	cpu->bus = cpu->sp;
	cpu->sp = cpu->index;
	cpu->executionState = SM83_CORE_MEMORY_STORE;
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(CALLUpdateSPH,
	cpu->executionState = SM83_CORE_MEMORY_STORE;
	cpu->instruction = _SM83InstructionCALLUpdateSPL;)

DEFINE_INSTRUCTION_SM83(CALLUpdatePCH,
	if (cpu->condition) {
		int newPc = (cpu->bus << 8) | cpu->index;
		cpu->bus = cpu->pc >> 8;
		cpu->index = cpu->sp - 1;
		cpu->sp = cpu->pc; // GROSS
		cpu->pc = newPc;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		cpu->executionState = SM83_CORE_OP2;
		cpu->instruction = _SM83InstructionCALLUpdateSPH;
	})

DEFINE_INSTRUCTION_SM83(CALLUpdatePCL,
	cpu->executionState = SM83_CORE_READ_PC;
	cpu->index = cpu->bus;
	cpu->instruction = _SM83InstructionCALLUpdatePCH)

#define DEFINE_CALL_INSTRUCTION_SM83(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_SM83(CALL ## CONDITION_NAME, \
		cpu->condition = CONDITION; \
		cpu->executionState = SM83_CORE_READ_PC; \
		cpu->instruction = _SM83InstructionCALLUpdatePCL;)

DEFINE_CONDITIONAL_INSTRUCTION_SM83(CALL)

DEFINE_INSTRUCTION_SM83(RETFinish,
	cpu->sp += 2;  /* TODO: Atomic incrementing? */
	cpu->pc |= cpu->bus << 8;
	cpu->memory.setActiveRegion(cpu, cpu->pc);
	cpu->executionState = SM83_CORE_STALL;)

DEFINE_INSTRUCTION_SM83(RETUpdateSPL,
	cpu->index = cpu->sp + 1;
	cpu->pc = cpu->bus;
	cpu->executionState = SM83_CORE_MEMORY_LOAD;
	cpu->instruction = _SM83InstructionRETFinish;)

DEFINE_INSTRUCTION_SM83(RETUpdateSPH,
	if (cpu->condition) {
		cpu->index = cpu->sp;
		cpu->executionState = SM83_CORE_MEMORY_LOAD;
		cpu->instruction = _SM83InstructionRETUpdateSPL;
	})

#define DEFINE_RET_INSTRUCTION_SM83(CONDITION_NAME, CONDITION) \
	DEFINE_INSTRUCTION_SM83(RET ## CONDITION_NAME, \
		cpu->condition = CONDITION; \
		cpu->executionState = SM83_CORE_OP2; \
		cpu->instruction = _SM83InstructionRETUpdateSPH;)

DEFINE_INSTRUCTION_SM83(RET,
	cpu->condition = true;
	_SM83InstructionRETUpdateSPH(cpu);)

DEFINE_INSTRUCTION_SM83(RETI,
	cpu->condition = true;
	cpu->irqh.setInterrupts(cpu, true);
	_SM83InstructionRETUpdateSPH(cpu);)

DEFINE_CONDITIONAL_ONLY_INSTRUCTION_SM83(RET)

#define DEFINE_AND_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(AND ## NAME, \
		cpu->a &= OPERAND; \
		cpu->f.z = !cpu->a; \
		cpu->f.n = 0; \
		cpu->f.c = 0; \
		cpu->f.h = 1;)

#define DEFINE_XOR_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(XOR ## NAME, \
		cpu->a ^= OPERAND; \
		cpu->f.z = !cpu->a; \
		cpu->f.n = 0; \
		cpu->f.c = 0; \
		cpu->f.h = 0;)

#define DEFINE_OR_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(OR ## NAME, \
		cpu->a |= OPERAND; \
		cpu->f.z = !cpu->a; \
		cpu->f.n = 0; \
		cpu->f.c = 0; \
		cpu->f.h = 0;)

#define DEFINE_CP_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(CP ## NAME, \
		int diff = cpu->a - OPERAND; \
		cpu->f.n = 1; \
		cpu->f.z = !(diff & 0xFF); \
		cpu->f.h = (cpu->a & 0xF) - (OPERAND & 0xF) < 0; \
		cpu->f.c = diff < 0;)

#define DEFINE_LDB__INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(LDB_ ## NAME, \
		cpu->b = OPERAND;)

#define DEFINE_LDC__INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(LDC_ ## NAME, \
		cpu->c = OPERAND;)

#define DEFINE_LDD__INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(LDD_ ## NAME, \
		cpu->d = OPERAND;)

#define DEFINE_LDE__INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(LDE_ ## NAME, \
		cpu->e = OPERAND;)

#define DEFINE_LDH__INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(LDH_ ## NAME, \
		cpu->h = OPERAND;)

#define DEFINE_LDL__INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(LDL_ ## NAME, \
		cpu->l = OPERAND;)

#define DEFINE_LDHL__INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(LDHL_ ## NAME, \
		cpu->bus = OPERAND; \
		cpu->index = cpu->hl; \
		cpu->executionState = SM83_CORE_MEMORY_STORE; \
		cpu->instruction = _SM83InstructionNOP;)

#define DEFINE_LDA__INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(LDA_ ## NAME, \
		cpu->a = OPERAND;)

#define DEFINE_ALU_INSTRUCTION_SM83_NOHL(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(A, cpu->a); \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(B, cpu->b); \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(C, cpu->c); \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(D, cpu->d); \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(E, cpu->e); \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(H, cpu->h); \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(L, cpu->l);

DEFINE_INSTRUCTION_SM83(LDHL_Bus, \
	cpu->index = cpu->hl; \
	cpu->executionState = SM83_CORE_MEMORY_STORE; \
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDHL_, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDHL_Bus;)

DEFINE_INSTRUCTION_SM83(LDHL_SPDelay,
	int diff = (int8_t) cpu->bus;
	int sum = cpu->sp + diff;
	SM83WriteHL(cpu, sum);
	cpu->executionState = SM83_CORE_STALL;
	cpu->f.z = 0;
	cpu->f.n = 0;
	cpu->f.c = (diff & 0xFF) + (cpu->sp & 0xFF) >= 0x100;
	cpu->f.h = (diff & 0xF) + (cpu->sp & 0xF) >= 0x10;)

DEFINE_INSTRUCTION_SM83(LDHL_SP,
	cpu->executionState = SM83_CORE_READ_PC;
	cpu->instruction = _SM83InstructionLDHL_SPDelay;)

DEFINE_INSTRUCTION_SM83(LDSP_HL,
	cpu->sp = cpu->hl;
	cpu->executionState = SM83_CORE_STALL;)

#define DEFINE_ALU_INSTRUCTION_SM83_MEM(NAME, REG) \
	DEFINE_INSTRUCTION_SM83(NAME ## REG, \
		cpu->executionState = SM83_CORE_MEMORY_LOAD; \
		cpu->index = SM83Read ## REG (cpu); \
		cpu->instruction = _SM83Instruction ## NAME ## Bus;)

#define DEFINE_ALU_INSTRUCTION_SM83(NAME) \
	DEFINE_ ## NAME ## _INSTRUCTION_SM83(Bus, cpu->bus); \
	DEFINE_ALU_INSTRUCTION_SM83_MEM(NAME, HL) \
	DEFINE_INSTRUCTION_SM83(NAME, \
		cpu->executionState = SM83_CORE_READ_PC; \
		cpu->instruction = _SM83Instruction ## NAME ## Bus;) \
	DEFINE_ALU_INSTRUCTION_SM83_NOHL(NAME)

DEFINE_ALU_INSTRUCTION_SM83(AND);
DEFINE_ALU_INSTRUCTION_SM83(XOR);
DEFINE_ALU_INSTRUCTION_SM83(OR);
DEFINE_ALU_INSTRUCTION_SM83(CP);

static void _SM83InstructionLDB_Bus(struct SM83Core*);
static void _SM83InstructionLDC_Bus(struct SM83Core*);
static void _SM83InstructionLDD_Bus(struct SM83Core*);
static void _SM83InstructionLDE_Bus(struct SM83Core*);
static void _SM83InstructionLDH_Bus(struct SM83Core*);
static void _SM83InstructionLDL_Bus(struct SM83Core*);
static void _SM83InstructionLDHL_Bus(struct SM83Core*);
static void _SM83InstructionLDA_Bus(struct SM83Core*);

#define DEFINE_ADD_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(ADD ## NAME, \
		int diff = cpu->a + OPERAND; \
		cpu->f.n = 0; \
		cpu->f.h = (cpu->a & 0xF) + (OPERAND & 0xF) >= 0x10; \
		cpu->f.c = diff >= 0x100; \
		cpu->a = diff; \
		cpu->f.z = !cpu->a;)

#define DEFINE_ADC_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(ADC ## NAME, \
		int diff = cpu->a + OPERAND + cpu->f.c; \
		cpu->f.n = 0; \
		cpu->f.h = (cpu->a & 0xF) + (OPERAND & 0xF) + cpu->f.c >= 0x10; \
		cpu->f.c = diff >= 0x100; \
		cpu->a = diff; \
		cpu->f.z = !cpu->a;)

#define DEFINE_SUB_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(SUB ## NAME, \
		int diff = cpu->a - OPERAND; \
		cpu->f.n = 1; \
		cpu->f.h = (cpu->a & 0xF) - (OPERAND & 0xF) < 0; \
		cpu->f.c = diff < 0; \
		cpu->a = diff; \
		cpu->f.z = !cpu->a;)

#define DEFINE_SBC_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(SBC ## NAME, \
		int diff = cpu->a - OPERAND - cpu->f.c; \
		cpu->f.n = 1; \
		cpu->f.h = (cpu->a & 0xF) - (OPERAND & 0xF) - cpu->f.c < 0; \
		cpu->f.c = diff < 0; \
		cpu->a = diff; \
		cpu->f.z = !cpu->a;)

DEFINE_ALU_INSTRUCTION_SM83(LDB_);
DEFINE_ALU_INSTRUCTION_SM83(LDC_);
DEFINE_ALU_INSTRUCTION_SM83(LDD_);
DEFINE_ALU_INSTRUCTION_SM83(LDE_);
DEFINE_ALU_INSTRUCTION_SM83(LDH_);
DEFINE_ALU_INSTRUCTION_SM83(LDL_);
DEFINE_ALU_INSTRUCTION_SM83_NOHL(LDHL_);
DEFINE_ALU_INSTRUCTION_SM83(LDA_);
DEFINE_ALU_INSTRUCTION_SM83_MEM(LDA_, BC);
DEFINE_ALU_INSTRUCTION_SM83_MEM(LDA_, DE);
DEFINE_ALU_INSTRUCTION_SM83(ADD);
DEFINE_ALU_INSTRUCTION_SM83(ADC);
DEFINE_ALU_INSTRUCTION_SM83(SUB);
DEFINE_ALU_INSTRUCTION_SM83(SBC);

DEFINE_INSTRUCTION_SM83(ADDSPFinish,
	cpu->sp = cpu->index;
	cpu->executionState = SM83_CORE_STALL;)

DEFINE_INSTRUCTION_SM83(ADDSPDelay,
	int diff = (int8_t) cpu->bus;
	int sum = cpu->sp + diff;
	cpu->index = sum;
	cpu->executionState = SM83_CORE_OP2;
	cpu->instruction = _SM83InstructionADDSPFinish;
	cpu->f.z = 0;
	cpu->f.n = 0;
	cpu->f.c = (diff & 0xFF) + (cpu->sp & 0xFF) >= 0x100;
	cpu->f.h = (diff & 0xF) + (cpu->sp & 0xF) >= 0x10;)

DEFINE_INSTRUCTION_SM83(ADDSP,
	cpu->executionState = SM83_CORE_READ_PC;
	cpu->instruction = _SM83InstructionADDSPDelay;)

DEFINE_INSTRUCTION_SM83(LDBCDelay, \
	cpu->c = cpu->bus; \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDB_Bus;)

DEFINE_INSTRUCTION_SM83(LDBC, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDBCDelay;)

DEFINE_INSTRUCTION_SM83(LDBC_A, \
	cpu->index = cpu->bc; \
	cpu->bus = cpu->a; \
	cpu->executionState = SM83_CORE_MEMORY_STORE; \
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDDEDelay, \
	cpu->e = cpu->bus; \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDD_Bus;)

DEFINE_INSTRUCTION_SM83(LDDE, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDDEDelay;)

DEFINE_INSTRUCTION_SM83(LDDE_A, \
	cpu->index = cpu->de; \
	cpu->bus = cpu->a; \
	cpu->executionState = SM83_CORE_MEMORY_STORE; \
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDHLDelay, \
	cpu->l = cpu->bus; \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDH_Bus;)

DEFINE_INSTRUCTION_SM83(LDHL, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDHLDelay;)

DEFINE_INSTRUCTION_SM83(LDSPFinish, cpu->sp |= cpu->bus << 8;)

DEFINE_INSTRUCTION_SM83(LDSPDelay, \
	cpu->sp = cpu->bus; \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDSPFinish;)

DEFINE_INSTRUCTION_SM83(LDSP, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDSPDelay;)

DEFINE_INSTRUCTION_SM83(LDIHLA, \
	cpu->index = cpu->hl; \
	SM83WriteHL(cpu, cpu->index + 1); \
	cpu->bus = cpu->a; \
	cpu->executionState = SM83_CORE_MEMORY_STORE; \
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDDHLA, \
	cpu->index = cpu->hl; \
	SM83WriteHL(cpu, cpu->index - 1); \
	cpu->bus = cpu->a; \
	cpu->executionState = SM83_CORE_MEMORY_STORE; \
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDA_IHL, \
	cpu->index = cpu->hl; \
	SM83WriteHL(cpu, cpu->index + 1); \
	cpu->executionState = SM83_CORE_MEMORY_LOAD; \
	cpu->instruction = _SM83InstructionLDA_Bus;)

DEFINE_INSTRUCTION_SM83(LDA_DHL, \
	cpu->index = cpu->hl; \
	SM83WriteHL(cpu, cpu->index - 1); \
	cpu->executionState = SM83_CORE_MEMORY_LOAD; \
	cpu->instruction = _SM83InstructionLDA_Bus;)

DEFINE_INSTRUCTION_SM83(LDIAFinish, \
	cpu->index |= cpu->bus << 8;
	cpu->bus = cpu->a; \
	cpu->executionState = SM83_CORE_MEMORY_STORE; \
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDIADelay, \
	cpu->index = cpu->bus;
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDIAFinish;)

DEFINE_INSTRUCTION_SM83(LDIA, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDIADelay;)

DEFINE_INSTRUCTION_SM83(LDAIFinish, \
	cpu->index |= cpu->bus << 8;
	cpu->executionState = SM83_CORE_MEMORY_LOAD; \
	cpu->instruction = _SM83InstructionLDA_Bus;)

DEFINE_INSTRUCTION_SM83(LDAIDelay, \
	cpu->index = cpu->bus;
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDAIFinish;)

DEFINE_INSTRUCTION_SM83(LDAI, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDAIDelay;)

DEFINE_INSTRUCTION_SM83(LDAIOC, \
	cpu->index = 0xFF00 | cpu->c; \
	cpu->executionState = SM83_CORE_MEMORY_LOAD; \
	cpu->instruction = _SM83InstructionLDA_Bus;)

DEFINE_INSTRUCTION_SM83(LDIOCA, \
	cpu->index = 0xFF00 | cpu->c; \
	cpu->bus = cpu->a; \
	cpu->executionState = SM83_CORE_MEMORY_STORE; \
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDAIODelay, \
	cpu->index = 0xFF00 | cpu->bus; \
	cpu->executionState = SM83_CORE_MEMORY_LOAD; \
	cpu->instruction = _SM83InstructionLDA_Bus;)

DEFINE_INSTRUCTION_SM83(LDAIO, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDAIODelay;)

DEFINE_INSTRUCTION_SM83(LDIOADelay, \
	cpu->index = 0xFF00 | cpu->bus; \
	cpu->bus = cpu->a; \
	cpu->executionState = SM83_CORE_MEMORY_STORE; \
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDIOA, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionLDIOADelay;)

DEFINE_INSTRUCTION_SM83(LDISPStoreH,
	++cpu->index;
	cpu->bus = cpu->sp >> 8;
	cpu->executionState = SM83_CORE_MEMORY_STORE;
	cpu->instruction = _SM83InstructionNOP;)

DEFINE_INSTRUCTION_SM83(LDISPStoreL,
	cpu->index |= cpu->bus << 8;
	cpu->bus = cpu->sp;
	cpu->executionState = SM83_CORE_MEMORY_STORE;
	cpu->instruction = _SM83InstructionLDISPStoreH;)

DEFINE_INSTRUCTION_SM83(LDISPReadAddr,
	cpu->index = cpu->bus;
	cpu->executionState = SM83_CORE_READ_PC;
	cpu->instruction = _SM83InstructionLDISPStoreL;)

DEFINE_INSTRUCTION_SM83(LDISP,
	cpu->executionState = SM83_CORE_READ_PC;
	cpu->instruction = _SM83InstructionLDISPReadAddr;)

#define DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(REG) \
	DEFINE_INSTRUCTION_SM83(INC ## REG, \
		uint16_t reg = SM83Read ## REG (cpu); \
		SM83Write ## REG (cpu, reg + 1); \
		cpu->executionState = SM83_CORE_STALL;) \
	DEFINE_INSTRUCTION_SM83(DEC ## REG, \
		uint16_t reg = SM83Read ## REG (cpu); \
		SM83Write ## REG (cpu, reg - 1); \
		cpu->executionState = SM83_CORE_STALL;)

DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(BC);
DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(DE);
DEFINE_INCDEC_WIDE_INSTRUCTION_SM83(HL);

#define DEFINE_ADD_HL_INSTRUCTION_SM83(REG, L, H) \
	DEFINE_INSTRUCTION_SM83(ADDHL_ ## REG ## Finish, \
		int diff = H + cpu->h + cpu->f.c; \
		cpu->f.n = 0; \
		cpu->f.h = (H & 0xF) + (cpu->h & 0xF) + cpu->f.c >= 0x10; \
		cpu->f.c = diff >= 0x100; \
		cpu->h = diff;) \
	DEFINE_INSTRUCTION_SM83(ADDHL_ ## REG, \
		int diff = L + cpu->l; \
		cpu->l = diff; \
		cpu->f.c = diff >= 0x100; \
		cpu->executionState = SM83_CORE_OP2; \
		cpu->instruction = _SM83InstructionADDHL_ ## REG ## Finish;)

DEFINE_ADD_HL_INSTRUCTION_SM83(BC, cpu->c, cpu->b);
DEFINE_ADD_HL_INSTRUCTION_SM83(DE, cpu->e, cpu->d);
DEFINE_ADD_HL_INSTRUCTION_SM83(HL, cpu->l, cpu->h);
DEFINE_ADD_HL_INSTRUCTION_SM83(SP, (cpu->sp & 0xFF), (cpu->sp >> 8));


#define DEFINE_INC_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(INC ## NAME, \
		int diff = OPERAND + 1; \
		cpu->f.h = (OPERAND & 0xF) == 0xF; \
		OPERAND = diff; \
		cpu->f.n = 0; \
		cpu->f.z = !OPERAND;)

#define DEFINE_DEC_INSTRUCTION_SM83(NAME, OPERAND) \
	DEFINE_INSTRUCTION_SM83(DEC ## NAME, \
		int diff = OPERAND - 1; \
		cpu->f.h = (OPERAND & 0xF) == 0x0; \
		OPERAND = diff; \
		cpu->f.n = 1; \
		cpu->f.z = !OPERAND;)

DEFINE_ALU_INSTRUCTION_SM83_NOHL(INC);
DEFINE_ALU_INSTRUCTION_SM83_NOHL(DEC);

DEFINE_INSTRUCTION_SM83(INC_HLDelay,
	int diff = cpu->bus + 1;
	cpu->f.n = 0;
	cpu->f.h = (cpu->bus & 0xF) == 0xF;
	cpu->bus = diff;
	cpu->f.z = !cpu->bus;
	cpu->instruction = _SM83InstructionNOP;
	cpu->executionState = SM83_CORE_MEMORY_STORE;)

DEFINE_INSTRUCTION_SM83(INC_HL,
	cpu->index = cpu->hl;
	cpu->instruction = _SM83InstructionINC_HLDelay;
	cpu->executionState = SM83_CORE_MEMORY_LOAD;)

DEFINE_INSTRUCTION_SM83(DEC_HLDelay,
	int diff = cpu->bus - 1;
	cpu->f.n = 1;
	cpu->f.h = (cpu->bus & 0xF) == 0;
	cpu->bus = diff;
	cpu->f.z = !cpu->bus;
	cpu->instruction = _SM83InstructionNOP;
	cpu->executionState = SM83_CORE_MEMORY_STORE;)

DEFINE_INSTRUCTION_SM83(DEC_HL,
	cpu->index = cpu->hl;
	cpu->instruction = _SM83InstructionDEC_HLDelay;
	cpu->executionState = SM83_CORE_MEMORY_LOAD;)

DEFINE_INSTRUCTION_SM83(INCSP,
	++cpu->sp;
	cpu->executionState = SM83_CORE_STALL;)

DEFINE_INSTRUCTION_SM83(DECSP,
	--cpu->sp;
	cpu->executionState = SM83_CORE_STALL;)

DEFINE_INSTRUCTION_SM83(SCF,
	cpu->f.c = 1;
	cpu->f.h = 0;
	cpu->f.n = 0;)

DEFINE_INSTRUCTION_SM83(CCF,
	cpu->f.c ^= 1;
	cpu->f.h = 0;
	cpu->f.n = 0;)

DEFINE_INSTRUCTION_SM83(CPL_,
	cpu->a ^= 0xFF;
	cpu->f.h = 1;
	cpu->f.n = 1;)

DEFINE_INSTRUCTION_SM83(DAA,
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

#define DEFINE_POPPUSH_INSTRUCTION_SM83(REG, HH, H, L) \
	DEFINE_INSTRUCTION_SM83(POP ## REG ## Delay, \
		cpu-> L = cpu->bus; \
		cpu->f.packed &= 0xF0; \
		cpu->index = cpu->sp; \
		++cpu->sp; \
		cpu->instruction = _SM83InstructionLD ## HH ## _Bus; \
		cpu->executionState = SM83_CORE_MEMORY_LOAD;) \
	DEFINE_INSTRUCTION_SM83(POP ## REG, \
		cpu->index = cpu->sp; \
		++cpu->sp; \
		cpu->instruction = _SM83InstructionPOP ## REG ## Delay; \
		cpu->executionState = SM83_CORE_MEMORY_LOAD;) \
	DEFINE_INSTRUCTION_SM83(PUSH ## REG ## Finish, \
		cpu->executionState = SM83_CORE_STALL;) \
	DEFINE_INSTRUCTION_SM83(PUSH ## REG ## Delay, \
		--cpu->sp; \
		cpu->index = cpu->sp; \
		cpu->bus = cpu-> L; \
		cpu->instruction = _SM83InstructionPUSH ## REG ## Finish; \
		cpu->executionState = SM83_CORE_MEMORY_STORE;) \
	DEFINE_INSTRUCTION_SM83(PUSH ## REG, \
		--cpu->sp; \
		cpu->index = cpu->sp; \
		cpu->bus = cpu-> H; \
		cpu->instruction = _SM83InstructionPUSH ## REG ## Delay; \
		cpu->executionState = SM83_CORE_MEMORY_STORE;)

DEFINE_POPPUSH_INSTRUCTION_SM83(BC, B, b, c);
DEFINE_POPPUSH_INSTRUCTION_SM83(DE, D, d, e);
DEFINE_POPPUSH_INSTRUCTION_SM83(HL, H, h, l);
DEFINE_POPPUSH_INSTRUCTION_SM83(AF, A, a, f.packed);

#define DEFINE_CB_2_INSTRUCTION_SM83(NAME, WB, BODY) \
	DEFINE_INSTRUCTION_SM83(NAME ## B, uint8_t reg = cpu->b; BODY; cpu->b = reg) \
	DEFINE_INSTRUCTION_SM83(NAME ## C, uint8_t reg = cpu->c; BODY; cpu->c = reg) \
	DEFINE_INSTRUCTION_SM83(NAME ## D, uint8_t reg = cpu->d; BODY; cpu->d = reg) \
	DEFINE_INSTRUCTION_SM83(NAME ## E, uint8_t reg = cpu->e; BODY; cpu->e = reg) \
	DEFINE_INSTRUCTION_SM83(NAME ## H, uint8_t reg = cpu->h; BODY; cpu->h = reg) \
	DEFINE_INSTRUCTION_SM83(NAME ## L, uint8_t reg = cpu->l; BODY; cpu->l = reg) \
	DEFINE_INSTRUCTION_SM83(NAME ## HLDelay, \
		uint8_t reg = cpu->bus; \
		BODY; \
		cpu->bus = reg; \
		cpu->executionState = WB; \
		cpu->instruction = _SM83InstructionNOP;) \
	DEFINE_INSTRUCTION_SM83(NAME ## HL, \
		cpu->index = cpu->hl; \
		cpu->executionState = SM83_CORE_MEMORY_LOAD; \
		cpu->instruction = _SM83Instruction ## NAME ## HLDelay;) \
	DEFINE_INSTRUCTION_SM83(NAME ## A, uint8_t reg = cpu->a; BODY; cpu->a = reg)

#define DEFINE_CB_INSTRUCTION_SM83(NAME, WB, BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME ## 0, WB, uint8_t bit = 1; BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME ## 1, WB, uint8_t bit = 2; BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME ## 2, WB, uint8_t bit = 4; BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME ## 3, WB, uint8_t bit = 8; BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME ## 4, WB, uint8_t bit = 16; BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME ## 5, WB, uint8_t bit = 32; BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME ## 6, WB, uint8_t bit = 64; BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME ## 7, WB, uint8_t bit = 128; BODY)

DEFINE_CB_INSTRUCTION_SM83(BIT, SM83_CORE_FETCH, cpu->f.n = 0; cpu->f.h = 1; cpu->f.z = !(reg & bit))
DEFINE_CB_INSTRUCTION_SM83(RES, SM83_CORE_MEMORY_STORE, reg &= ~bit)
DEFINE_CB_INSTRUCTION_SM83(SET, SM83_CORE_MEMORY_STORE, reg |= bit)

#define DEFINE_CB_ALU_INSTRUCTION_SM83(NAME, BODY) \
	DEFINE_CB_2_INSTRUCTION_SM83(NAME, SM83_CORE_MEMORY_STORE, \
		BODY; \
		cpu->f.n = 0; \
		cpu->f.h = 0; \
		cpu->f.z = !reg;)

DEFINE_CB_ALU_INSTRUCTION_SM83(RL, int wide = (reg << 1) | cpu->f.c; reg = wide; cpu->f.c = wide >> 8)
DEFINE_CB_ALU_INSTRUCTION_SM83(RLC, reg = (reg << 1) | (reg >> 7); cpu->f.c = reg & 1)
DEFINE_CB_ALU_INSTRUCTION_SM83(RR, int low = reg & 1; reg = (reg >> 1) | (cpu->f.c << 7); cpu->f.c = low)
DEFINE_CB_ALU_INSTRUCTION_SM83(RRC, int low = reg & 1; reg = (reg >> 1) | (low << 7); cpu->f.c = low)
DEFINE_CB_ALU_INSTRUCTION_SM83(SLA, cpu->f.c = reg >> 7; reg <<= 1)
DEFINE_CB_ALU_INSTRUCTION_SM83(SRA, cpu->f.c = reg & 1; reg = ((int8_t) reg) >> 1)
DEFINE_CB_ALU_INSTRUCTION_SM83(SRL, cpu->f.c = reg & 1; reg >>= 1)
DEFINE_CB_ALU_INSTRUCTION_SM83(SWAP, reg = (reg << 4) | (reg >> 4); cpu->f.c = 0)

DEFINE_INSTRUCTION_SM83(RLA_,
	int wide = (cpu->a << 1) | cpu->f.c;
	cpu->a = wide;
	cpu->f.z = 0;
	cpu->f.h = 0;
	cpu->f.n = 0;
	cpu->f.c = wide >> 8;)

DEFINE_INSTRUCTION_SM83(RLCA_,
	cpu->a = (cpu->a << 1) | (cpu->a >> 7);
	cpu->f.z = 0;
	cpu->f.h = 0;
	cpu->f.n = 0;
	cpu->f.c = cpu->a & 1;)

DEFINE_INSTRUCTION_SM83(RRA_,
	int low = cpu->a & 1;
	cpu->a = (cpu->a >> 1) | (cpu->f.c << 7);
	cpu->f.z = 0;
	cpu->f.h = 0;
	cpu->f.n = 0;
	cpu->f.c = low;)

DEFINE_INSTRUCTION_SM83(RRCA_,
	int low = cpu->a & 1;
	cpu->a = (cpu->a >> 1) | (low << 7);
	cpu->f.z = 0;
	cpu->f.h = 0;
	cpu->f.n = 0;
	cpu->f.c = low;)

DEFINE_INSTRUCTION_SM83(DI, cpu->irqh.setInterrupts(cpu, false));
DEFINE_INSTRUCTION_SM83(EI, cpu->irqh.setInterrupts(cpu, true));
DEFINE_INSTRUCTION_SM83(HALT,
	cpu->irqh.halt(cpu);
	// XXX: Subtract the cycles that will be added later in the tick function
	cpu->cycles -= cpu->tMultiplier;);

#define DEFINE_RST_INSTRUCTION_SM83(VEC) \
	DEFINE_INSTRUCTION_SM83(RST ## VEC ## UpdateSPL, \
		--cpu->sp; \
		cpu->index = cpu->sp; \
		cpu->bus = cpu->pc; \
		cpu->pc = 0x ## VEC; \
		cpu->memory.setActiveRegion(cpu, cpu->pc); \
		cpu->executionState = SM83_CORE_MEMORY_STORE; \
		cpu->instruction = _SM83InstructionNOP;) \
	DEFINE_INSTRUCTION_SM83(RST ## VEC ## UpdateSPH, \
		--cpu->sp;\
		cpu->index = cpu->sp; \
		cpu->bus = cpu->pc >> 8; \
		cpu->executionState = SM83_CORE_MEMORY_STORE; \
		cpu->instruction = _SM83InstructionRST ## VEC ## UpdateSPL;) \
	DEFINE_INSTRUCTION_SM83(RST ## VEC, \
		cpu->executionState = SM83_CORE_OP2; \
		cpu->instruction = _SM83InstructionRST ## VEC ## UpdateSPH;)

DEFINE_RST_INSTRUCTION_SM83(00);
DEFINE_RST_INSTRUCTION_SM83(08);
DEFINE_RST_INSTRUCTION_SM83(10);
DEFINE_RST_INSTRUCTION_SM83(18);
DEFINE_RST_INSTRUCTION_SM83(20);
DEFINE_RST_INSTRUCTION_SM83(28);
DEFINE_RST_INSTRUCTION_SM83(30);
DEFINE_RST_INSTRUCTION_SM83(38);

DEFINE_INSTRUCTION_SM83(ILL, cpu->irqh.hitIllegal(cpu));

DEFINE_INSTRUCTION_SM83(STOP2, cpu->irqh.stop(cpu));

DEFINE_INSTRUCTION_SM83(STOP, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionSTOP2;)

static const SM83Instruction _sm83CBInstructionTable[0x100] = {
	DECLARE_SM83_CB_EMITTER_BLOCK(_SM83Instruction)
};

DEFINE_INSTRUCTION_SM83(CBDelegate, _sm83CBInstructionTable[cpu->bus](cpu))

DEFINE_INSTRUCTION_SM83(CB, \
	cpu->executionState = SM83_CORE_READ_PC; \
	cpu->instruction = _SM83InstructionCBDelegate;)

const SM83Instruction _sm83InstructionTable[0x100] = {
	DECLARE_SM83_EMITTER_BLOCK(_SM83Instruction)
};
