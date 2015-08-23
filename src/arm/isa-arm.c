/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "isa-arm.h"

#include "arm.h"
#include "emitter-arm.h"
#include "isa-inlines.h"

#define PSR_USER_MASK   0xF0000000
#define PSR_PRIV_MASK   0x000000CF
#define PSR_STATE_MASK  0x00000020

// Addressing mode 1
static inline void _shiftLSL(struct ARMCore* cpu, uint32_t opcode) {
	int rm = opcode & 0x0000000F;
	int immediate = (opcode & 0x00000F80) >> 7;
	if (!immediate) {
		cpu->shifterOperand = cpu->gprs[rm];
		cpu->shifterCarryOut = cpu->cpsr.c;
	} else {
		cpu->shifterOperand = cpu->gprs[rm] << immediate;
		cpu->shifterCarryOut = (cpu->gprs[rm] >> (32 - immediate)) & 1;
	}
}

static inline void _shiftLSLR(struct ARMCore* cpu, uint32_t opcode) {
	int rm = opcode & 0x0000000F;
	int rs = (opcode >> 8) & 0x0000000F;
	++cpu->cycles;
	int shift = cpu->gprs[rs];
	if (rs == ARM_PC) {
		shift += 4;
	}
	shift &= 0xFF;
	int32_t shiftVal = cpu->gprs[rm];
	if (rm == ARM_PC) {
		shiftVal += 4;
	}
	if (!shift) {
		cpu->shifterOperand = shiftVal;
		cpu->shifterCarryOut = cpu->cpsr.c;
	} else if (shift < 32) {
		cpu->shifterOperand = shiftVal << shift;
		cpu->shifterCarryOut = (shiftVal >> (32 - shift)) & 1;
	} else if (shift == 32) {
		cpu->shifterOperand = 0;
		cpu->shifterCarryOut = shiftVal & 1;
	} else {
		cpu->shifterOperand = 0;
		cpu->shifterCarryOut = 0;
	}
}

static inline void _shiftLSR(struct ARMCore* cpu, uint32_t opcode) {
	int rm = opcode & 0x0000000F;
	int immediate = (opcode & 0x00000F80) >> 7;
	if (immediate) {
		cpu->shifterOperand = ((uint32_t) cpu->gprs[rm]) >> immediate;
		cpu->shifterCarryOut = (cpu->gprs[rm] >> (immediate - 1)) & 1;
	} else {
		cpu->shifterOperand = 0;
		cpu->shifterCarryOut = ARM_SIGN(cpu->gprs[rm]);
	}
}

static inline void _shiftLSRR(struct ARMCore* cpu, uint32_t opcode) {
	int rm = opcode & 0x0000000F;
	int rs = (opcode >> 8) & 0x0000000F;
	++cpu->cycles;
	int shift = cpu->gprs[rs];
	if (rs == ARM_PC) {
		shift += 4;
	}
	shift &= 0xFF;
	uint32_t shiftVal = cpu->gprs[rm];
	if (rm == ARM_PC) {
		shiftVal += 4;
	}
	if (!shift) {
		cpu->shifterOperand = shiftVal;
		cpu->shifterCarryOut = cpu->cpsr.c;
	} else if (shift < 32) {
		cpu->shifterOperand = shiftVal >> shift;
		cpu->shifterCarryOut = (shiftVal >> (shift - 1)) & 1;
	} else if (shift == 32) {
		cpu->shifterOperand = 0;
		cpu->shifterCarryOut = shiftVal >> 31;
	} else {
		cpu->shifterOperand = 0;
		cpu->shifterCarryOut = 0;
	}
}

static inline void _shiftASR(struct ARMCore* cpu, uint32_t opcode) {
	int rm = opcode & 0x0000000F;
	int immediate = (opcode & 0x00000F80) >> 7;
	if (immediate) {
		cpu->shifterOperand = cpu->gprs[rm] >> immediate;
		cpu->shifterCarryOut = (cpu->gprs[rm] >> (immediate - 1)) & 1;
	} else {
		cpu->shifterCarryOut = ARM_SIGN(cpu->gprs[rm]);
		cpu->shifterOperand = cpu->shifterCarryOut;
	}
}

static inline void _shiftASRR(struct ARMCore* cpu, uint32_t opcode) {
	int rm = opcode & 0x0000000F;
	int rs = (opcode >> 8) & 0x0000000F;
	++cpu->cycles;
	int shift = cpu->gprs[rs];
	if (rs == ARM_PC) {
		shift += 4;
	}
	shift &= 0xFF;
	int shiftVal =  cpu->gprs[rm];
	if (rm == ARM_PC) {
		shiftVal += 4;
	}
	if (!shift) {
		cpu->shifterOperand = shiftVal;
		cpu->shifterCarryOut = cpu->cpsr.c;
	} else if (shift < 32) {
		cpu->shifterOperand = shiftVal >> shift;
		cpu->shifterCarryOut = (shiftVal >> (shift - 1)) & 1;
	} else if (cpu->gprs[rm] >> 31) {
		cpu->shifterOperand = 0xFFFFFFFF;
		cpu->shifterCarryOut = 1;
	} else {
		cpu->shifterOperand = 0;
		cpu->shifterCarryOut = 0;
	}
}

