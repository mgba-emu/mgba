/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "debugger.h"

#include "arm.h"
#include "isa-inlines.h"

#include "memory-debugger.h"

const uint32_t ARM_DEBUGGER_ID = 0xDEADBEEF;

DEFINE_VECTOR(DebugBreakpointList, struct DebugBreakpoint);
DEFINE_VECTOR(DebugWatchpointList, struct DebugWatchpoint);

static struct DebugBreakpoint* _lookupBreakpoint(struct DebugBreakpointList* breakpoints, uint32_t address) {
	size_t i;
	for (i = 0; i < DebugBreakpointListSize(breakpoints); ++i) {
		if (DebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			return DebugBreakpointListGetPointer(breakpoints, i);
		}
	}
	return 0;
}

static void _checkBreakpoints(struct ARMDebugger* debugger) {
	int instructionLength;
	enum ExecutionMode mode = debugger->cpu->cpsr.a.t;
	if (mode == MODE_ARM) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	struct DebugBreakpoint* breakpoint = _lookupBreakpoint(&debugger->breakpoints, debugger->cpu->gprs[ARM_PC] - instructionLength);
	if (!breakpoint) {
		return;
	}
	struct DebuggerEntryInfo info = {
		.address = breakpoint->address
	};
	ARMDebuggerEnter(debugger, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void ARMDebuggerInit(struct ARMCore*, struct ARMComponent*);
static void ARMDebuggerDeinit(struct ARMComponent*);

void ARMDebuggerCreate(struct ARMDebugger* debugger) {
	debugger->d.id = ARM_DEBUGGER_ID;
	debugger->d.init = ARMDebuggerInit;
	debugger->d.deinit = ARMDebuggerDeinit;
}

void ARMDebuggerInit(struct ARMCore* cpu, struct ARMComponent* component) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) component;
	debugger->cpu = cpu;
	debugger->state = DEBUGGER_RUNNING;
	debugger->originalMemory = cpu->memory;
	debugger->currentBreakpoint = 0;
	DebugBreakpointListInit(&debugger->breakpoints, 0);
	DebugBreakpointListInit(&debugger->swBreakpoints, 0);
	DebugWatchpointListInit(&debugger->watchpoints, 0);
	if (debugger->init) {
		debugger->init(debugger);
	}
}

void ARMDebuggerDeinit(struct ARMComponent* component) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) component;
	debugger->deinit(debugger);
	DebugBreakpointListDeinit(&debugger->breakpoints);
	DebugBreakpointListDeinit(&debugger->swBreakpoints);
	DebugWatchpointListDeinit(&debugger->watchpoints);
}

void ARMDebuggerRun(struct ARMDebugger* debugger) {
	switch (debugger->state) {
	case DEBUGGER_RUNNING:
		if (!DebugBreakpointListSize(&debugger->breakpoints) && !DebugWatchpointListSize(&debugger->watchpoints)) {
			ARMRunLoop(debugger->cpu);
		} else {
			ARMRun(debugger->cpu);
			_checkBreakpoints(debugger);
		}
		break;
	case DEBUGGER_CUSTOM:
		ARMRun(debugger->cpu);
		_checkBreakpoints(debugger);
		debugger->custom(debugger);
		break;
	case DEBUGGER_PAUSED:
		if (debugger->paused) {
			debugger->paused(debugger);
		} else {
			debugger->state = DEBUGGER_RUNNING;
		}
		if (debugger->state != DEBUGGER_PAUSED && debugger->currentBreakpoint) {
			if (debugger->currentBreakpoint->isSw && debugger->setSoftwareBreakpoint) {
				debugger->setSoftwareBreakpoint(debugger, debugger->currentBreakpoint->address, debugger->currentBreakpoint->sw.mode, &debugger->currentBreakpoint->sw.opcode);
			}
			debugger->currentBreakpoint = 0;
		}
		break;
	case DEBUGGER_SHUTDOWN:
		return;
	}
}

void ARMDebuggerEnter(struct ARMDebugger* debugger, enum DebuggerEntryReason reason, struct DebuggerEntryInfo* info) {
	debugger->state = DEBUGGER_PAUSED;
	struct ARMCore* cpu = debugger->cpu;
	cpu->nextEvent = cpu->cycles;
	if (reason == DEBUGGER_ENTER_BREAKPOINT) {
		struct DebugBreakpoint* breakpoint = _lookupBreakpoint(&debugger->swBreakpoints, _ARMPCAddress(cpu));
		debugger->currentBreakpoint = breakpoint;
		if (breakpoint && breakpoint->isSw) {
			info->address = breakpoint->address;
			if (debugger->clearSoftwareBreakpoint) {
				debugger->clearSoftwareBreakpoint(debugger, breakpoint->address, breakpoint->sw.mode, breakpoint->sw.opcode);
			}

			ARMRunFake(cpu, breakpoint->sw.opcode);
		}
	}
	if (debugger->entered) {
		debugger->entered(debugger, reason, info);
	}
}

void ARMDebuggerSetBreakpoint(struct ARMDebugger* debugger, uint32_t address) {
	struct DebugBreakpoint* breakpoint = DebugBreakpointListAppend(&debugger->breakpoints);
	breakpoint->address = address;
	breakpoint->isSw = false;
}

bool ARMDebuggerSetSoftwareBreakpoint(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	uint32_t opcode;
	if (!debugger->setSoftwareBreakpoint || !debugger->setSoftwareBreakpoint(debugger, address, mode, &opcode)) {
		return false;
	}

	struct DebugBreakpoint* breakpoint = DebugBreakpointListAppend(&debugger->swBreakpoints);
	breakpoint->address = address;
	breakpoint->isSw = true;
	breakpoint->sw.opcode = opcode;
	breakpoint->sw.mode = mode;

	return true;
}

void ARMDebuggerClearBreakpoint(struct ARMDebugger* debugger, uint32_t address) {
	struct DebugBreakpointList* breakpoints = &debugger->breakpoints;
	size_t i;
	for (i = 0; i < DebugBreakpointListSize(breakpoints); ++i) {
		if (DebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			DebugBreakpointListShift(breakpoints, i, 1);
		}
	}

}

void ARMDebuggerSetWatchpoint(struct ARMDebugger* debugger, uint32_t address, enum WatchpointType type) {
	if (!DebugWatchpointListSize(&debugger->watchpoints)) {
		ARMDebuggerInstallMemoryShim(debugger);
	}
	struct DebugWatchpoint* watchpoint = DebugWatchpointListAppend(&debugger->watchpoints);
	watchpoint->address = address;
	watchpoint->type = type;
}

void ARMDebuggerClearWatchpoint(struct ARMDebugger* debugger, uint32_t address) {
	struct DebugWatchpointList* watchpoints = &debugger->watchpoints;
	size_t i;
	for (i = 0; i < DebugWatchpointListSize(watchpoints); ++i) {
		if (DebugWatchpointListGetPointer(watchpoints, i)->address == address) {
			DebugWatchpointListShift(watchpoints, i, 1);
		}
	}
	if (!DebugWatchpointListSize(&debugger->watchpoints)) {
		ARMDebuggerRemoveMemoryShim(debugger);
	}
}
