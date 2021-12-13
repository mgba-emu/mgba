/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ISA_INLINES_H
#define ISA_INLINES_H

#include "macros.h"

#include "arm.h"

#define ARM_COND_EQ (cpu->cpsr.z)
#define ARM_COND_NE (!cpu->cpsr.z)
#define ARM_COND_CS (cpu->cpsr.c)
#define ARM_COND_CC (!cpu->cpsr.c)
#define ARM_COND_MI (cpu->cpsr.n)
#define ARM_COND_PL (!cpu->cpsr.n)
#define ARM_COND_VS (cpu->cpsr.v)
#define ARM_COND_VC (!cpu->cpsr.v)
#define ARM_COND_HI (cpu->cpsr.c && !cpu->cpsr.z)
#define ARM_COND_LS (!cpu->cpsr.c || cpu->cpsr.z)
#define ARM_COND_GE (!cpu->cpsr.n == !cpu->cpsr.v)
#define ARM_COND_LT (!cpu->cpsr.n != !cpu->cpsr.v)
#define ARM_COND_GT (!cpu->cpsr.z && !cpu->cpsr.n == !cpu->cpsr.v)
#define ARM_COND_LE (cpu->cpsr.z || !cpu->cpsr.n != !cpu->cpsr.v)
#define ARM_COND_AL 1

#define ARM_SIGN(I) ((I) >> 31)
#define ARM_SXT_8(I) (((int8_t) (I) << 24) >> 24)
#define ARM_SXT_16(I) (((int16_t) (I) << 16) >> 16)
#define ARM_UXT_64(I) (uint64_t)(uint32_t) (I)

#define ARM_CARRY_FROM(M, N, D) (((uint32_t) (M) >> 31) + ((uint32_t) (N) >> 31) > ((uint32_t) (D) >> 31))
#define ARM_BORROW_FROM(M, N, D) (((uint32_t) (M)) >= ((uint32_t) (N)))
#define ARM_BORROW_FROM_CARRY(M, N, D, C) (ARM_UXT_64(M) >= (ARM_UXT_64(N)) + (uint64_t) (C))
#define ARM_V_ADDITION(M, N, D) (!(ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))))
#define ARM_V_SUBTRACTION(M, N, D) ((ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))))

#define ARM_WAIT_SMUL(R, WAIT)                                            \
	{                                                                     \
		int32_t wait = WAIT;                                              \
		if ((R & 0xFFFFFF00) == 0xFFFFFF00 || !(R & 0xFFFFFF00)) {        \
			wait += 1;                                                    \
		} else if ((R & 0xFFFF0000) == 0xFFFF0000 || !(R & 0xFFFF0000)) { \
			wait += 2;                                                    \
		} else if ((R & 0xFF000000) == 0xFF000000 || !(R & 0xFF000000)) { \
			wait += 3;                                                    \
		} else {                                                          \
			wait += 4;                                                    \
		}                                                                 \
		currentCycles += cpu->memory.stall(cpu, wait);                    \
	}

#define ARM_WAIT_UMUL(R, WAIT)                                            \
	{                                                                     \
		int32_t wait = WAIT;                                              \
		if (!(R & 0xFFFFFF00)) {        \
			wait += 1;                                                    \
		} else if (!(R & 0xFFFF0000)) { \
			wait += 2;                                                    \
		} else if (!(R & 0xFF000000)) { \
			wait += 3;                                                    \
		} else {                                                          \
			wait += 4;                                                    \
		}                                                                 \
		currentCycles += cpu->memory.stall(cpu, wait);                    \
	}

#define ARM_STUB cpu->irqh.hitStub(cpu, opcode)
#define ARM_ILL cpu->irqh.hitIllegal(cpu, opcode)

