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
#define ARM_ROR(I, ROTATE) ((((uint32_t) (I)) >> ROTATE) | ((uint32_t) (I) << ((-ROTATE) & 31)))


#define ARM_CARRY_FROM(M, N, D) (((uint32_t) (M) >> 31) + ((uint32_t) (N) >> 31) > ((uint32_t) (D) >> 31))
#define ARM_BORROW_FROM(M, N, D) (((uint32_t) (M)) >= ((uint32_t) (N)))
#define ARM_V_ADDITION(M, N, D) (!(ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))) && (ARM_SIGN((N) ^ (D))))
#define ARM_V_SUBTRACTION(M, N, D) ((ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))))

#define ARM_WAIT_MUL(R) \
	if ((R & 0xFFFFFF00) == 0xFFFFFF00 || !(R & 0xFFFFFF00)) { \
		currentCycles += 1; \
	} else if ((R & 0xFFFF0000) == 0xFFFF0000 || !(R & 0xFFFF0000)) { \
		currentCycles += 2; \
	} else if ((R & 0xFF000000) == 0xFF000000 || !(R & 0xFF000000)) { \
		currentCycles += 3; \
	} else { \
		currentCycles += 4; \
	}

#define ARM_STUB cpu->irqh.hitStub(cpu, opcode)
#define ARM_ILL cpu->irqh.hitIllegal(cpu, opcode)

#define ARM_WRITE_PC \
	cpu->gprs[ARM_PC] = (cpu->gprs[ARM_PC] & -WORD_SIZE_ARM); \
	cpu->memory.setActiveRegion(cpu, cpu->gprs[ARM_PC]); \
	LOAD_32(cpu->prefetch, cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion); \
	cpu->gprs[ARM_PC] += WORD_SIZE_ARM; \
	currentCycles += 2 + cpu->memory.activeUncachedCycles32 + cpu->memory.activeSeqCycles32;

#define THUMB_WRITE_PC \
	cpu->gprs[ARM_PC] = (cpu->gprs[ARM_PC] & -WORD_SIZE_THUMB); \
	cpu->memory.setActiveRegion(cpu, cpu->gprs[ARM_PC]); \
	LOAD_16(cpu->prefetch, cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion); \
	cpu->gprs[ARM_PC] += WORD_SIZE_THUMB; \
	currentCycles += 2 + cpu->memory.activeUncachedCycles16 + cpu->memory.activeSeqCycles16;

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
		break;
	case MODE_THUMB:
		cpu->cpsr.t = 1;
	}
	cpu->nextEvent = 0;
}

static inline void _ARMReadCPSR(struct ARMCore* cpu) {
	_ARMSetMode(cpu, cpu->cpsr.t);
	ARMSetPrivilegeMode(cpu, cpu->cpsr.priv);
	cpu->irqh.readCPSR(cpu);
}

#endif
