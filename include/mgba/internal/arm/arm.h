/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_H
#define ARM_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/cpu.h>

enum {
	ARM_SP = 13,
	ARM_LR = 14,
	ARM_PC = 15
};

enum ExecutionMode {
	MODE_ARM = 0,
	MODE_THUMB = 1
};

enum PrivilegeMode {
	MODE_USER = 0x10,
	MODE_FIQ = 0x11,
	MODE_IRQ = 0x12,
	MODE_SUPERVISOR = 0x13,
	MODE_ABORT = 0x17,
	MODE_UNDEFINED = 0x1B,
	MODE_SYSTEM = 0x1F
};

enum WordSize {
	WORD_SIZE_ARM = 4,
	WORD_SIZE_THUMB = 2
};

enum ExecutionVector {
	BASE_RESET = 0x00000000,
	BASE_UNDEF = 0x00000004,
	BASE_SWI = 0x00000008,
	BASE_PABT = 0x0000000C,
	BASE_DABT = 0x00000010,
	BASE_IRQ = 0x00000018,
	BASE_FIQ = 0x0000001C
};

enum RegisterBank {
	BANK_NONE = 0,
	BANK_FIQ = 1,
	BANK_IRQ = 2,
	BANK_SUPERVISOR = 3,
	BANK_ABORT = 4,
	BANK_UNDEFINED = 5
};

enum LSMDirection {
	LSM_B = 1,
	LSM_D = 2,
	LSM_IA = 0,
	LSM_IB = 1,
	LSM_DA = 2,
	LSM_DB = 3
};

struct ARMCore;

DECL_BITFIELD(ARMPSR, uint32_t);
DECL_BITS(ARMPSR, Priv, 0, 5);
DECL_BIT(ARMPSR, T, 5);
DECL_BIT(ARMPSR, F, 6);
DECL_BIT(ARMPSR, I, 7);
DECL_BIT(ARMPSR, V, 28);
DECL_BIT(ARMPSR, C, 29);
DECL_BIT(ARMPSR, Z, 30);
DECL_BIT(ARMPSR, N, 31);

struct ARMMemory {
	uint32_t (*load32)(struct ARMCore*, uint32_t address, int* cycleCounter);
	uint32_t (*load16)(struct ARMCore*, uint32_t address, int* cycleCounter);
	uint32_t (*load8)(struct ARMCore*, uint32_t address, int* cycleCounter);

	void (*store32)(struct ARMCore*, uint32_t address, int32_t value, int* cycleCounter);
	void (*store16)(struct ARMCore*, uint32_t address, int16_t value, int* cycleCounter);
	void (*store8)(struct ARMCore*, uint32_t address, int8_t value, int* cycleCounter);

	uint32_t (*loadMultiple)(struct ARMCore*, uint32_t baseAddress, int mask, enum LSMDirection direction,
	                         int* cycleCounter);
	uint32_t (*storeMultiple)(struct ARMCore*, uint32_t baseAddress, int mask, enum LSMDirection direction,
	                          int* cycleCounter);

	uint32_t* activeRegion;
	uint32_t activeMask;
	uint32_t activeSeqCycles32;
	uint32_t activeSeqCycles16;
	uint32_t activeNonseqCycles32;
	uint32_t activeNonseqCycles16;
	int32_t (*stall)(struct ARMCore*, int32_t wait);
	void (*setActiveRegion)(struct ARMCore*, uint32_t address);
};

struct ARMInterruptHandler {
	void (*reset)(struct ARMCore* cpu);
	void (*processEvents)(struct ARMCore* cpu);
	void (*swi16)(struct ARMCore* cpu, int immediate);
	void (*swi32)(struct ARMCore* cpu, int immediate);
	void (*hitIllegal)(struct ARMCore* cpu, uint32_t opcode);
	void (*bkpt16)(struct ARMCore* cpu, int immediate);
	void (*bkpt32)(struct ARMCore* cpu, int immediate);
	void (*readCPSR)(struct ARMCore* cpu);
	void (*writeCP15)(struct ARMCore*, int crn, int crm, int opcode1, int opcode2, uint32_t value);
	uint32_t (*readCP15)(struct ARMCore*, int crn, int crm, int opcode1, int opcode2);

	void (*hitStub)(struct ARMCore* cpu, uint32_t opcode);
};

DECL_BITFIELD(ARMCPUID, uint32_t);
DECL_BITFIELD(ARMCacheType, uint32_t);
DECL_BITFIELD(ARMTCMType, uint32_t);
DECL_BITFIELD(ARMTLBType, uint32_t);
DECL_BITFIELD(ARMMPUType, uint32_t);

