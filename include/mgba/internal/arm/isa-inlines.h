/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ISA_INLINES_H
#define ISA_INLINES_H

#include "macros.h"

#include "arm.h"

#define ARM_COND_EQ (ARMPSRIsZ(cpu->cpsr))
#define ARM_COND_NE (!ARMPSRIsZ(cpu->cpsr))
#define ARM_COND_CS (ARMPSRIsC(cpu->cpsr))
#define ARM_COND_CC (!ARMPSRIsC(cpu->cpsr))
#define ARM_COND_MI (ARMPSRIsN(cpu->cpsr))
#define ARM_COND_PL (!ARMPSRIsN(cpu->cpsr))
#define ARM_COND_VS (ARMPSRIsV(cpu->cpsr))
#define ARM_COND_VC (!ARMPSRIsV(cpu->cpsr))
#define ARM_COND_HI (ARMPSRIsC(cpu->cpsr) && !ARMPSRIsZ(cpu->cpsr))
#define ARM_COND_LS (!ARMPSRIsC(cpu->cpsr) || ARMPSRIsZ(cpu->cpsr))
#define ARM_COND_GE (!ARMPSRIsN(cpu->cpsr) == !ARMPSRIsV(cpu->cpsr))
#define ARM_COND_LT (!ARMPSRIsN(cpu->cpsr) != !ARMPSRIsV(cpu->cpsr))
#define ARM_COND_GT (!ARMPSRIsZ(cpu->cpsr) && !ARMPSRIsN(cpu->cpsr) == !ARMPSRIsV(cpu->cpsr))
#define ARM_COND_LE (ARMPSRIsZ(cpu->cpsr) || !ARMPSRIsN(cpu->cpsr) != !ARMPSRIsV(cpu->cpsr))
#define ARM_COND_AL 1

#define ARM_SIGN(I) ((I) >> 31)
#define ARM_SXT_8(I) (((int8_t) (I) << 24) >> 24)
#define ARM_SXT_16(I) (((int16_t) (I) << 16) >> 16)
#define ARM_UXT_64(I) (uint64_t)(uint32_t) (I)

#define ARM_CARRY_FROM(M, N, D) (((uint32_t) (M) >> 31) + ((uint32_t) (N) >> 31) > ((uint32_t) (D) >> 31))
#define ARM_BORROW_FROM(M, N, D) (((uint32_t) (M)) >= ((uint32_t) (N)))
#define ARM_BORROW_FROM_CARRY(M, N, D, C) (ARM_UXT_64(M) >= (ARM_UXT_64(N)) + (uint64_t) (C))
#define ARM_V_ADDITION(M, N, D) (!(ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))) && (ARM_SIGN((N) ^ (D))))
#define ARM_V_SUBTRACTION(M, N, D) ((ARM_SIGN((M) ^ (N))) && (ARM_SIGN((M) ^ (D))))

#define ARM_WAIT_MUL(R)                                                   \
	{                                                                     \
		int32_t wait;                                                     \
		if ((R & 0xFFFFFF00) == 0xFFFFFF00 || !(R & 0xFFFFFF00)) {        \
			wait = 1;                                                     \
		} else if ((R & 0xFFFF0000) == 0xFFFF0000 || !(R & 0xFFFF0000)) { \
			wait = 2;                                                     \
		} else if ((R & 0xFF000000) == 0xFF000000 || !(R & 0xFF000000)) { \
			wait = 3;                                                     \
		} else {                                                          \
			wait = 4;                                                     \
		}                                                                 \
		currentCycles += cpu->memory.stall(cpu, wait);                    \
	}

#define ARM_STUB cpu->irqh.hitStub(cpu, opcode)
#define ARM_ILL cpu->irqh.hitIllegal(cpu, opcode)

#define ARM_WRITE_PC                                                                                 \
	cpu->gprs[ARM_PC] = (cpu->gprs[ARM_PC] & -WORD_SIZE_ARM);                                        \
	cpu->memory.setActiveRegion(cpu, cpu->gprs[ARM_PC]);                                             \
	LOAD_32(cpu->prefetch[0], cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion); \
	cpu->gprs[ARM_PC] += WORD_SIZE_ARM;                                                              \
	LOAD_32(cpu->prefetch[1], cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion); \
	currentCycles += 2 + cpu->memory.activeNonseqCycles32 + cpu->memory.activeSeqCycles32;

#define THUMB_WRITE_PC                                                                               \
	cpu->gprs[ARM_PC] = (cpu->gprs[ARM_PC] & -WORD_SIZE_THUMB);                                      \
	cpu->memory.setActiveRegion(cpu, cpu->gprs[ARM_PC]);                                             \
	LOAD_16(cpu->prefetch[0], cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion); \
	cpu->gprs[ARM_PC] += WORD_SIZE_THUMB;                                                            \
	LOAD_16(cpu->prefetch[1], cpu->gprs[ARM_PC] & cpu->memory.activeMask, cpu->memory.activeRegion); \
	currentCycles += 2 + cpu->memory.activeNonseqCycles16 + cpu->memory.activeSeqCycles16;

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
		cpu->cpsr = ARMPSRClearT(cpu->cpsr);
		break;
	case MODE_THUMB:
		cpu->cpsr = ARMPSRFillT(cpu->cpsr);
	}
	cpu->nextEvent = cpu->cycles;
}

static inline void _ARMReadCPSR(struct ARMCore* cpu) {
	_ARMSetMode(cpu, ARMPSRGetT(cpu->cpsr));
	ARMSetPrivilegeMode(cpu, ARMPSRGetPriv(cpu->cpsr));
	cpu->irqh.readCPSR(cpu);
}

static inline uint32_t _ARMPCAddress(struct ARMCore* cpu) {
	int instructionLength;
	if (ARMPSRIsT(cpu->cpsr)) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	return cpu->gprs[ARM_PC] - instructionLength * 2;
}

#endif
