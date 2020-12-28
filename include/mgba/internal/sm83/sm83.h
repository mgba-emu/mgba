/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef SM83_H
#define SM83_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/cpu.h>
#include <mgba/internal/sm83/isa-sm83.h>

struct SM83Core;

#pragma pack(push, 1)
union FlagRegister {
	struct {
#ifdef __BIG_ENDIAN__
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

enum SM83ExecutionState {
	SM83_CORE_FETCH = 3,
	SM83_CORE_IDLE_0 = 0,
	SM83_CORE_IDLE_1 = 1,
	SM83_CORE_EXECUTE = 2,

	SM83_CORE_MEMORY_LOAD = 7,
	SM83_CORE_MEMORY_STORE = 11,
	SM83_CORE_READ_PC = 15,
	SM83_CORE_STALL = 19,
	SM83_CORE_OP2 = 23,
	SM83_CORE_HALT_BUG = 27,
};
struct SM83Memory {
	uint8_t (*cpuLoad8)(struct SM83Core*, uint16_t address);
	uint8_t (*load8)(struct SM83Core*, uint16_t address);
	void (*store8)(struct SM83Core*, uint16_t address, int8_t value);

	int (*currentSegment)(struct SM83Core*, uint16_t address);

	const uint8_t* activeRegion;
	uint16_t activeMask;
	uint16_t activeRegionEnd;
	void (*setActiveRegion)(struct SM83Core*, uint16_t address);
};

struct SM83InterruptHandler {
	void (*reset)(struct SM83Core* cpu);
	void (*processEvents)(struct SM83Core* cpu);
	void (*setInterrupts)(struct SM83Core* cpu, bool enable);
	uint16_t (*irqVector)(struct SM83Core* cpu);
	void (*halt)(struct SM83Core* cpu);
	void (*stop)(struct SM83Core* cpu);

	void (*hitIllegal)(struct SM83Core* cpu);
};

#ifdef __BIG_ENDIAN__
#define SM83_REGISTER_PAIR(HIGH, LOW) union { \
		struct { \
			uint8_t HIGH; \
			uint8_t LOW; \
		}; \
		uint16_t HIGH ## LOW; \
	}

#define SM83_AF_REGISTER union { \
		struct { \
			uint8_t a; \
			union FlagRegister f; \
		}; \
		uint16_t af; \
	}
#else
#define SM83_REGISTER_PAIR(HIGH, LOW) union { \
		struct { \
			uint8_t LOW; \
			uint8_t HIGH; \
		}; \
		uint16_t HIGH ## LOW; \
	}

#define SM83_AF_REGISTER union { \
		struct { \
			union FlagRegister f; \
			uint8_t a; \
		}; \
		uint16_t af; \
	}
#endif

#define SM83_REGISTER_FILE struct { \
	SM83_AF_REGISTER; \
	SM83_REGISTER_PAIR(b, c); \
	SM83_REGISTER_PAIR(d, e); \
	SM83_REGISTER_PAIR(h, l); \
	uint16_t sp; \
	uint16_t pc; \
}

struct SM83RegisterFile {
#pragma pack(push, 1)
	SM83_REGISTER_FILE;
#pragma pack(pop)
};

struct SM83Core {
#pragma pack(push, 1)
	union {
		struct SM83RegisterFile regs;
		SM83_REGISTER_FILE;
	};
#pragma pack(pop)

	uint16_t index;

	int tMultiplier;
	int32_t cycles;
	int32_t nextEvent;
	enum SM83ExecutionState executionState;
	bool halted;

	uint8_t bus;
	bool condition;
	SM83Instruction instruction;

	bool irqPending;

	struct SM83Memory memory;
	struct SM83InterruptHandler irqh;

	struct mCPUComponent* master;

	size_t numComponents;
	struct mCPUComponent** components;
};
#undef SM83_REGISTER_FILE

void SM83Init(struct SM83Core* cpu);
void SM83Deinit(struct SM83Core* cpu);
void SM83SetComponents(struct SM83Core* cpu, struct mCPUComponent* master, int extra, struct mCPUComponent** extras);
void SM83HotplugAttach(struct SM83Core* cpu, size_t slot);
void SM83HotplugDetach(struct SM83Core* cpu, size_t slot);

void SM83Reset(struct SM83Core* cpu);
void SM83RaiseIRQ(struct SM83Core* cpu);

void SM83Tick(struct SM83Core* cpu);
void SM83Run(struct SM83Core* cpu);

CXX_GUARD_END

#endif
