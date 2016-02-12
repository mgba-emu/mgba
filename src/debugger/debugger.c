/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "debugger.h"

#include "arm.h"
#include "isa-inlines.h"

#include "memory-debugger.h"

const uint32_t DEBUGGER_ID = 0xDEADBEEF;

DEFINE_VECTOR(DebugBreakpointList, struct DebugBreakpoint);
DEFINE_VECTOR(DebugWatchpointList, struct DebugWatchpoint);

mLOG_DEFINE_CATEGORY(DEBUGGER, "Debugger");

static struct DebugBreakpoint* _lookupBreakpoint(struct DebugBreakpointList* breakpoints, uint32_t address) {
	size_t i;
	for (i = 0; i < DebugBreakpointListSize(breakpoints); ++i) {
		if (DebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			return DebugBreakpointListGetPointer(breakpoints, i);
		}
	}
	return 0;
}

static void _checkBreakpoints(struct Debugger* debugger) {
	int instructionLength;
	enum ExecutionMode mode = debugger->cpu->cpsr.t;
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
	DebuggerEnter(debugger, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void DebuggerInit(void* cpu, struct mCPUComponent*);
static void DebuggerDeinit(struct mCPUComponent*);

void DebuggerCreate(struct Debugger* debugger) {
	debugger->d.id = DEBUGGER_ID;
	debugger->d.init = DebuggerInit;
	debugger->d.deinit = DebuggerDeinit;
}

void DebuggerInit(void* cpu, struct mCPUComponent* component) {
	struct Debugger* debugger = (struct Debugger*) component;
	debugger->cpu = cpu;
	debugger->state = DEBUGGER_RUNNING;
	debugger->originalMemory = debugger->cpu->memory;
	debugger->currentBreakpoint = 0;
	DebugBreakpointListInit(&debugger->breakpoints, 0);
	DebugBreakpointListInit(&debugger->swBreakpoints, 0);
	DebugWatchpointListInit(&debugger->watchpoints, 0);
	if (debugger->init) {
		debugger->init(debugger);
	}
}

void DebuggerDeinit(struct mCPUComponent* component) {
	struct Debugger* debugger = (struct Debugger*) component;
	debugger->deinit(debugger);
	DebugBreakpointListDeinit(&debugger->breakpoints);
	DebugBreakpointListDeinit(&debugger->swBreakpoints);
	DebugWatchpointListDeinit(&debugger->watchpoints);
}

void DebuggerRun(struct Debugger* debugger) {
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

void DebuggerEnter(struct Debugger* debugger, enum DebuggerEntryReason reason, struct DebuggerEntryInfo* info) {
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

void DebuggerSetBreakpoint(struct Debugger* debugger, uint32_t address) {
	struct DebugBreakpoint* breakpoint = DebugBreakpointListAppend(&debugger->breakpoints);
	breakpoint->address = address;
	breakpoint->isSw = false;
}

bool DebuggerSetSoftwareBreakpoint(struct Debugger* debugger, uint32_t address, enum ExecutionMode mode) {
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

void DebuggerClearBreakpoint(struct Debugger* debugger, uint32_t address) {
	struct DebugBreakpointList* breakpoints = &debugger->breakpoints;
	size_t i;
	for (i = 0; i < DebugBreakpointListSize(breakpoints); ++i) {
		if (DebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			DebugBreakpointListShift(breakpoints, i, 1);
		}
	}

}

void DebuggerSetWatchpoint(struct Debugger* debugger, uint32_t address, enum WatchpointType type) {
	if (!DebugWatchpointListSize(&debugger->watchpoints)) {
		DebuggerInstallMemoryShim(debugger);
	}
	struct DebugWatchpoint* watchpoint = DebugWatchpointListAppend(&debugger->watchpoints);
	watchpoint->address = address;
	watchpoint->type = type;
}

void DebuggerClearWatchpoint(struct Debugger* debugger, uint32_t address) {
	struct DebugWatchpointList* watchpoints = &debugger->watchpoints;
	size_t i;
	for (i = 0; i < DebugWatchpointListSize(watchpoints); ++i) {
		if (DebugWatchpointListGetPointer(watchpoints, i)->address == address) {
			DebugWatchpointListShift(watchpoints, i, 1);
		}
	}
	if (!DebugWatchpointListSize(&debugger->watchpoints)) {
		DebuggerRemoveMemoryShim(debugger);
	}
}
