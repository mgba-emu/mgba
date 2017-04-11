/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/arm/debugger/debugger.h>

#include <mgba/core/core.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/arm/debugger/memory-debugger.h>

DEFINE_VECTOR(ARMDebugBreakpointList, struct ARMDebugBreakpoint);
DEFINE_VECTOR(ARMDebugWatchpointList, struct ARMDebugWatchpoint);

static struct ARMDebugBreakpoint* _lookupBreakpoint(struct ARMDebugBreakpointList* breakpoints, uint32_t address) {
	size_t i;
	for (i = 0; i < ARMDebugBreakpointListSize(breakpoints); ++i) {
		if (ARMDebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			return ARMDebugBreakpointListGetPointer(breakpoints, i);
		}
	}
	return 0;
}

static void ARMDebuggerCheckBreakpoints(struct mDebuggerPlatform* d) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	int instructionLength;
	enum ExecutionMode mode = debugger->cpu->cpsr.t;
	if (mode == MODE_ARM) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	struct ARMDebugBreakpoint* breakpoint = _lookupBreakpoint(&debugger->breakpoints, debugger->cpu->gprs[ARM_PC] - instructionLength);
	if (!breakpoint) {
		return;
	}
	struct mDebuggerEntryInfo info = {
		.address = breakpoint->address,
		.breakType = BREAKPOINT_HARDWARE
	};
	mDebuggerEnter(d->p, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void ARMDebuggerInit(void* cpu, struct mDebuggerPlatform* platform);
static void ARMDebuggerDeinit(struct mDebuggerPlatform* platform);

static void ARMDebuggerEnter(struct mDebuggerPlatform* d, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info);

static void ARMDebuggerSetBreakpoint(struct mDebuggerPlatform*, uint32_t address);
static void ARMDebuggerClearBreakpoint(struct mDebuggerPlatform*, uint32_t address);
static void ARMDebuggerSetWatchpoint(struct mDebuggerPlatform*, uint32_t address, enum mWatchpointType type);
static void ARMDebuggerClearWatchpoint(struct mDebuggerPlatform*, uint32_t address);
static void ARMDebuggerCheckBreakpoints(struct mDebuggerPlatform*);
static bool ARMDebuggerHasBreakpoints(struct mDebuggerPlatform*);

struct mDebuggerPlatform* ARMDebuggerPlatformCreate(void) {
	struct mDebuggerPlatform* platform = (struct mDebuggerPlatform*) malloc(sizeof(struct ARMDebugger));
	platform->entered = ARMDebuggerEnter;
	platform->init = ARMDebuggerInit;
	platform->deinit = ARMDebuggerDeinit;
	platform->setBreakpoint = ARMDebuggerSetBreakpoint;
	platform->clearBreakpoint = ARMDebuggerClearBreakpoint;
	platform->setWatchpoint = ARMDebuggerSetWatchpoint;
	platform->clearWatchpoint = ARMDebuggerClearWatchpoint;
	platform->checkBreakpoints = ARMDebuggerCheckBreakpoints;
	platform->hasBreakpoints = ARMDebuggerHasBreakpoints;
	return platform;
}

void ARMDebuggerInit(void* cpu, struct mDebuggerPlatform* platform) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) platform;
	debugger->cpu = cpu;
	debugger->originalMemory = debugger->cpu->memory;
	ARMDebugBreakpointListInit(&debugger->breakpoints, 0);
	ARMDebugBreakpointListInit(&debugger->swBreakpoints, 0);
	ARMDebugWatchpointListInit(&debugger->watchpoints, 0);
}

void ARMDebuggerDeinit(struct mDebuggerPlatform* platform) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) platform;
	if (debugger->clearSoftwareBreakpoint) {
		// Clear the stack backwards in case any overlap
		size_t b;
		for (b = ARMDebugBreakpointListSize(&debugger->swBreakpoints); b; --b) {
			struct ARMDebugBreakpoint* breakpoint = ARMDebugBreakpointListGetPointer(&debugger->swBreakpoints, b - 1);
			debugger->clearSoftwareBreakpoint(debugger, breakpoint->address, breakpoint->sw.mode, breakpoint->sw.opcode);
		}
	}
	ARMDebuggerRemoveMemoryShim(debugger);

	ARMDebugBreakpointListDeinit(&debugger->breakpoints);
	ARMDebugBreakpointListDeinit(&debugger->swBreakpoints);
	ARMDebugWatchpointListDeinit(&debugger->watchpoints);
}