static inline int32_t ARMWritePC(struct ARMCore* cpu) {
	uint32_t pc = cpu->gprs[ARM_PC] & -WORD_SIZE_THUMB;
	cpu->memory.setActiveRegion(cpu, pc);
	LOAD_32(cpu->prefetch[0], pc & cpu->memory.activeMask, cpu->memory.activeRegion);
	pc += WORD_SIZE_ARM;
	LOAD_32(cpu->prefetch[1], pc & cpu->memory.activeMask, cpu->memory.activeRegion);
	cpu->gprs[ARM_PC] = pc;
	return 2 + cpu->memory.activeNonseqCycles32 + cpu->memory.activeSeqCycles32;
}

static inline int32_t ThumbWritePC(struct ARMCore* cpu) {
	uint32_t pc = cpu->gprs[ARM_PC] & -WORD_SIZE_THUMB;
	cpu->memory.setActiveRegion(cpu, pc);
	LOAD_16(cpu->prefetch[0], pc & cpu->memory.activeMask, cpu->memory.activeRegion);
	pc += WORD_SIZE_THUMB;
	LOAD_16(cpu->prefetch[1], pc & cpu->memory.activeMask, cpu->memory.activeRegion);
	cpu->gprs[ARM_PC] = pc;
	return 2 + cpu->memory.activeNonseqCycles16 + cpu->memory.activeSeqCycles16;
}

static inline int _ARMModeHasSPSR(enum PrivilegeMode mode) {
	return mode != MODE_SYSTEM && mode != MODE_USER;
}

static inline void _ARMSetMode(struct ARMCore* cpu, enum ExecutionMode executionMode) {
	if (executionMode == cpu->executionMode) {
		return;
	}

	cpu->executionMode = executionMode;
	switch (executionMode) {
	case MODE_ARM:
		cpu->cpsr.t = 0;
		cpu->memory.activeMask &= ~2;
		break;
	case MODE_THUMB:
		cpu->cpsr.t = 1;
		cpu->memory.activeMask |= 2;
	}
	cpu->nextEvent = cpu->cycles;
}

static inline void _ARMReadCPSR(struct ARMCore* cpu) {
	_ARMSetMode(cpu, cpu->cpsr.t);
	ARMSetPrivilegeMode(cpu, cpu->cpsr.priv);
	cpu->irqh.readCPSR(cpu);
}

static inline uint32_t _ARMInstructionLength(struct ARMCore* cpu) {
	return cpu->cpsr.t == MODE_ARM ? WORD_SIZE_ARM : WORD_SIZE_THUMB;
}

static inline uint32_t _ARMPCAddress(struct ARMCore* cpu) {
	return cpu->gprs[ARM_PC] - _ARMInstructionLength(cpu) * 2;
}

static inline bool ARMTestCondition(struct ARMCore* cpu, unsigned condition) {
	switch (condition) {
		case 0x0:
			return ARM_COND_EQ;
		case 0x1:
			return ARM_COND_NE;
		case 0x2:
			return ARM_COND_CS;
		case 0x3:
			return ARM_COND_CC;
		case 0x4:
			return ARM_COND_MI;
		case 0x5:
			return ARM_COND_PL;
		case 0x6:
			return ARM_COND_VS;
		case 0x7:
			return ARM_COND_VC;
		case 0x8:
			return ARM_COND_HI;
		case 0x9:
			return ARM_COND_LS;
		case 0xA:
			return ARM_COND_GE;
		case 0xB:
			return ARM_COND_LT;
		case 0xC:
			return ARM_COND_GT;
		case 0xD:
			return ARM_COND_LE;
		default:
			return true;
	}
}

static inline enum RegisterBank ARMSelectBank(enum PrivilegeMode mode) {
	switch (mode) {
	case MODE_USER:
	case MODE_SYSTEM:
		// No banked registers
		return BANK_NONE;
	case MODE_FIQ:
		return BANK_FIQ;
	case MODE_IRQ:
		return BANK_IRQ;
	case MODE_SUPERVISOR:
		return BANK_SUPERVISOR;
	case MODE_ABORT:
		return BANK_ABORT;
	case MODE_UNDEFINED:
		return BANK_UNDEFINED;
	default:
		// This should be unreached
		return BANK_NONE;
	}
}

#endif
