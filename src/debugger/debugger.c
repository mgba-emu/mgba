/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "debugger.h"

#include "arm.h"

#include "memory-debugger.h"

const uint32_t ARM_DEBUGGER_ID = 0xDEADBEEF;

static void _checkBreakpoints(struct ARMDebugger* debugger) {
	struct DebugBreakpoint* breakpoint;
	int instructionLength;
	enum ExecutionMode mode = debugger->cpu->cpsr.t;
	if (mode == MODE_ARM) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	for (breakpoint = debugger->breakpoints; breakpoint; breakpoint = breakpoint->next) {
		if (breakpoint->address + instructionLength == (uint32_t) debugger->cpu->gprs[ARM_PC]) {
			ARMDebuggerEnter(debugger, DEBUGGER_ENTER_BREAKPOINT);
			break;
		}
	}
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
	debugger->originalMemory = cpu->memory;
	debugger->watchpoints = 0;
	if (debugger->init) {
		debugger->init(debugger);
	}
}

void ARMDebuggerDeinit(struct ARMComponent* component) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) component;
	debugger->deinit(debugger);
}

void ARMDebuggerRun(struct ARMDebugger* debugger) {
	if (debugger->state == DEBUGGER_EXITING) {
		debugger->state = DEBUGGER_RUNNING;
	}
	while (debugger->state < DEBUGGER_EXITING) {
		if (!debugger->breakpoints && !debugger->watchpoints) {
			while (debugger->state == DEBUGGER_RUNNING) {
				ARMRun(debugger->cpu);
			}
		} else if (!debugger->breakpoints) {
			while (debugger->state == DEBUGGER_RUNNING) {
				ARMRun(debugger->cpu);
			}
		} else {
			while (debugger->state == DEBUGGER_RUNNING) {
				ARMRun(debugger->cpu);
				_checkBreakpoints(debugger);
			}
		}
		switch (debugger->state) {
		case DEBUGGER_RUNNING:
			break;
		case DEBUGGER_PAUSED:
			if (debugger->paused) {
				debugger->paused(debugger);
			} else {
				debugger->state = DEBUGGER_RUNNING;
			}
			break;
		case DEBUGGER_EXITING:
		case DEBUGGER_SHUTDOWN:
			return;
		}
	}
}

void ARMDebuggerEnter(struct ARMDebugger* debugger, enum DebuggerEntryReason reason) {
	debugger->state = DEBUGGER_PAUSED;
	if (debugger->entered) {
		debugger->entered(debugger, reason);
	}
}

void ARMDebuggerSetBreakpoint(struct ARMDebugger* debugger, uint32_t address) {
	struct DebugBreakpoint* breakpoint = malloc(sizeof(struct DebugBreakpoint));
	breakpoint->address = address;
	breakpoint->next = debugger->breakpoints;
	debugger->breakpoints = breakpoint;
}

void ARMDebuggerClearBreakpoint(struct ARMDebugger* debugger, uint32_t address) {
	struct DebugBreakpoint** previous = &debugger->breakpoints;
	struct DebugBreakpoint* breakpoint;
	for (; (breakpoint = *previous); previous = &breakpoint->next) {
		if (breakpoint->address == address) {
			*previous = breakpoint->next;
			free(breakpoint);
		}
	}
}

void ARMDebuggerSetWatchpoint(struct ARMDebugger* debugger, uint32_t address) {
	if (!debugger->watchpoints) {
		ARMDebuggerInstallMemoryShim(debugger);
	}
	struct DebugBreakpoint* watchpoint = malloc(sizeof(struct DebugBreakpoint));
	watchpoint->address = address;
	watchpoint->next = debugger->watchpoints;
	debugger->watchpoints = watchpoint;
}

void ARMDebuggerClearWatchpoint(struct ARMDebugger* debugger, uint32_t address) {
	struct DebugBreakpoint** previous = &debugger->watchpoints;
	struct DebugBreakpoint* breakpoint;
	for (; (breakpoint = *previous); previous = &breakpoint->next) {
		if (breakpoint->address == address) {
			*previous = breakpoint->next;
			free(breakpoint);
		}
	}
	if (!debugger->watchpoints) {
		ARMDebuggerRemoveMemoryShim(debugger);
	}
}
