/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ARM_H
#define ARM_H

#include "util/common.h"

#include "core/cpu.h"
#include "util/table.h"
#include "util/bump-allocator.h"

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

enum ARMExecutor {
	ARM_INTERPRETER = 0,
	ARM_DYNAREC = 1,
};

struct ARMCore;

union PSR {
	struct {
#if defined(__POWERPC__) || defined(__PPC__)
		unsigned n : 1;
		unsigned z : 1;
		unsigned c : 1;
		unsigned v : 1;
		unsigned : 20;
		unsigned i : 1;
		unsigned f : 1;
		enum ExecutionMode t : 1;
		enum PrivilegeMode priv : 5;
#else
		enum PrivilegeMode priv : 5;
		enum ExecutionMode t : 1;
		unsigned f : 1;
		unsigned i : 1;
		unsigned : 20;
		unsigned v : 1;
		unsigned c : 1;
		unsigned z : 1;
		unsigned n : 1;
#endif
	};

	int32_t packed;
};

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

	void (*hitStub)(struct ARMCore* cpu, uint32_t opcode);
};

struct ARMDynarec {
	bool inDynarec;
	struct BumpAllocator traceAlloc;
	struct Table armTraces;
	struct Table thumbTraces;
	void* bufferStart;
	void* buffer;
	void* tracePrediction;
	void* currentTrace;
	void* temporaryMemory;
	void (*execute)(struct ARMCore* cpu, void* execution_token);
	void* epilogue;
	void* flushNZCVAndRegsAndEpilogue;
	void* flushNZCVAndEpilogue;
	void* flushRegsAndEpilogue;
	void* cycleExitAndEpilogue;
	void* cycleExitAndFlushNZCVAndRegsAndEpilogue;
	void* cycleExitAndFlushNZCVAndEpilogue;
	void* cycleExitAndFlushRegsAndEpilogue;
	void* cycleCheckHandler;
};

struct ARMCore {
	int32_t gprs[16];
	union PSR cpsr;
	union PSR spsr;

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

	enum ARMExecutor executor;
	struct ARMDynarec dynarec;

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

void ARMRun(struct ARMCore* cpu);
void ARMRunLoop(struct ARMCore* cpu);
void ARMRunFake(struct ARMCore* cpu, uint32_t opcode);

#endif
