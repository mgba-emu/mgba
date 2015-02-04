/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "memory-debugger.h"

#include "debugger.h"

#include <string.h>

static bool _checkWatchpoints(struct ARMDebugger* debugger, uint32_t address, struct DebuggerEntryInfo* info, int width);

static uint32_t _popcount32(unsigned bits) {
	bits = bits - ((bits >> 1) & 0x55555555);
	bits = (bits & 0x33333333) + ((bits >> 2) & 0x33333333);
	return (((bits + (bits >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

#define FIND_DEBUGGER(DEBUGGER, CPU) \
	{ \
		DEBUGGER = 0; \
		size_t i; \
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
		struct DebuggerEntryInfo info = { }; \
		if (_checkWatchpoints(debugger, address, &info, WIDTH)) { \
			ARMDebuggerEnter(debugger, DEBUGGER_ENTER_WATCHPOINT, &info); \
		} \
		return debugger->originalMemory.NAME(cpu, ARGS); \
	}

#define CREATE_MULTIPLE_WATCHPOINT_SHIM(NAME) \
	static uint32_t ARMDebuggerShim_ ## NAME (struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) { \
		struct ARMDebugger* debugger; \
		FIND_DEBUGGER(debugger, cpu); \
		uint32_t popcount = _popcount32(mask); \
		int offset = 4; \
		int base = address; \
		if (direction & LSM_D) { \
			offset = -4; \
			base -= (popcount << 2) - 4; \
		} \
		if (direction & LSM_B) { \
			base += offset; \
		} \
		unsigned i; \
		for (i = 0; i < popcount; ++i) { \
			struct DebuggerEntryInfo info = { }; \
			if (_checkWatchpoints(debugger, base + 4 * i, &info, 4)) { \
				ARMDebuggerEnter(debugger, DEBUGGER_ENTER_WATCHPOINT, &info); \
			} \
		} \
		return debugger->originalMemory.NAME(cpu, address, mask, direction, cycleCounter); \
	}

CREATE_WATCHPOINT_SHIM(load32, 4, uint32_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_SHIM(load16, 2, uint32_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_SHIM(load8, 1, uint32_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_SHIM(store32, 4, void, (struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_WATCHPOINT_SHIM(store16, 2, void, (struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_WATCHPOINT_SHIM(store8, 1, void, (struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_MULTIPLE_WATCHPOINT_SHIM(loadMultiple)
CREATE_MULTIPLE_WATCHPOINT_SHIM(storeMultiple)
CREATE_SHIM(setActiveRegion, void, (struct ARMCore* cpu, uint32_t address), address)

static bool _checkWatchpoints(struct ARMDebugger* debugger, uint32_t address, struct DebuggerEntryInfo* info, int width) {
	--width;
	struct DebugWatchpoint* watchpoints;
	for (watchpoints = debugger->watchpoints; watchpoints; watchpoints = watchpoints->next) {
		if (!((watchpoints->address ^ address) & ~width)) {
			switch (width + 1) {
			case 1:
				info->oldValue = debugger->originalMemory.load8(debugger->cpu, address, 0);
				break;
			case 2:
				info->oldValue = debugger->originalMemory.load16(debugger->cpu, address, 0);
				break;
			case 4:
				info->oldValue = debugger->originalMemory.load32(debugger->cpu, address, 0);
				break;
			}
			info->address = address;
			info->watchType = watchpoints->type;
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
	debugger->cpu->memory.load8 = ARMDebuggerShim_load8;
	debugger->cpu->memory.storeMultiple = ARMDebuggerShim_storeMultiple;
	debugger->cpu->memory.loadMultiple = ARMDebuggerShim_loadMultiple;
	debugger->cpu->memory.setActiveRegion = ARMDebuggerShim_setActiveRegion;
}

void ARMDebuggerRemoveMemoryShim(struct ARMDebugger* debugger) {
	debugger->cpu->memory.store32 = debugger->originalMemory.store32;
	debugger->cpu->memory.store16 = debugger->originalMemory.store16;
	debugger->cpu->memory.store8 = debugger->originalMemory.store8;
	debugger->cpu->memory.load32 = debugger->originalMemory.load32;
	debugger->cpu->memory.load16 = debugger->originalMemory.load16;
	debugger->cpu->memory.load8 = debugger->originalMemory.load8;
	debugger->cpu->memory.storeMultiple = debugger->originalMemory.storeMultiple;
	debugger->cpu->memory.loadMultiple = debugger->originalMemory.loadMultiple;
	debugger->cpu->memory.setActiveRegion = debugger->originalMemory.setActiveRegion;
}
