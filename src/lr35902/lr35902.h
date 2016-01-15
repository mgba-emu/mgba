/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef LR35902_H
#define LR35902_H

#include "util/common.h"

#include "lr35902/isa-lr35902.h"

struct LR35902Core;

#pragma pack(push, 1)
union FlagRegister {
	struct {
#if defined(__POWERPC__) || defined(__PPC__)
		unsigned z : 1;
		unsigned n : 1;
		unsigned h : 1;
		unsigned c : 1;
		unsigned : 4;
#else
		unsigned : 4;
		unsigned c : 1;
		unsigned h : 1;
		unsigned n : 1;
		unsigned z : 1;
#endif
	};

	uint8_t packed;
};
#pragma pack(pop, 1)

enum LR35902ExecutionState {
	LR35902_CORE_FETCH = 0,
	LR35902_CORE_DECODE,
	LR35902_CORE_STALL,
	LR35902_CORE_EXECUTE,

	LR35902_CORE_MEMORY_LOAD = 5,
	LR35902_CORE_MEMORY_STORE = 9,
	LR35902_CORE_MEMORY_MOVE_INDEX_LOAD,
	LR35902_CORE_MEMORY_MOVE_INDEX_STORE,
	LR35902_CORE_READ_PC,
	LR35902_CORE_READ_PC_STALL,
};

struct LR35902Memory {
	uint16_t (*load16)(struct LR35902Core*, uint16_t address);
	uint8_t (*load8)(struct LR35902Core*, uint16_t address);

	void (*store16)(struct LR35902Core*, uint16_t address, int16_t value);
	void (*store8)(struct LR35902Core*, uint16_t address, int8_t value);

	uint8_t* activeRegion;
	uint16_t activeMask;
	void (*setActiveRegion)(struct LR35902Core*, uint16_t address);
};

struct LR35902InterruptHandler {
	void (*reset)(struct LR35902Core* cpu);
	void (*processEvents)(struct LR35902Core* cpu);
	void (*setInterrupts)(struct LR35902Core* cpu, bool enable);

	void (*hitStub)(struct LR35902Core* cpu);
};

// TODO: Merge with ARMComponent?
struct LR35902Component {
	uint32_t id;
	void (*init)(struct LR35902Core* cpu, struct LR35902Component* component);
	void (*deinit)(struct LR35902Component* component);
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
#pragma pack(pop, 1)
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
	int halted;

	uint8_t bus;
	bool condition;
	LR35902Instruction instruction;

	bool irqPending;
	uint16_t irqVector;

	struct LR35902Memory memory;
	struct LR35902InterruptHandler irqh;

	struct LR35902Component* master;

	size_t numComponents;
	struct LR35902Component** components;
};

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

void LR35902Init(struct LR35902Core* cpu);
void LR35902Deinit(struct LR35902Core* cpu);
void LR35902SetComponents(struct LR35902Core* cpu, struct LR35902Component* master, int extra, struct LR35902Component** extras);
void LR35902HotplugAttach(struct LR35902Core* cpu, size_t slot);
void LR35902HotplugDetach(struct LR35902Core* cpu, size_t slot);

void LR35902Reset(struct LR35902Core* cpu);
void LR35902RaiseIRQ(struct LR35902Core* cpu, uint8_t vector);

void LR35902Tick(struct LR35902Core* cpu);

#endif
