/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/sm83/sm83.h>

#include <mgba/internal/sm83/isa-sm83.h>

void SM83Init(struct SM83Core* cpu) {
	cpu->master->init(cpu, cpu->master);
	size_t i;
	for (i = 0; i < cpu->numComponents; ++i) {
		if (cpu->components[i] && cpu->components[i]->init) {
			cpu->components[i]->init(cpu, cpu->components[i]);
		}
	}
}

void SM83Deinit(struct SM83Core* cpu) {
	if (cpu->master->deinit) {
		cpu->master->deinit(cpu->master);
	}
	size_t i;
	for (i = 0; i < cpu->numComponents; ++i) {
		if (cpu->components[i] && cpu->components[i]->deinit) {
			cpu->components[i]->deinit(cpu->components[i]);
		}
	}
}

void SM83SetComponents(struct SM83Core* cpu, struct mCPUComponent* master, int extra, struct mCPUComponent** extras) {
	cpu->master = master;
	cpu->numComponents = extra;
	cpu->components = extras;
}


void SM83HotplugAttach(struct SM83Core* cpu, size_t slot) {
	if (slot >= cpu->numComponents) {
		return;
	}
	cpu->components[slot]->init(cpu, cpu->components[slot]);
}

void SM83HotplugDetach(struct SM83Core* cpu, size_t slot) {
	if (slot >= cpu->numComponents) {
		return;
	}
	cpu->components[slot]->deinit(cpu->components[slot]);
}

void SM83Reset(struct SM83Core* cpu) {
	cpu->af = 0;
	cpu->bc = 0;
	cpu->de = 0;
	cpu->hl = 0;

	cpu->sp = 0;
	cpu->pc = 0;

	cpu->instruction = 0;

	cpu->tMultiplier = 2;
	cpu->cycles = 0;
	cpu->nextEvent = 0;
	cpu->executionState = SM83_CORE_FETCH;
	cpu->halted = 0;

	cpu->irqPending = false;
	cpu->irqh.reset(cpu);
}

void SM83RaiseIRQ(struct SM83Core* cpu) {
	cpu->irqPending = true;
}

static void _SM83InstructionIRQStall(struct SM83Core* cpu) {
	cpu->executionState = SM83_CORE_STALL;
}

static void _SM83InstructionIRQFinish(struct SM83Core* cpu) {
	cpu->executionState = SM83_CORE_OP2;
	cpu->instruction = _SM83InstructionIRQStall;
}

static void _SM83InstructionIRQDelay(struct SM83Core* cpu) {
	--cpu->sp;
	cpu->index = cpu->sp;
	cpu->bus = cpu->pc;
	cpu->executionState = SM83_CORE_MEMORY_STORE;
	cpu->instruction = _SM83InstructionIRQFinish;
	cpu->pc = cpu->irqh.irqVector(cpu);
	cpu->memory.setActiveRegion(cpu, cpu->pc);
}

static void _SM83InstructionIRQ(struct SM83Core* cpu) {
	--cpu->sp;
	cpu->index = cpu->sp;
	cpu->bus = cpu->pc >> 8;
	cpu->executionState = SM83_CORE_MEMORY_STORE;
	cpu->instruction = _SM83InstructionIRQDelay;
}

static void _SM83Step(struct SM83Core* cpu) {
	cpu->cycles += cpu->tMultiplier;
	enum SM83ExecutionState state = cpu->executionState;
	cpu->executionState = SM83_CORE_IDLE_0;
	switch (state) {
	case SM83_CORE_FETCH:
		if (cpu->irqPending) {
			cpu->index = cpu->sp;
			cpu->irqPending = false;
			cpu->instruction = _SM83InstructionIRQ;
			cpu->irqh.setInterrupts(cpu, false);
			break;
		}
		cpu->bus = cpu->memory.cpuLoad8(cpu, cpu->pc);
		cpu->instruction = _sm83InstructionTable[cpu->bus];
		++cpu->pc;
		break;
	case SM83_CORE_MEMORY_LOAD:
		cpu->bus = cpu->memory.load8(cpu, cpu->index);
		break;
	case SM83_CORE_MEMORY_STORE:
		cpu->memory.store8(cpu, cpu->index, cpu->bus);
		break;
	case SM83_CORE_READ_PC:
		cpu->bus = cpu->memory.cpuLoad8(cpu, cpu->pc);
		++cpu->pc;
		break;
	case SM83_CORE_STALL:
		cpu->instruction = _sm83InstructionTable[0]; // NOP
		break;
	case SM83_CORE_HALT_BUG:
		if (cpu->irqPending) {
			cpu->index = cpu->sp;
			cpu->irqPending = false;
			cpu->instruction = _SM83InstructionIRQ;
			cpu->irqh.setInterrupts(cpu, false);
			break;
		}
		cpu->bus = cpu->memory.cpuLoad8(cpu, cpu->pc);
		cpu->instruction = _sm83InstructionTable[cpu->bus];
		break;
	default:
		break;
	}
}

static inline bool _SM83TickInternal(struct SM83Core* cpu) {
	bool running = true;
	_SM83Step(cpu);
	int t = cpu->tMultiplier;
	if (cpu->cycles + t * 2 >= cpu->nextEvent) {
		if (cpu->cycles >= cpu->nextEvent) {
			cpu->irqh.processEvents(cpu);
		}
		cpu->cycles += t;
		++cpu->executionState;
		if (cpu->cycles >= cpu->nextEvent) {
			cpu->irqh.processEvents(cpu);
		}
		cpu->cycles += t;
		++cpu->executionState;
		if (cpu->cycles >= cpu->nextEvent) {
			cpu->irqh.processEvents(cpu);
		}
		running = false;
	} else {
		cpu->cycles += t * 2;
	}
	cpu->executionState = SM83_CORE_FETCH;
	cpu->instruction(cpu);
	cpu->cycles += t;
	return running;
}

void SM83Tick(struct SM83Core* cpu) {
	while (cpu->cycles >= cpu->nextEvent) {
		cpu->irqh.processEvents(cpu);
	}
	_SM83TickInternal(cpu);
}

void SM83Run(struct SM83Core* cpu) {
	bool running = true;
	while (running || cpu->executionState != SM83_CORE_FETCH) {
		if (cpu->cycles < cpu->nextEvent) {
			running = _SM83TickInternal(cpu) && running;
		} else {
			cpu->irqh.processEvents(cpu);
			running = false;
		}
	}
}