static inline void _shiftROR(struct ARMCore* cpu, uint32_t opcode) {
	int rm = opcode & 0x0000000F;
	int immediate = (opcode & 0x00000F80) >> 7;
	if (immediate) {
		cpu->shifterOperand = ROR(cpu->gprs[rm], immediate);
		cpu->shifterCarryOut = (cpu->gprs[rm] >> (immediate - 1)) & 1;
	} else {
		// RRX
		cpu->shifterOperand = (cpu->cpsr.c << 31) | (((uint32_t) cpu->gprs[rm]) >> 1);
		cpu->shifterCarryOut = cpu->gprs[rm] & 0x00000001;
	}
}

static inline void _shiftRORR(struct ARMCore* cpu, uint32_t opcode) {
	int rm = opcode & 0x0000000F;
	int rs = (opcode >> 8) & 0x0000000F;
	++cpu->cycles;
	int shift = cpu->gprs[rs];
	if (rs == ARM_PC) {
		shift += 4;
	}
	shift &= 0xFF;
	int shiftVal =  cpu->gprs[rm];
	if (rm == ARM_PC) {
		shiftVal += 4;
	}
	int rotate = shift & 0x1F;
	if (!shift) {
		cpu->shifterOperand = shiftVal;
		cpu->shifterCarryOut = cpu->cpsr.c;
	} else if (rotate) {
		cpu->shifterOperand = ROR(shiftVal, rotate);
		cpu->shifterCarryOut = (shiftVal >> (rotate - 1)) & 1;
	} else {
		cpu->shifterOperand = shiftVal;
		cpu->shifterCarryOut = ARM_SIGN(shiftVal);
	}
}

static inline void _immediate(struct ARMCore* cpu, uint32_t opcode) {
	int rotate = (opcode & 0x00000F00) >> 7;
	int immediate = opcode & 0x000000FF;
	if (!rotate) {
		cpu->shifterOperand = immediate;
		cpu->shifterCarryOut = cpu->cpsr.c;
	} else {
		cpu->shifterOperand = ROR(immediate, rotate);
		cpu->shifterCarryOut = ARM_SIGN(cpu->shifterOperand);
	}
}

// Instruction definitions
// Beware pre-processor antics

#define NO_EXTEND64(V) (uint64_t)(uint32_t) (V)

#define ARM_ADDITION_S(M, N, D) \
	if (rd == ARM_PC && _ARMModeHasSPSR(cpu->cpsr.priv)) { \
		cpu->cpsr = cpu->spsr; \
		_ARMReadCPSR(cpu); \
	} else { \
		cpu->cpsr.n = ARM_SIGN(D); \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = ARM_CARRY_FROM(M, N, D); \
		cpu->cpsr.v = ARM_V_ADDITION(M, N, D); \
	}

#define ARM_SUBTRACTION_S(M, N, D) \
	if (rd == ARM_PC && _ARMModeHasSPSR(cpu->cpsr.priv)) { \
		cpu->cpsr = cpu->spsr; \
		_ARMReadCPSR(cpu); \
	} else { \
		cpu->cpsr.n = ARM_SIGN(D); \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = ARM_BORROW_FROM(M, N, D); \
		cpu->cpsr.v = ARM_V_SUBTRACTION(M, N, D); \
	}

#define ARM_NEUTRAL_S(M, N, D) \
	if (rd == ARM_PC && _ARMModeHasSPSR(cpu->cpsr.priv)) { \
		cpu->cpsr = cpu->spsr; \
		_ARMReadCPSR(cpu); \
	} else { \
		cpu->cpsr.n = ARM_SIGN(D); \
		cpu->cpsr.z = !(D); \
		cpu->cpsr.c = cpu->shifterCarryOut; \
	}

#define ARM_NEUTRAL_HI_S(DLO, DHI) \
	cpu->cpsr.n = ARM_SIGN(DHI); \
	cpu->cpsr.z = !((DHI) | (DLO));

#define ADDR_MODE_2_I_TEST (opcode & 0x00000F80)
#define ADDR_MODE_2_I ((opcode & 0x00000F80) >> 7)
#define ADDR_MODE_2_ADDRESS (address)
#define ADDR_MODE_2_RN (cpu->gprs[rn])
#define ADDR_MODE_2_RM (cpu->gprs[rm])
#define ADDR_MODE_2_IMMEDIATE (opcode & 0x00000FFF)
#define ADDR_MODE_2_INDEX(U_OP, M) (cpu->gprs[rn] U_OP M)
#define ADDR_MODE_2_WRITEBACK(ADDR) \
	cpu->gprs[rn] = ADDR; \
	if (UNLIKELY(rn == ARM_PC)) { \
		ARM_WRITE_PC; \
	}