static void ARMDebuggerEnter(struct mDebuggerPlatform* platform, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) platform;
	struct ARMCore* cpu = debugger->cpu;
	cpu->nextEvent = cpu->cycles;
	if (reason == DEBUGGER_ENTER_BREAKPOINT) {
		struct ARMDebugBreakpoint* breakpoint = _lookupBreakpoint(&debugger->swBreakpoints, _ARMPCAddress(cpu));
		if (breakpoint && breakpoint->isSw) {
			info->address = breakpoint->address;
			if (debugger->clearSoftwareBreakpoint) {
				debugger->clearSoftwareBreakpoint(debugger, breakpoint->address, breakpoint->sw.mode, breakpoint->sw.opcode);
			}

			ARMRunFake(cpu, breakpoint->sw.opcode);

			if (debugger->setSoftwareBreakpoint) {
				debugger->setSoftwareBreakpoint(debugger, breakpoint->address, breakpoint->sw.mode, &breakpoint->sw.opcode);
			}
		}
	}
	if (debugger->d.p->entered) {
		debugger->d.p->entered(debugger->d.p, reason, info);
	}
}

bool ARMDebuggerSetSoftwareBreakpoint(struct mDebuggerPlatform* d, uint32_t address, enum ExecutionMode mode) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	uint32_t opcode;
	if (!debugger->setSoftwareBreakpoint || !debugger->setSoftwareBreakpoint(debugger, address, mode, &opcode)) {
		return false;
	}

	if (address >= 0x3FFF) {
		mLOG(DEBUGGER, WARN, "Software breakpoints are not supported in the BIOS region. Ignoring software breakpoint at 0x%X.", address);
	}

	struct ARMDebugBreakpoint* breakpoint = ARMDebugBreakpointListAppend(&debugger->swBreakpoints);
	breakpoint->address = address;
	breakpoint->isSw = true;
	breakpoint->sw.opcode = opcode;
	breakpoint->sw.mode = mode;

	return true;
}

void ARMDebuggerClearSoftwareBreakpoint(struct mDebuggerPlatform* d, uint32_t address) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	if (!debugger->clearSoftwareBreakpoint) {
		return;
	}

	struct ARMDebugBreakpoint* breakpoint = NULL;
	// Clear the stack backwards in case any overlap
	size_t b;
	for (b = ARMDebugBreakpointListSize(&debugger->swBreakpoints); b; --b) {
		breakpoint = ARMDebugBreakpointListGetPointer(&debugger->swBreakpoints, b - 1);
		if (breakpoint->address == address) {
			break;
		}
		breakpoint = NULL;
	}

	if (breakpoint) {
		debugger->clearSoftwareBreakpoint(debugger, address, breakpoint->sw.mode, breakpoint->sw.opcode);
	}
}

static void ARMDebuggerSetBreakpoint(struct mDebuggerPlatform* d, uint32_t address) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMDebugBreakpoint* breakpoint = ARMDebugBreakpointListAppend(&debugger->breakpoints);
	breakpoint->address = address;
	breakpoint->isSw = false;
}

static void ARMDebuggerClearBreakpoint(struct mDebuggerPlatform* d, uint32_t address) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMDebugBreakpointList* breakpoints = &debugger->breakpoints;
	size_t i;
	for (i = 0; i < ARMDebugBreakpointListSize(breakpoints); ++i) {
		if (ARMDebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			ARMDebugBreakpointListShift(breakpoints, i, 1);
		}
	}
}

static bool ARMDebuggerHasBreakpoints(struct mDebuggerPlatform* d) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	return ARMDebugBreakpointListSize(&debugger->breakpoints) || ARMDebugWatchpointListSize(&debugger->watchpoints);
}

static void ARMDebuggerSetWatchpoint(struct mDebuggerPlatform* d, uint32_t address, enum mWatchpointType type) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	if (!ARMDebugWatchpointListSize(&debugger->watchpoints)) {
		ARMDebuggerInstallMemoryShim(debugger);
	}
	struct ARMDebugWatchpoint* watchpoint = ARMDebugWatchpointListAppend(&debugger->watchpoints);
	watchpoint->address = address;
	watchpoint->type = type;
}

static void ARMDebuggerClearWatchpoint(struct mDebuggerPlatform* d, uint32_t address) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMDebugWatchpointList* watchpoints = &debugger->watchpoints;
	size_t i;
	for (i = 0; i < ARMDebugWatchpointListSize(watchpoints); ++i) {
		if (ARMDebugWatchpointListGetPointer(watchpoints, i)->address == address) {
			ARMDebugWatchpointListShift(watchpoints, i, 1);
		}
	}
	if (!ARMDebugWatchpointListSize(&debugger->watchpoints)) {
		ARMDebuggerRemoveMemoryShim(debugger);
	}
}