DECL_BITFIELD(ARMControlReg, uint32_t);
DECL_BIT(ARMControlReg, M, 0);
DECL_BIT(ARMControlReg, A, 1);
DECL_BIT(ARMControlReg, C, 2);
DECL_BIT(ARMControlReg, W, 3);
DECL_BIT(ARMControlReg, P, 4);
DECL_BIT(ARMControlReg, D, 5);
DECL_BIT(ARMControlReg, L, 6);
DECL_BIT(ARMControlReg, B, 7);
DECL_BIT(ARMControlReg, S, 8);
DECL_BIT(ARMControlReg, R, 9);
DECL_BIT(ARMControlReg, F, 10);
DECL_BIT(ARMControlReg, Z, 11);
DECL_BIT(ARMControlReg, I, 12);
DECL_BIT(ARMControlReg, V, 13);
DECL_BIT(ARMControlReg, RR, 14);
DECL_BIT(ARMControlReg, L4, 15);
DECL_BIT(ARMControlReg, FI, 21);
DECL_BIT(ARMControlReg, U, 22);
DECL_BIT(ARMControlReg, XP, 23);
DECL_BIT(ARMControlReg, VE, 24);
DECL_BIT(ARMControlReg, EE, 25);
DECL_BIT(ARMControlReg, L2, 26);

DECL_BITFIELD(ARMCoprocessorAccess, uint32_t);

DECL_BITFIELD(ARMCacheability, uint32_t);
DECL_BIT(ARMCacheability, 0, 0);
DECL_BIT(ARMCacheability, 1, 1);
DECL_BIT(ARMCacheability, 2, 2);
DECL_BIT(ARMCacheability, 3, 3);
DECL_BIT(ARMCacheability, 4, 4);
DECL_BIT(ARMCacheability, 5, 5);
DECL_BIT(ARMCacheability, 6, 6);
DECL_BIT(ARMCacheability, 7, 7);

DECL_BITFIELD(ARMProtection, uint32_t);
DECL_BIT(ARMProtection, Enable, 0);
DECL_BITS(ARMProtection, Size, 1, 5);
DECL_BITS(ARMProtection, Base, 12, 20);

DECL_BITFIELD(ARMTCMControl, uint32_t);
DECL_BITS(ARMTCMControl, VirtualSize, 1, 5);
DECL_BITS(ARMTCMControl, Base, 12, 20);

struct ARMCP15 {
	struct {
		ARMCPUID cpuid;
		ARMCacheType cachetype;
		ARMTCMType tcmtype;
		ARMTLBType tlbtype;
		ARMMPUType mputype;
	} r0;
	struct {
		ARMControlReg c0;
		uint32_t c1;
		ARMCoprocessorAccess cpAccess;
	} r1;
	struct {
		ARMCacheability d;
		ARMCacheability i;
	} r2;
	struct {
		ARMCacheability d;
	} r3;
	struct {
		ARMProtection region[8];
	} r6;
	struct {
		ARMTCMControl d;
		ARMTCMControl i;
	} r9;
};

struct ARMCore {
	int32_t gprs[16];
	ARMPSR cpsr;
	ARMPSR spsr;

	int32_t cycles;
	int32_t nextEvent;
	int halted;

	int32_t bankedRegisters[6][7];
	int32_t bankedSPSRs[6];

	int32_t shifterOperand;
	int32_t shifterCarryOut;

	uint32_t prefetch[2];
	enum ExecutionMode executionMode;
	enum PrivilegeMode privilegeMode;

	struct ARMMemory memory;
	struct ARMInterruptHandler irqh;
	struct ARMCP15 cp15;

	struct mCPUComponent* master;

	size_t numComponents;
	struct mCPUComponent** components;
};

void ARMInit(struct ARMCore* cpu);
void ARMDeinit(struct ARMCore* cpu);
void ARMSetComponents(struct ARMCore* cpu, struct mCPUComponent* master, int extra, struct mCPUComponent** extras);
void ARMHotplugAttach(struct ARMCore* cpu, size_t slot);
void ARMHotplugDetach(struct ARMCore* cpu, size_t slot);

void ARMReset(struct ARMCore* cpu);
void ARMSetPrivilegeMode(struct ARMCore*, enum PrivilegeMode);
void ARMRaiseIRQ(struct ARMCore*);
void ARMRaiseSWI(struct ARMCore*);
void ARMRaiseUndefined(struct ARMCore*);
void ARMHalt(struct ARMCore*);

void ARMv4Run(struct ARMCore* cpu);
void ARMv4RunLoop(struct ARMCore* cpu);
int32_t ARMv4RunCycles(struct ARMCore* cpu, int32_t cycles);
void ARMv5Run(struct ARMCore* cpu);
void ARMv5RunLoop(struct ARMCore* cpu);
int32_t ARMv5RunCycles(struct ARMCore* cpu, int32_t cycles);
void ARMRunFake(struct ARMCore* cpu, uint32_t opcode);

CXX_GUARD_END

#endif