#define ADDR_MODE_2_LSL (cpu->gprs[rm] << ADDR_MODE_2_I)
#define ADDR_MODE_2_LSR (ADDR_MODE_2_I_TEST ? ((uint32_t) cpu->gprs[rm]) >> ADDR_MODE_2_I : 0)
#define ADDR_MODE_2_ASR (ADDR_MODE_2_I_TEST ? ((int32_t) cpu->gprs[rm]) >> ADDR_MODE_2_I : ((int32_t) cpu->gprs[rm]) >> 31)
#define ADDR_MODE_2_ROR (ADDR_MODE_2_I_TEST ? ROR(cpu->gprs[rm], ADDR_MODE_2_I) : (cpu->cpsr.c << 31) | (((uint32_t) cpu->gprs[rm]) >> 1))

#define ADDR_MODE_3_ADDRESS ADDR_MODE_2_ADDRESS
#define ADDR_MODE_3_RN ADDR_MODE_2_RN
#define ADDR_MODE_3_RM ADDR_MODE_2_RM
#define ADDR_MODE_3_IMMEDIATE (((opcode & 0x00000F00) >> 4) | (opcode & 0x0000000F))
#define ADDR_MODE_3_INDEX(U_OP, M) ADDR_MODE_2_INDEX(U_OP, M)
#define ADDR_MODE_3_WRITEBACK(ADDR) ADDR_MODE_2_WRITEBACK(ADDR)

#define ADDR_MODE_4_WRITEBACK_LDM \
		if (!((1 << rn) & rs)) { \
			cpu->gprs[rn] = address; \
		}

#define ADDR_MODE_4_WRITEBACK_STM cpu->gprs[rn] = address;

#define ARM_LOAD_POST_BODY \
	currentCycles += cpu->memory.activeNonseqCycles32 - cpu->memory.activeSeqCycles32; \
	if (rd == ARM_PC) { \
		ARM_WRITE_PC; \
	}

#define ARM_STORE_POST_BODY \
	currentCycles += cpu->memory.activeNonseqCycles32 - cpu->memory.activeSeqCycles32;

#define DEFINE_INSTRUCTION_ARM(NAME, BODY) \
	static void _ARMInstruction ## NAME (struct ARMCore* cpu, uint32_t opcode) { \
		int currentCycles = ARM_PREFETCH_CYCLES; \
		BODY; \
		cpu->cycles += currentCycles; \
	}

#define DEFINE_ALU_INSTRUCTION_EX_ARM(NAME, S_BODY, SHIFTER, BODY) \
	DEFINE_INSTRUCTION_ARM(NAME, \
		int rd = (opcode >> 12) & 0xF; \
		int rn = (opcode >> 16) & 0xF; \
		UNUSED(rn); \
		SHIFTER(cpu, opcode); \
		BODY; \
		S_BODY; \
		if (rd == ARM_PC) { \
			if (cpu->executionMode == MODE_ARM) { \
				ARM_WRITE_PC; \
			} else { \
				THUMB_WRITE_PC; \
			} \
		})

#define DEFINE_ALU_INSTRUCTION_ARM(NAME, S_BODY, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _LSL, , _shiftLSL, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S_LSL, S_BODY, _shiftLSL, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _LSLR, , _shiftLSLR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S_LSLR, S_BODY, _shiftLSLR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _LSR, , _shiftLSR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S_LSR, S_BODY, _shiftLSR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _LSRR, , _shiftLSRR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S_LSRR, S_BODY, _shiftLSRR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _ASR, , _shiftASR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S_ASR, S_BODY, _shiftASR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _ASRR, , _shiftASRR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S_ASRR, S_BODY, _shiftASRR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _ROR, , _shiftROR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S_ROR, S_BODY, _shiftROR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _RORR, , _shiftRORR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## S_RORR, S_BODY, _shiftRORR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## I, , _immediate, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## SI, S_BODY, _immediate, BODY)

