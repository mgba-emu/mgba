#include "memory-debugger.h"

#include "debugger.h"

#include <string.h>

static void ARMDebuggerShim_store32(struct ARMMemory*, uint32_t address, int32_t value, int* cycleCounter);
static void ARMDebuggerShim_store16(struct ARMMemory*, uint32_t address, int16_t value, int* cycleCounter);
static void ARMDebuggerShim_store8(struct ARMMemory*, uint32_t address, int8_t value, int* cycleCounter);
static void ARMDebuggerShim_setActiveRegion(struct ARMMemory* memory, uint32_t address);

#define CREATE_SHIM(NAME, RETURN, TYPES, ARGS...) \
	static RETURN ARMDebuggerShim_ ## NAME TYPES { \
		struct DebugMemoryShim* debugMemory = (struct DebugMemoryShim*) memory; \
		return debugMemory->original->NAME(debugMemory->original, ARGS); \
	}

CREATE_SHIM(load32, int32_t, (struct ARMMemory* memory, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(load16, int16_t, (struct ARMMemory* memory, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(loadU16, uint16_t, (struct ARMMemory* memory, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(load8, int8_t, (struct ARMMemory* memory, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(loadU8, uint8_t, (struct ARMMemory* memory, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(waitMultiple, int, (struct ARMMemory* memory, uint32_t startAddress, int count), startAddress, count)

static int _checkWatchpoints(struct DebugBreakpoint* watchpoints, uint32_t address, int width) {
	width -= 1;
	for (; watchpoints; watchpoints = watchpoints->next) {
		if (!((watchpoints->address ^ address) & ~width)) {
			return 1;
		}
	}
	return 0;
}

void ARMDebuggerInstallMemoryShim(struct ARMDebugger* debugger) {
	debugger->memoryShim.original = debugger->cpu->memory;
	memcpy(&debugger->memoryShim.d, debugger->cpu->memory, sizeof(struct ARMMemory));
	debugger->memoryShim.d.store32 = ARMDebuggerShim_store32;
	debugger->memoryShim.d.store16 = ARMDebuggerShim_store16;
	debugger->memoryShim.d.store8 = ARMDebuggerShim_store8;
	debugger->memoryShim.d.load32 = ARMDebuggerShim_load32;
	debugger->memoryShim.d.load16 = ARMDebuggerShim_load16;
	debugger->memoryShim.d.loadU16 = ARMDebuggerShim_loadU16;
	debugger->memoryShim.d.load8 = ARMDebuggerShim_load8;
	debugger->memoryShim.d.loadU8 = ARMDebuggerShim_loadU8;
	debugger->memoryShim.d.setActiveRegion = ARMDebuggerShim_setActiveRegion;
	debugger->memoryShim.d.waitMultiple = ARMDebuggerShim_waitMultiple;
	debugger->cpu->memory = &debugger->memoryShim.d;
}

void ARMDebuggerShim_store32(struct ARMMemory* memory, uint32_t address, int32_t value, int* cycleCounter) {
	struct DebugMemoryShim* debugMemory = (struct DebugMemoryShim*) memory;
	if (_checkWatchpoints(debugMemory->watchpoints, address, 4)) {
		ARMDebuggerEnter(debugMemory->p, DEBUGGER_ENTER_WATCHPOINT);
	}
	debugMemory->original->store32(debugMemory->original, address, value, cycleCounter);
}

void ARMDebuggerShim_store16(struct ARMMemory* memory, uint32_t address, int16_t value, int* cycleCounter) {
	struct DebugMemoryShim* debugMemory = (struct DebugMemoryShim*) memory;
	if (_checkWatchpoints(debugMemory->watchpoints, address, 2)) {
		ARMDebuggerEnter(debugMemory->p, DEBUGGER_ENTER_WATCHPOINT);
	}
	debugMemory->original->store16(debugMemory->original, address, value, cycleCounter);
}

void ARMDebuggerShim_store8(struct ARMMemory* memory, uint32_t address, int8_t value, int* cycleCounter) {
	struct DebugMemoryShim* debugMemory = (struct DebugMemoryShim*) memory;
	if (_checkWatchpoints(debugMemory->watchpoints, address, 1)) {
		ARMDebuggerEnter(debugMemory->p, DEBUGGER_ENTER_WATCHPOINT);
	}
	debugMemory->original->store8(debugMemory->original, address, value, cycleCounter);
}

void ARMDebuggerShim_setActiveRegion(struct ARMMemory* memory, uint32_t address) {
	struct DebugMemoryShim* debugMemory = (struct DebugMemoryShim*) memory;
	debugMemory->original->setActiveRegion(debugMemory->original, address);
	memory->activeRegion = debugMemory->original->activeRegion;
	memory->activeMask = debugMemory->original->activeMask;
	memory->activePrefetchCycles32 = debugMemory->original->activePrefetchCycles32;
	memory->activePrefetchCycles16 = debugMemory->original->activePrefetchCycles16;
	memory->activeNonseqCycles32 = debugMemory->original->activeNonseqCycles32;
	memory->activeNonseqCycles16 = debugMemory->original->activeNonseqCycles16;
}
