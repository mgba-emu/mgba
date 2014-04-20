#include "memory-debugger.h"

#include "debugger.h"

#include <string.h>

static void ARMDebuggerShim_store32(struct ARMCore*, uint32_t address, int32_t value, int* cycleCounter);
static void ARMDebuggerShim_store16(struct ARMCore*, uint32_t address, int16_t value, int* cycleCounter);
static void ARMDebuggerShim_store8(struct ARMCore*, uint32_t address, int8_t value, int* cycleCounter);
static void ARMDebuggerShim_setActiveRegion(struct ARMCore* cpu, uint32_t address);

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

CREATE_SHIM(load32, int32_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(load16, int16_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(loadU16, uint16_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(load8, int8_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(loadU8, uint8_t, (struct ARMCore* cpu, uint32_t address, int* cycleCounter), address, cycleCounter)
CREATE_SHIM(waitMultiple, int, (struct ARMCore* cpu, uint32_t startAddress, int count), startAddress, count)

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
	debugger->cpu->memory.waitMultiple = ARMDebuggerShim_waitMultiple;
}

void ARMDebuggerShim_store32(struct ARMCore* cpu, uint32_t address, int32_t value, int* cycleCounter) {
	struct ARMDebugger* debugger;
	FIND_DEBUGGER(debugger, cpu);
	if (_checkWatchpoints(debugger->watchpoints, address, 4)) {
		ARMDebuggerEnter(debugger, DEBUGGER_ENTER_WATCHPOINT);
	}
	debugger->originalMemory.store32(debugger->cpu, address, value, cycleCounter);
}

void ARMDebuggerShim_store16(struct ARMCore* cpu, uint32_t address, int16_t value, int* cycleCounter) {
	struct ARMDebugger* debugger;
	FIND_DEBUGGER(debugger, cpu);
	if (_checkWatchpoints(debugger->watchpoints, address, 2)) {
		ARMDebuggerEnter(debugger, DEBUGGER_ENTER_WATCHPOINT);
	}
	debugger->originalMemory.store16(debugger->cpu, address, value, cycleCounter);
}

void ARMDebuggerShim_store8(struct ARMCore* cpu, uint32_t address, int8_t value, int* cycleCounter) {
	struct ARMDebugger* debugger;
	FIND_DEBUGGER(debugger, cpu);
	if (_checkWatchpoints(debugger->watchpoints, address, 1)) {
		ARMDebuggerEnter(debugger, DEBUGGER_ENTER_WATCHPOINT);
	}
	debugger->originalMemory.store8(debugger->cpu, address, value, cycleCounter);
}

void ARMDebuggerShim_setActiveRegion(struct ARMCore* cpu, uint32_t address) {
	struct ARMDebugger* debugger;
	FIND_DEBUGGER(debugger, cpu);
	debugger->originalMemory.setActiveRegion(cpu, address);
}
