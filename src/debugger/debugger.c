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

static struct DebugBreakpoint* _lookupBreakpoint(struct DebugBreakpoint* breakpoints, uint32_t address) {
	for (; breakpoints; breakpoints = breakpoints->next) {
		if (breakpoints->address == address) {
			return breakpoints;
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
	struct DebugBreakpoint* breakpoint = _lookupBreakpoint(debugger->breakpoints, debugger->cpu->gprs[ARM_PC] - instructionLength);
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
	debugger->breakpoints = 0;
	debugger->swBreakpoints = 0;
	debugger->originalMemory = cpu->memory;
	debugger->watchpoints = 0;
	debugger->currentBreakpoint = 0;
	if (debugger->init) {
		debugger->init(debugger);
	}
}

void ARMDebuggerDeinit(struct ARMComponent* component) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) component;
	debugger->deinit(debugger);
}

void ARMDebuggerRun(struct ARMDebugger* debugger) {
	switch (debugger->state) {
	case DEBUGGER_RUNNING:
		if (!debugger->breakpoints && !debugger->watchpoints) {
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
		struct DebugBreakpoint* breakpoint = _lookupBreakpoint(debugger->swBreakpoints, _ARMPCAddress(cpu));
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
	struct DebugBreakpoint* breakpoint = malloc(sizeof(struct DebugBreakpoint));
	breakpoint->address = address;
	breakpoint->next = debugger->breakpoints;
	breakpoint->isSw = false;
	debugger->breakpoints = breakpoint;
}

bool ARMDebuggerSetSoftwareBreakpoint(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	uint32_t opcode;
	if (!debugger->setSoftwareBreakpoint || !debugger->setSoftwareBreakpoint(debugger, address, mode, &opcode)) {
		return false;
	}

	struct DebugBreakpoint* breakpoint = malloc(sizeof(struct DebugBreakpoint));
	breakpoint->address = address;
	breakpoint->next = debugger->swBreakpoints;
	breakpoint->isSw = true;
	breakpoint->sw.opcode = opcode;
	breakpoint->sw.mode = mode;
	debugger->swBreakpoints = breakpoint;

	return true;
}

void ARMDebuggerClearBreakpoint(struct ARMDebugger* debugger, uint32_t address) {
	struct DebugBreakpoint** previous = &debugger->breakpoints;
	struct DebugBreakpoint* breakpoint;
	struct DebugBreakpoint** next;
	while ((breakpoint = *previous)) {
		next = &breakpoint->next;
		if (breakpoint->address == address) {
			*previous = *next;
			free(breakpoint);
			continue;
		}
		previous = next;
	}
}

void ARMDebuggerSetWatchpoint(struct ARMDebugger* debugger, uint32_t address) {
	if (!debugger->watchpoints) {
		ARMDebuggerInstallMemoryShim(debugger);
	}
	struct DebugWatchpoint* watchpoint = malloc(sizeof(struct DebugWatchpoint));
	watchpoint->address = address;
	watchpoint->next = debugger->watchpoints;
	debugger->watchpoints = watchpoint;
}

void ARMDebuggerClearWatchpoint(struct ARMDebugger* debugger, uint32_t address) {
	struct DebugWatchpoint** previous = &debugger->watchpoints;
	struct DebugWatchpoint* watchpoint;
	struct DebugWatchpoint** next;
	while ((watchpoint = *previous)) {
		next = &watchpoint->next;
		if (watchpoint->address == address) {
			*previous = *next;
			free(watchpoint);
			continue;
		}
		previous = next;
	}
	if (!debugger->watchpoints) {
		ARMDebuggerRemoveMemoryShim(debugger);
	}
}