#define DEFINE_ALU_INSTRUCTION_S_ONLY_ARM(NAME, S_BODY, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _LSL, S_BODY, _shiftLSL, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _LSLR, S_BODY, _shiftLSLR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _LSR, S_BODY, _shiftLSR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _LSRR, S_BODY, _shiftLSRR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _ASR, S_BODY, _shiftASR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _ASRR, S_BODY, _shiftASRR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _ROR, S_BODY, _shiftROR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## _RORR, S_BODY, _shiftRORR, BODY) \
	DEFINE_ALU_INSTRUCTION_EX_ARM(NAME ## I, S_BODY, _immediate, BODY)

#define DEFINE_MULTIPLY_INSTRUCTION_EX_ARM(NAME, BODY, S_BODY) \
	DEFINE_INSTRUCTION_ARM(NAME, \
		int rd = (opcode >> 12) & 0xF; \
		int rdHi = (opcode >> 16) & 0xF; \
		int rs = (opcode >> 8) & 0xF; \
		int rm = opcode & 0xF; \
		if (rdHi == ARM_PC || rd == ARM_PC) { \
			return; \
		} \
		ARM_WAIT_MUL(cpu->gprs[rs]); \
		BODY; \
		S_BODY; \
		currentCycles += cpu->memory.activeNonseqCycles32 - cpu->memory.activeSeqCycles32)

#define DEFINE_MULTIPLY_INSTRUCTION_ARM(NAME, BODY, S_BODY) \
	DEFINE_MULTIPLY_INSTRUCTION_EX_ARM(NAME, BODY, ) \
	DEFINE_MULTIPLY_INSTRUCTION_EX_ARM(NAME ## S, BODY, S_BODY)

#define DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME, ADDRESS, WRITEBACK, BODY) \
	DEFINE_INSTRUCTION_ARM(NAME, \
		uint32_t address; \
		int rn = (opcode >> 16) & 0xF; \
		int rd = (opcode >> 12) & 0xF; \
		int rm = opcode & 0xF; \
		UNUSED(rm); \
		address = ADDRESS; \
		WRITEBACK; \
		BODY;)

#define DEFINE_LOAD_STORE_INSTRUCTION_SHIFTER_ARM(NAME, SHIFTER, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME, ADDR_MODE_2_RN, ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_INDEX(-, SHIFTER)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## U, ADDR_MODE_2_RN, ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_INDEX(+, SHIFTER)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## P, ADDR_MODE_2_INDEX(-, SHIFTER), , BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## PW, ADDR_MODE_2_INDEX(-, SHIFTER), ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_ADDRESS), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## PU, ADDR_MODE_2_INDEX(+, SHIFTER), , BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## PUW, ADDR_MODE_2_INDEX(+, SHIFTER), ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_ADDRESS), BODY)

#define DEFINE_LOAD_STORE_INSTRUCTION_ARM(NAME, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_SHIFTER_ARM(NAME ## _LSL_, ADDR_MODE_2_LSL, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_SHIFTER_ARM(NAME ## _LSR_, ADDR_MODE_2_LSR, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_SHIFTER_ARM(NAME ## _ASR_, ADDR_MODE_2_ASR, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_SHIFTER_ARM(NAME ## _ROR_, ADDR_MODE_2_ROR, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## I, ADDR_MODE_2_RN, ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_INDEX(-, ADDR_MODE_2_IMMEDIATE)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IU, ADDR_MODE_2_RN, ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_INDEX(+, ADDR_MODE_2_IMMEDIATE)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IP, ADDR_MODE_2_INDEX(-, ADDR_MODE_2_IMMEDIATE), , BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IPW, ADDR_MODE_2_INDEX(-, ADDR_MODE_2_IMMEDIATE), ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_ADDRESS), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IPU, ADDR_MODE_2_INDEX(+, ADDR_MODE_2_IMMEDIATE), , BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IPUW, ADDR_MODE_2_INDEX(+, ADDR_MODE_2_IMMEDIATE), ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_ADDRESS), BODY) \

#define DEFINE_LOAD_STORE_MODE_3_INSTRUCTION_ARM(NAME, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME, ADDR_MODE_3_RN, ADDR_MODE_3_WRITEBACK(ADDR_MODE_3_INDEX(-, ADDR_MODE_3_RM)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## U, ADDR_MODE_3_RN, ADDR_MODE_3_WRITEBACK(ADDR_MODE_3_INDEX(+, ADDR_MODE_3_RM)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## P, ADDR_MODE_3_INDEX(-, ADDR_MODE_3_RM), , BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## PW, ADDR_MODE_3_INDEX(-, ADDR_MODE_3_RM), ADDR_MODE_3_WRITEBACK(ADDR_MODE_3_ADDRESS), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## PU, ADDR_MODE_3_INDEX(+, ADDR_MODE_3_RM), , BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## PUW, ADDR_MODE_3_INDEX(+, ADDR_MODE_3_RM), ADDR_MODE_3_WRITEBACK(ADDR_MODE_3_ADDRESS), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## I, ADDR_MODE_3_RN, ADDR_MODE_3_WRITEBACK(ADDR_MODE_3_INDEX(-, ADDR_MODE_3_IMMEDIATE)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IU, ADDR_MODE_3_RN, ADDR_MODE_3_WRITEBACK(ADDR_MODE_3_INDEX(+, ADDR_MODE_3_IMMEDIATE)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IP, ADDR_MODE_3_INDEX(-, ADDR_MODE_3_IMMEDIATE), , BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IPW, ADDR_MODE_3_INDEX(-, ADDR_MODE_3_IMMEDIATE), ADDR_MODE_3_WRITEBACK(ADDR_MODE_3_ADDRESS), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IPU, ADDR_MODE_3_INDEX(+, ADDR_MODE_3_IMMEDIATE), , BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IPUW, ADDR_MODE_3_INDEX(+, ADDR_MODE_3_IMMEDIATE), ADDR_MODE_3_WRITEBACK(ADDR_MODE_3_ADDRESS), BODY) \

#define DEFINE_LOAD_STORE_T_INSTRUCTION_SHIFTER_ARM(NAME, SHIFTER, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME, SHIFTER, ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_INDEX(-, ADDR_MODE_2_RM)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## U, SHIFTER, ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_INDEX(+, ADDR_MODE_2_RM)), BODY) \

#define DEFINE_LOAD_STORE_T_INSTRUCTION_ARM(NAME, BODY) \
	DEFINE_LOAD_STORE_T_INSTRUCTION_SHIFTER_ARM(NAME ## _LSL_, ADDR_MODE_2_LSL, BODY) \
	DEFINE_LOAD_STORE_T_INSTRUCTION_SHIFTER_ARM(NAME ## _LSR_, ADDR_MODE_2_LSR, BODY) \
	DEFINE_LOAD_STORE_T_INSTRUCTION_SHIFTER_ARM(NAME ## _ASR_, ADDR_MODE_2_ASR, BODY) \
	DEFINE_LOAD_STORE_T_INSTRUCTION_SHIFTER_ARM(NAME ## _ROR_, ADDR_MODE_2_ROR, BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## I, ADDR_MODE_2_RN, ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_INDEX(-, ADDR_MODE_2_IMMEDIATE)), BODY) \
	DEFINE_LOAD_STORE_INSTRUCTION_EX_ARM(NAME ## IU, ADDR_MODE_2_RN, ADDR_MODE_2_WRITEBACK(ADDR_MODE_2_INDEX(+, ADDR_MODE_2_IMMEDIATE)), BODY) \

#define ARM_MS_PRE \
	enum PrivilegeMode privilegeMode = cpu->privilegeMode; \
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);

#define ARM_MS_POST ARMSetPrivilegeMode(cpu, privilegeMode);

#define DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME, LS, WRITEBACK, S_PRE, S_POST, DIRECTION, POST_BODY) \
	DEFINE_INSTRUCTION_ARM(NAME, \
		int rn = (opcode >> 16) & 0xF; \
		int rs = opcode & 0x0000FFFF; \
		uint32_t address = cpu->gprs[rn]; \
		S_PRE; \
		address = cpu->memory. LS ## Multiple(cpu, address, rs, LSM_ ## DIRECTION, &currentCycles); \
		S_POST; \
		POST_BODY; \
		WRITEBACK;)


#define DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_ARM(NAME, LS, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## DA,   LS,                               ,           ,            , DA, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## DAW,  LS, ADDR_MODE_4_WRITEBACK_ ## NAME,           ,            , DA, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## DB,   LS,                               ,           ,            , DB, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## DBW,  LS, ADDR_MODE_4_WRITEBACK_ ## NAME,           ,            , DB, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## IA,   LS,                               ,           ,            , IA, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## IAW,  LS, ADDR_MODE_4_WRITEBACK_ ## NAME,           ,            , IA, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## IB,   LS,                               ,           ,            , IB, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## IBW,  LS, ADDR_MODE_4_WRITEBACK_ ## NAME,           ,            , IB, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## SDA,  LS,                               , ARM_MS_PRE, ARM_MS_POST, DA, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## SDAW, LS, ADDR_MODE_4_WRITEBACK_ ## NAME, ARM_MS_PRE, ARM_MS_POST, DA, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## SDB,  LS,                               , ARM_MS_PRE, ARM_MS_POST, DB, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## SDBW, LS, ADDR_MODE_4_WRITEBACK_ ## NAME, ARM_MS_PRE, ARM_MS_POST, DB, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## SIA,  LS,                               , ARM_MS_PRE, ARM_MS_POST, IA, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## SIAW, LS, ADDR_MODE_4_WRITEBACK_ ## NAME, ARM_MS_PRE, ARM_MS_POST, IA, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## SIB,  LS,                               , ARM_MS_PRE, ARM_MS_POST, IB, POST_BODY) \
	DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_EX_ARM(NAME ## SIBW, LS, ADDR_MODE_4_WRITEBACK_ ## NAME, ARM_MS_PRE, ARM_MS_POST, IB, POST_BODY)

// Begin ALU definitions

DEFINE_ALU_INSTRUCTION_ARM(ADD, ARM_ADDITION_S(n, cpu->shifterOperand, cpu->gprs[rd]),
	int32_t n = cpu->gprs[rn];
	cpu->gprs[rd] = n + cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_ARM(ADC, ARM_ADDITION_S(n, cpu->shifterOperand, cpu->gprs[rd]),
	int32_t n = cpu->gprs[rn];
	cpu->gprs[rd] = n + cpu->shifterOperand + cpu->cpsr.c;)

DEFINE_ALU_INSTRUCTION_ARM(AND, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]),
	cpu->gprs[rd] = cpu->gprs[rn] & cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_ARM(BIC, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]),
	cpu->gprs[rd] = cpu->gprs[rn] & ~cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_S_ONLY_ARM(CMN, ARM_ADDITION_S(cpu->gprs[rn], cpu->shifterOperand, aluOut),
	int32_t aluOut = cpu->gprs[rn] + cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_S_ONLY_ARM(CMP, ARM_SUBTRACTION_S(cpu->gprs[rn], cpu->shifterOperand, aluOut),
	int32_t aluOut = cpu->gprs[rn] - cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_ARM(EOR, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]),
	cpu->gprs[rd] = cpu->gprs[rn] ^ cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_ARM(MOV, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]),
	cpu->gprs[rd] = cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_ARM(MVN, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]),
	cpu->gprs[rd] = ~cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_ARM(ORR, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, cpu->gprs[rd]),
	cpu->gprs[rd] = cpu->gprs[rn] | cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_ARM(RSB, ARM_SUBTRACTION_S(cpu->shifterOperand, n, cpu->gprs[rd]),
	int32_t n = cpu->gprs[rn];
	cpu->gprs[rd] = cpu->shifterOperand - n;)

DEFINE_ALU_INSTRUCTION_ARM(RSC, ARM_SUBTRACTION_S(cpu->shifterOperand, n, cpu->gprs[rd]),
	int32_t n = cpu->gprs[rn] + !cpu->cpsr.c;
	cpu->gprs[rd] = cpu->shifterOperand - n;)

DEFINE_ALU_INSTRUCTION_ARM(SBC, ARM_SUBTRACTION_S(n, shifterOperand, cpu->gprs[rd]),
	int32_t n = cpu->gprs[rn];
	int32_t shifterOperand = cpu->shifterOperand + !cpu->cpsr.c;
	cpu->gprs[rd] = n - shifterOperand;)

DEFINE_ALU_INSTRUCTION_ARM(SUB, ARM_SUBTRACTION_S(n, cpu->shifterOperand, cpu->gprs[rd]),
	int32_t n = cpu->gprs[rn];
	cpu->gprs[rd] = n - cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_S_ONLY_ARM(TEQ, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, aluOut),
	int32_t aluOut = cpu->gprs[rn] ^ cpu->shifterOperand;)

DEFINE_ALU_INSTRUCTION_S_ONLY_ARM(TST, ARM_NEUTRAL_S(cpu->gprs[rn], cpu->shifterOperand, aluOut),
	int32_t aluOut = cpu->gprs[rn] & cpu->shifterOperand;)

// End ALU definitions

// Begin multiply definitions

DEFINE_MULTIPLY_INSTRUCTION_ARM(MLA, cpu->gprs[rdHi] = cpu->gprs[rm] * cpu->gprs[rs] + cpu->gprs[rd], ARM_NEUTRAL_S(, , cpu->gprs[rdHi]))
DEFINE_MULTIPLY_INSTRUCTION_ARM(MUL, cpu->gprs[rdHi] = cpu->gprs[rm] * cpu->gprs[rs], ARM_NEUTRAL_S(cpu->gprs[rm], cpu->gprs[rs], cpu->gprs[rdHi]))

DEFINE_MULTIPLY_INSTRUCTION_ARM(SMLAL,
	int64_t d = ((int64_t) cpu->gprs[rm]) * ((int64_t) cpu->gprs[rs]);
	int32_t dm = cpu->gprs[rd];
	int32_t dn = d;
	cpu->gprs[rd] = dm + dn;
	cpu->gprs[rdHi] = cpu->gprs[rdHi] + (d >> 32) + ARM_CARRY_FROM(dm, dn, cpu->gprs[rd]);,
	ARM_NEUTRAL_HI_S(cpu->gprs[rd], cpu->gprs[rdHi]))

DEFINE_MULTIPLY_INSTRUCTION_ARM(SMULL,
	int64_t d = ((int64_t) cpu->gprs[rm]) * ((int64_t) cpu->gprs[rs]);
	cpu->gprs[rd] = d;
	cpu->gprs[rdHi] = d >> 32;,
	ARM_NEUTRAL_HI_S(cpu->gprs[rd], cpu->gprs[rdHi]))

DEFINE_MULTIPLY_INSTRUCTION_ARM(UMLAL,
	uint64_t d = NO_EXTEND64(cpu->gprs[rm]) * NO_EXTEND64(cpu->gprs[rs]);
	int32_t dm = cpu->gprs[rd];
	int32_t dn = d;
	cpu->gprs[rd] = dm + dn;
	cpu->gprs[rdHi] = cpu->gprs[rdHi] + (d >> 32) + ARM_CARRY_FROM(dm, dn, cpu->gprs[rd]);,
	ARM_NEUTRAL_HI_S(cpu->gprs[rd], cpu->gprs[rdHi]))

DEFINE_MULTIPLY_INSTRUCTION_ARM(UMULL,
	uint64_t d = NO_EXTEND64(cpu->gprs[rm]) * NO_EXTEND64(cpu->gprs[rs]);
	cpu->gprs[rd] = d;
	cpu->gprs[rdHi] = d >> 32;,
	ARM_NEUTRAL_HI_S(cpu->gprs[rd], cpu->gprs[rdHi]))

// End multiply definitions

// Begin load/store definitions

DEFINE_LOAD_STORE_INSTRUCTION_ARM(LDR, cpu->gprs[rd] = cpu->memory.load32(cpu, address, &currentCycles); ARM_LOAD_POST_BODY;)
DEFINE_LOAD_STORE_INSTRUCTION_ARM(LDRB, cpu->gprs[rd] = cpu->memory.load8(cpu, address, &currentCycles); ARM_LOAD_POST_BODY;)
DEFINE_LOAD_STORE_MODE_3_INSTRUCTION_ARM(LDRH, cpu->gprs[rd] = cpu->memory.load16(cpu, address, &currentCycles); ARM_LOAD_POST_BODY;)
DEFINE_LOAD_STORE_MODE_3_INSTRUCTION_ARM(LDRSB, cpu->gprs[rd] = ARM_SXT_8(cpu->memory.load8(cpu, address, &currentCycles)); ARM_LOAD_POST_BODY;)
DEFINE_LOAD_STORE_MODE_3_INSTRUCTION_ARM(LDRSH, cpu->gprs[rd] = ARM_SXT_16(cpu->memory.load16(cpu, address, &currentCycles)); ARM_LOAD_POST_BODY;)
DEFINE_LOAD_STORE_INSTRUCTION_ARM(STR, cpu->memory.store32(cpu, address, cpu->gprs[rd], &currentCycles); ARM_STORE_POST_BODY;)
DEFINE_LOAD_STORE_INSTRUCTION_ARM(STRB, cpu->memory.store8(cpu, address, cpu->gprs[rd], &currentCycles); ARM_STORE_POST_BODY;)
DEFINE_LOAD_STORE_MODE_3_INSTRUCTION_ARM(STRH, cpu->memory.store16(cpu, address, cpu->gprs[rd], &currentCycles); ARM_STORE_POST_BODY;)

DEFINE_LOAD_STORE_T_INSTRUCTION_ARM(LDRBT,
	enum PrivilegeMode priv = cpu->privilegeMode;
	ARMSetPrivilegeMode(cpu, MODE_USER);
	cpu->gprs[rd] = cpu->memory.load8(cpu, address, &currentCycles);
	ARMSetPrivilegeMode(cpu, priv);
	ARM_LOAD_POST_BODY;)

DEFINE_LOAD_STORE_T_INSTRUCTION_ARM(LDRT,
	enum PrivilegeMode priv = cpu->privilegeMode;
	ARMSetPrivilegeMode(cpu, MODE_USER);
	cpu->gprs[rd] = cpu->memory.load32(cpu, address, &currentCycles);
	ARMSetPrivilegeMode(cpu, priv);
	ARM_LOAD_POST_BODY;)

DEFINE_LOAD_STORE_T_INSTRUCTION_ARM(STRBT,
	enum PrivilegeMode priv = cpu->privilegeMode;
	ARMSetPrivilegeMode(cpu, MODE_USER);
	cpu->memory.store32(cpu, address, cpu->gprs[rd], &currentCycles);
	ARMSetPrivilegeMode(cpu, priv);
	ARM_STORE_POST_BODY;)

DEFINE_LOAD_STORE_T_INSTRUCTION_ARM(STRT,
	enum PrivilegeMode priv = cpu->privilegeMode;
	ARMSetPrivilegeMode(cpu, MODE_USER);
	cpu->memory.store8(cpu, address, cpu->gprs[rd], &currentCycles);
	ARMSetPrivilegeMode(cpu, priv);
	ARM_STORE_POST_BODY;)

DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_ARM(LDM,
	load,
	currentCycles += cpu->memory.activeNonseqCycles32 - cpu->memory.activeSeqCycles32;
	if (rs & 0x8000) {
		ARM_WRITE_PC;
	})

DEFINE_LOAD_STORE_MULTIPLE_INSTRUCTION_ARM(STM,
	store,
	ARM_STORE_POST_BODY;)

DEFINE_INSTRUCTION_ARM(SWP,
	int rm = opcode & 0xF;
	int rd = (opcode >> 12) & 0xF;
	int rn = (opcode >> 16) & 0xF;
	int32_t d = cpu->memory.load32(cpu, cpu->gprs[rn], &currentCycles);
	cpu->memory.store32(cpu, cpu->gprs[rn], cpu->gprs[rm], &currentCycles);
	cpu->gprs[rd] = d;)

DEFINE_INSTRUCTION_ARM(SWPB,
	int rm = opcode & 0xF;
	int rd = (opcode >> 12) & 0xF;
	int rn = (opcode >> 16) & 0xF;
	int32_t d = cpu->memory.load8(cpu, cpu->gprs[rn], &currentCycles);
	cpu->memory.store8(cpu, cpu->gprs[rn], cpu->gprs[rm], &currentCycles);
	cpu->gprs[rd] = d;)

// End load/store definitions

// Begin branch definitions

DEFINE_INSTRUCTION_ARM(B,
	int32_t offset = opcode << 8;
	offset >>= 6;
	cpu->gprs[ARM_PC] += offset;
	ARM_WRITE_PC;)

DEFINE_INSTRUCTION_ARM(BL,
	int32_t immediate = (opcode & 0x00FFFFFF) << 8;
	cpu->gprs[ARM_LR] = cpu->gprs[ARM_PC] - WORD_SIZE_ARM;
	cpu->gprs[ARM_PC] += immediate >> 6;
	ARM_WRITE_PC;)

DEFINE_INSTRUCTION_ARM(BX,
	int rm = opcode & 0x0000000F;
	_ARMSetMode(cpu, cpu->gprs[rm] & 0x00000001);
	cpu->gprs[ARM_PC] = cpu->gprs[rm] & 0xFFFFFFFE;
	if (cpu->executionMode == MODE_THUMB) {
		THUMB_WRITE_PC;
	} else {
		ARM_WRITE_PC;
	})

// End branch definitions

// Begin coprocessor definitions

DEFINE_INSTRUCTION_ARM(CDP, ARM_STUB)
DEFINE_INSTRUCTION_ARM(LDC, ARM_STUB)
DEFINE_INSTRUCTION_ARM(STC, ARM_STUB)
DEFINE_INSTRUCTION_ARM(MCR, ARM_STUB)
DEFINE_INSTRUCTION_ARM(MRC, ARM_STUB)

// Begin miscellaneous definitions

DEFINE_INSTRUCTION_ARM(BKPT, cpu->irqh.bkpt32(cpu, ((opcode >> 4) & 0xFFF0) | (opcode & 0xF))); // Not strictly in ARMv4T, but here for convenience
DEFINE_INSTRUCTION_ARM(ILL, ARM_ILL) // Illegal opcode

DEFINE_INSTRUCTION_ARM(MSR,
	int c = opcode & 0x00010000;
	int f = opcode & 0x00080000;
	int32_t operand = cpu->gprs[opcode & 0x0000000F];
	int32_t mask = (c ? 0x000000FF : 0) | (f ? 0xFF000000 : 0);
	if (mask & PSR_USER_MASK) {
		cpu->cpsr.packed = (cpu->cpsr.packed & ~PSR_USER_MASK) | (operand & PSR_USER_MASK);
	}
	if (cpu->privilegeMode != MODE_USER && (mask & PSR_PRIV_MASK)) {
		ARMSetPrivilegeMode(cpu, (enum PrivilegeMode) ((operand & 0x0000000F) | 0x00000010));
		cpu->cpsr.packed = (cpu->cpsr.packed & ~PSR_PRIV_MASK) | (operand & PSR_PRIV_MASK);
	}
	_ARMReadCPSR(cpu);)

DEFINE_INSTRUCTION_ARM(MSRR,
	int c = opcode & 0x00010000;
	int f = opcode & 0x00080000;
	int32_t operand = cpu->gprs[opcode & 0x0000000F];
	int32_t mask = (c ? 0x000000FF : 0) | (f ? 0xFF000000 : 0);
	mask &= PSR_USER_MASK | PSR_PRIV_MASK | PSR_STATE_MASK;
	cpu->spsr.packed = (cpu->spsr.packed & ~mask) | (operand & mask);)

DEFINE_INSTRUCTION_ARM(MRS, \
	int rd = (opcode >> 12) & 0xF; \
	cpu->gprs[rd] = cpu->cpsr.packed;)

DEFINE_INSTRUCTION_ARM(MRSR, \
	int rd = (opcode >> 12) & 0xF; \
	cpu->gprs[rd] = cpu->spsr.packed;)

DEFINE_INSTRUCTION_ARM(MSRI,
	int c = opcode & 0x00010000;
	int f = opcode & 0x00080000;
	int rotate = (opcode & 0x00000F00) >> 7;
	int32_t operand = ROR(opcode & 0x000000FF, rotate);
	int32_t mask = (c ? 0x000000FF : 0) | (f ? 0xFF000000 : 0);
	if (mask & PSR_USER_MASK) {
		cpu->cpsr.packed = (cpu->cpsr.packed & ~PSR_USER_MASK) | (operand & PSR_USER_MASK);
	}
	if (cpu->privilegeMode != MODE_USER && (mask & PSR_PRIV_MASK)) {
		ARMSetPrivilegeMode(cpu, (enum PrivilegeMode) ((operand & 0x0000000F) | 0x00000010));
		cpu->cpsr.packed = (cpu->cpsr.packed & ~PSR_PRIV_MASK) | (operand & PSR_PRIV_MASK);
	}
	_ARMReadCPSR(cpu);)

DEFINE_INSTRUCTION_ARM(MSRRI,
	int c = opcode & 0x00010000;
	int f = opcode & 0x00080000;
	int rotate = (opcode & 0x00000F00) >> 7;
	int32_t operand = ROR(opcode & 0x000000FF, rotate);
	int32_t mask = (c ? 0x000000FF : 0) | (f ? 0xFF000000 : 0);
	mask &= PSR_USER_MASK | PSR_PRIV_MASK | PSR_STATE_MASK;
	cpu->spsr.packed = (cpu->spsr.packed & ~mask) | (operand & mask);)

DEFINE_INSTRUCTION_ARM(SWI, cpu->irqh.swi32(cpu, opcode & 0xFFFFFF))

const ARMInstruction _armTable[0x1000] = {
	DECLARE_ARM_EMITTER_BLOCK(_ARMInstruction)
};
