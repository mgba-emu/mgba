/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/arm/debugger/memory-debugger.h>

#include <mgba/internal/arm/debugger/debugger.h>
#include <mgba/internal/debugger/parser.h>

#include <mgba-util/math.h>

#include <string.h>

static bool _checkWatchpoints(struct ARMDebugger* debugger, uint32_t address, struct mDebuggerEntryInfo* info, enum mWatchpointType type, uint32_t newValue, int width);

#define FIND_DEBUGGER(DEBUGGER, CPU) \
	do { \
		DEBUGGER = 0; \
		size_t i; \
		for (i = 0; i < CPU->numComponents; ++i) { \
			if (CPU->components[i]->id == DEBUGGER_ID) { \
				DEBUGGER = (struct ARMDebugger*) ((struct mDebugger*) cpu->components[i])->platform; \
				goto debuggerFound; \
			} \
		} \
		abort(); \
		debuggerFound: break; \
	} while(0)

#define CREATE_SHIM(NAME, RETURN, TYPES, ...) \
	static RETURN DebuggerShim_ ## NAME TYPES { \
		struct ARMDebugger* debugger; \
		FIND_DEBUGGER(debugger, cpu); \
		return debugger->originalMemory.NAME(cpu, __VA_ARGS__); \
	}

#define CREATE_WATCHPOINT_READ_SHIM(NAME, WIDTH, RETURN, TYPES, ...) \
	static RETURN DebuggerShim_ ## NAME TYPES { \
		struct ARMDebugger* debugger; \
		FIND_DEBUGGER(debugger, cpu); \
		struct mDebuggerEntryInfo info; \
		if (_checkWatchpoints(debugger, address, &info, WATCHPOINT_READ, 0, WIDTH)) { \
			mDebuggerEnter(debugger->d.p, DEBUGGER_ENTER_WATCHPOINT, &info); \
		} \
		return debugger->originalMemory.NAME(cpu, __VA_ARGS__); \
	}

#define CREATE_WATCHPOINT_WRITE_SHIM(NAME, WIDTH, RETURN, TYPES, ...) \
	static RETURN DebuggerShim_ ## NAME TYPES { \
		struct ARMDebugger* debugger; \
		FIND_DEBUGGER(debugger, cpu); \
		struct mDebuggerEntryInfo info; \
		if (_checkWatchpoints(debugger, address, &info, WATCHPOINT_WRITE, value, WIDTH)) { \
			mDebuggerEnter(debugger->d.p, DEBUGGER_ENTER_WATCHPOINT, &info); \
		} \
		return debugger->originalMemory.NAME(cpu, __VA_ARGS__); \
	}

#define CREATE_MULTIPLE_WATCHPOINT_SHIM(NAME, ACCESS_TYPE) \
	static uint32_t DebuggerShim_ ## NAME (struct ARMCore* cpu, uint32_t address, int mask, enum LSMDirection direction, int* cycleCounter) { \
		struct ARMDebugger* debugger; \
		FIND_DEBUGGER(debugger, cpu); \
		uint32_t popcount = popcount32(mask); \
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
			struct mDebuggerEntryInfo info; \
			if (_checkWatchpoints(debugger, base + 4 * i, &info, ACCESS_TYPE, 0, 4)) { \
				mDebuggerEnter(debugger->d.p, DEBUGGER_ENTER_WATCHPOINT, &info); \
			} \
		} \
		return debugger->originalMemory.NAME(cpu, address, mask, direction, cycleCounter); \
	}

CREATE_WATCHPOINT_READ_SHIM(load32, 4, uint32_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_READ_SHIM(load16, 2, uint32_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_READ_SHIM(load8, 1, uint32_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_WATCHPOINT_WRITE_SHIM(store32, 4, void, (struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_WATCHPOINT_WRITE_SHIM(store16, 2, void, (struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_WATCHPOINT_WRITE_SHIM(store8, 1, void, (struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter), address, value, cycleCounter)
CREATE_MULTIPLE_WATCHPOINT_SHIM(loadMultiple, WATCHPOINT_READ)
CREATE_MULTIPLE_WATCHPOINT_SHIM(storeMultiple, WATCHPOINT_WRITE)
CREATE_SHIM(setActiveRegion, void, (struct ARMCore* cpu, uint32_t address), address)

static bool _checkWatchpoints(struct ARMDebugger* debugger, uint32_t address, struct mDebuggerEntryInfo* info, enum mWatchpointType type, uint32_t newValue, int width) {
	struct mWatchpoint* watchpoint;
	size_t i;
	uint32_t minAddress = address & ~(width - 1);
	uint32_t maxAddress = minAddress + width;
	for (i = 0; i < mWatchpointListSize(&debugger->watchpoints); ++i) {
		watchpoint = mWatchpointListGetPointer(&debugger->watchpoints, i);
		if (watchpoint->type & type && watchpoint->minAddress < maxAddress && minAddress < watchpoint->maxAddress) {
			if (watchpoint->condition) {
				int32_t value;
				int segment;
				if (!mDebuggerEvaluateParseTree(debugger->d.p, watchpoint->condition, &value, &segment) || !(value || segment >= 0)) {
					continue;
				}
			}

			uint32_t oldValue;
			switch (width) {
			case 1:
				oldValue = debugger->originalMemory.load8(debugger->cpu, address, 0);
				break;
			case 2:
				oldValue = debugger->originalMemory.load16(debugger->cpu, address, 0);
				break;
			case 4:
				oldValue = debugger->originalMemory.load32(debugger->cpu, address, 0);
				break;
			default:
				continue;
			}
			if ((watchpoint->type & WATCHPOINT_CHANGE) && newValue == oldValue) {
				continue;
			}
			info->type.wp.oldValue = oldValue;
			info->type.wp.newValue = newValue;
			info->address = address;
			info->type.wp.watchType = watchpoint->type;
			info->type.wp.accessType = type;
			info->pointId = watchpoint->id;
			return true;
		}
	}
	return false;
}

void ARMDebuggerInstallMemoryShim(struct ARMDebugger* debugger) {
	debugger->originalMemory = debugger->cpu->memory;
	debugger->cpu->memory.store32 = DebuggerShim_store32;
	debugger->cpu->memory.store16 = DebuggerShim_store16;
	debugger->cpu->memory.store8 = DebuggerShim_store8;
	debugger->cpu->memory.load32 = DebuggerShim_load32;
	debugger->cpu->memory.load16 = DebuggerShim_load16;
	debugger->cpu->memory.load8 = DebuggerShim_load8;
	debugger->cpu->memory.storeMultiple = DebuggerShim_storeMultiple;
	debugger->cpu->memory.loadMultiple = DebuggerShim_loadMultiple;
	debugger->cpu->memory.setActiveRegion = DebuggerShim_setActiveRegion;
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
