/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "memory-debugger.h"

#include "debugger.h"

#include <string.h>

static bool _checkWatchpoints(struct DebugBreakpoint* watchpoints, uint32_t address, int width);

#define FIND_DEBUGGER(DEBUGGER, CPU) \
	{ \
		DEBUGGER = 0; \
		int i; \
		for (i = 0; i < CPU->numComponents; ++i) { \
			if (CPU->components[i]->id == ARM_DEBUGGER_ID) { \
				DEBUGGER = (struct ARMDebugger*) cpu->components[i]; \
				break; \
			} \
		} \
	}

#define CREATE_SHIM(NAME, RETURN, TYPES, ARGS...) \
	static RETURN ARMDebuggerShim_ ## NAME TYPES { \
		struct ARMDebugger* debugger; \
		FIND_DEBUGGER(debugger, cpu); \
		return debugger->originalMemory.NAME(cpu, ARGS); \
	}

#define CREATE_WATCHPOINT_SHIM(NAME, WIDTH, RETURN, TYPES, ARGS...) \
	static RETURN ARMDebuggerShim_ ## NAME TYPES { \
		struct ARMDebugger* debugger; \
		FIND_DEBUGGER(debugger, cpu); \
		if (_checkWatchpoints(debugger->watchpoints, address, WIDTH)) { \
			ARMDebuggerEnter(debugger, DEBUGGER_ENTER_WATCHPOINT); \
		} \
		return debugger->originalMemory.NAME(cpu, ARGS); \
	}

CREATE_WATCHPOINT_SHIM(load32, 4, int32_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_SHIM(load16, 2, int16_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_SHIM(loadU16, 2, uint16_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_SHIM(load8, 1, int8_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_SHIM(loadU8, 1, uint8_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_SHIM(store32, 4, void, (struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_WATCHPOINT_SHIM(store16, 2, void, (struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_WATCHPOINT_SHIM(store8, 1, void, (struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_SHIM(setActiveRegion, void, (struct ARMCore* cpu, uint32_t address), address)

static bool _checkWatchpoints(struct DebugBreakpoint* watchpoints, uint32_t address, int width) {
	width -= 1;
	for (; watchpoints; watchpoints = watchpoints->next) {
		if (!((watchpoints->address ^ address) & ~width)) {
			return true;
		}
	}
	return false;
}

void ARMDebuggerInstallMemoryShim(struct ARMDebugger* debugger) {
	debugger->originalMemory = debugger->cpu->memory;
	debugger->cpu->memory.store32 = ARMDebuggerShim_store32;
	debugger->cpu->memory.store16 = ARMDebuggerShim_store16;
	debugger->cpu->memory.store8 = ARMDebuggerShim_store8;
	debugger->cpu->memory.load32 = ARMDebuggerShim_load32;
	debugger->cpu->memory.load16 = ARMDebuggerShim_load16;
	debugger->cpu->memory.loadU16 = ARMDebuggerShim_loadU16;
	debugger->cpu->memory.load8 = ARMDebuggerShim_load8;
	debugger->cpu->memory.loadU8 = ARMDebuggerShim_loadU8;
	debugger->cpu->memory.setActiveRegion = ARMDebuggerShim_setActiveRegion;
}

void ARMDebuggerRemoveMemoryShim(struct ARMDebugger* debugger) {
	debugger->cpu->memory.store32 = debugger->originalMemory.store32;
	debugger->cpu->memory.store16 = debugger->originalMemory.store16;
	debugger->cpu->memory.store8 = debugger->originalMemory.store8;
	debugger->cpu->memory.load32 = debugger->originalMemory.load32;
	debugger->cpu->memory.load16 = debugger->originalMemory.load16;
	debugger->cpu->memory.loadU16 = debugger->originalMemory.loadU16;
	debugger->cpu->memory.load8 = debugger->originalMemory.load8;
	debugger->cpu->memory.loadU8 = debugger->originalMemory.loadU8;
	debugger->cpu->memory.setActiveRegion = debugger->originalMemory.setActiveRegion;
}
