/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LR35902_H
#define LR35902_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/cpu.h>
#include <mgba/internal/lr35902/isa-lr35902.h>

struct LR35902Core;

#pragma pack(push, 1)
union FlagRegister {
	struct {
#if defined(__POWERPC__) || defined(__PPC__)
		unsigned z : 1;
		unsigned n : 1;
		unsigned h : 1;
		unsigned c : 1;
		unsigned unused : 4;
#else
		unsigned unused : 4;
		unsigned c : 1;
		unsigned h : 1;
		unsigned n : 1;
		unsigned z : 1;
#endif
	};

	uint8_t packed;
};
#pragma pack(pop)

enum LR35902ExecutionState {
	LR35902_CORE_FETCH = 3,
	LR35902_CORE_IDLE_0 = 0,
	LR35902_CORE_IDLE_1 = 1,
	LR35902_CORE_EXECUTE = 2,

	LR35902_CORE_MEMORY_LOAD = 7,
	LR35902_CORE_MEMORY_STORE = 11,
	LR35902_CORE_READ_PC = 15,
	LR35902_CORE_STALL = 19,
	LR35902_CORE_OP2 = 23
};
struct LR35902Memory {
	uint8_t (*cpuLoad8)(struct LR35902Core*, uint16_t address);
	uint8_t (*load8)(struct LR35902Core*, uint16_t address);
	void (*store8)(struct LR35902Core*, uint16_t address, int8_t value);

	int (*currentSegment)(struct LR35902Core*, uint16_t address);

	uint8_t* activeRegion;
	uint16_t activeMask;
	uint16_t activeRegionEnd;
	void (*setActiveRegion)(struct LR35902Core*, uint16_t address);
};

struct LR35902InterruptHandler {
	void (*reset)(struct LR35902Core* cpu);
	void (*processEvents)(struct LR35902Core* cpu);
	void (*setInterrupts)(struct LR35902Core* cpu, bool enable);
	void (*halt)(struct LR35902Core* cpu);
	void (*stop)(struct LR35902Core* cpu);

	void (*hitIllegal)(struct LR35902Core* cpu);
};

struct LR35902Core {
#pragma pack(push, 1)
	union {
		struct {
			union FlagRegister f;
			uint8_t a;
		};
		uint16_t af;
	};
#pragma pack(pop)
	union {
		struct {
			uint8_t c;
			uint8_t b;
		};
		uint16_t bc;
	};
	union {
		struct {
			uint8_t e;
			uint8_t d;
		};
		uint16_t de;
	};
	union {
		struct {
			uint8_t l;
			uint8_t h;
		};
		uint16_t hl;
	};
	uint16_t sp;
	uint16_t pc;

	uint16_t index;

	int32_t cycles;
	int32_t nextEvent;
	enum LR35902ExecutionState executionState;
	bool halted;

	uint8_t bus;
	bool condition;
	LR35902Instruction instruction;

	bool irqPending;
	uint16_t irqVector;

	struct LR35902Memory memory;
	struct LR35902InterruptHandler irqh;

	struct mCPUComponent* master;

	size_t numComponents;
	struct mCPUComponent** components;
};

void LR35902Init(struct LR35902Core* cpu);
void LR35902Deinit(struct LR35902Core* cpu);
void LR35902SetComponents(struct LR35902Core* cpu, struct mCPUComponent* master, int extra, struct mCPUComponent** extras);
void LR35902HotplugAttach(struct LR35902Core* cpu, size_t slot);
void LR35902HotplugDetach(struct LR35902Core* cpu, size_t slot);

void LR35902Reset(struct LR35902Core* cpu);
void LR35902RaiseIRQ(struct LR35902Core* cpu, uint8_t vector);

void LR35902Tick(struct LR35902Core* cpu);
void LR35902Run(struct LR35902Core* cpu);

CXX_GUARD_END

#endif
