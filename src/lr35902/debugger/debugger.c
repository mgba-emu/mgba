/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/lr35902/debugger/debugger.h>

#include <mgba/core/core.h>
#include <mgba/internal/lr35902/lr35902.h>
#include <mgba/internal/lr35902/debugger/memory-debugger.h>

DEFINE_VECTOR(LR35902DebugBreakpointList, struct LR35902DebugBreakpoint);
DEFINE_VECTOR(LR35902DebugWatchpointList, struct LR35902DebugWatchpoint);

static struct LR35902DebugBreakpoint* _lookupBreakpoint(struct LR35902DebugBreakpointList* breakpoints, uint16_t address) {
	size_t i;
	for (i = 0; i < LR35902DebugBreakpointListSize(breakpoints); ++i) {
		if (LR35902DebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			return LR35902DebugBreakpointListGetPointer(breakpoints, i);
		}
	}
	return 0;
}

static void LR35902DebuggerCheckBreakpoints(struct mDebuggerPlatform* d) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902DebugBreakpoint* breakpoint = _lookupBreakpoint(&debugger->breakpoints, debugger->cpu->pc);
	if (!breakpoint) {
		return;
	}
	if (breakpoint->segment >= 0 && debugger->cpu->memory.currentSegment(debugger->cpu, breakpoint->address) != breakpoint->segment) {
		return;
	}
	struct mDebuggerEntryInfo info = {
		.address = breakpoint->address
	};
	mDebuggerEnter(d->p, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void LR35902DebuggerInit(void* cpu, struct mDebuggerPlatform* platform);
static void LR35902DebuggerDeinit(struct mDebuggerPlatform* platform);

static void LR35902DebuggerEnter(struct mDebuggerPlatform* d, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info);

static void LR35902DebuggerSetBreakpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
static void LR35902DebuggerClearBreakpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
static void LR35902DebuggerSetWatchpoint(struct mDebuggerPlatform*, uint32_t address, int segment, enum mWatchpointType type);
static void LR35902DebuggerClearWatchpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
static void LR35902DebuggerCheckBreakpoints(struct mDebuggerPlatform*);
static bool LR35902DebuggerHasBreakpoints(struct mDebuggerPlatform*);

struct mDebuggerPlatform* LR35902DebuggerPlatformCreate(void) {
	struct mDebuggerPlatform* platform = (struct mDebuggerPlatform*) malloc(sizeof(struct LR35902Debugger));
	platform->entered = LR35902DebuggerEnter;
	platform->init = LR35902DebuggerInit;
	platform->deinit = LR35902DebuggerDeinit;
	platform->setBreakpoint = LR35902DebuggerSetBreakpoint;
	platform->clearBreakpoint = LR35902DebuggerClearBreakpoint;
	platform->setWatchpoint = LR35902DebuggerSetWatchpoint;
	platform->clearWatchpoint = LR35902DebuggerClearWatchpoint;
	platform->checkBreakpoints = LR35902DebuggerCheckBreakpoints;
	platform->hasBreakpoints = LR35902DebuggerHasBreakpoints;
	return platform;
}

void LR35902DebuggerInit(void* cpu, struct mDebuggerPlatform* platform) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) platform;
	debugger->cpu = cpu;
	LR35902DebugBreakpointListInit(&debugger->breakpoints, 0);
	LR35902DebugWatchpointListInit(&debugger->watchpoints, 0);
}

void LR35902DebuggerDeinit(struct mDebuggerPlatform* platform) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) platform;
	LR35902DebugBreakpointListDeinit(&debugger->breakpoints);
	LR35902DebugWatchpointListDeinit(&debugger->watchpoints);
}

static void LR35902DebuggerEnter(struct mDebuggerPlatform* platform, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	UNUSED(reason);
	UNUSED(info);
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) platform;
	struct LR35902Core* cpu = debugger->cpu;
	cpu->nextEvent = cpu->cycles;

	if (debugger->d.p->entered) {
		debugger->d.p->entered(debugger->d.p, reason, info);
	}
}

static void LR35902DebuggerSetBreakpoint(struct mDebuggerPlatform* d, uint32_t address, int segment) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902DebugBreakpoint* breakpoint = LR35902DebugBreakpointListAppend(&debugger->breakpoints);
	breakpoint->address = address;
	breakpoint->segment = segment;
}

static void LR35902DebuggerClearBreakpoint(struct mDebuggerPlatform* d, uint32_t address, int segment) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902DebugBreakpointList* breakpoints = &debugger->breakpoints;
	size_t i;
	for (i = 0; i < LR35902DebugBreakpointListSize(breakpoints); ++i) {
		struct LR35902DebugBreakpoint* breakpoint = LR35902DebugBreakpointListGetPointer(breakpoints, i);
		if (breakpoint->address == address && breakpoint->segment == segment) {
			LR35902DebugBreakpointListShift(breakpoints, i, 1);
		}
	}
}

static bool LR35902DebuggerHasBreakpoints(struct mDebuggerPlatform* d) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	return LR35902DebugBreakpointListSize(&debugger->breakpoints) || LR35902DebugWatchpointListSize(&debugger->watchpoints);
}

static void LR35902DebuggerSetWatchpoint(struct mDebuggerPlatform* d, uint32_t address, int segment, enum mWatchpointType type) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	if (!LR35902DebugWatchpointListSize(&debugger->watchpoints)) {
		LR35902DebuggerInstallMemoryShim(debugger);
	}
	struct LR35902DebugWatchpoint* watchpoint = LR35902DebugWatchpointListAppend(&debugger->watchpoints);
	watchpoint->address = address;
	watchpoint->type = type;
	watchpoint->segment = segment;
}

static void LR35902DebuggerClearWatchpoint(struct mDebuggerPlatform* d, uint32_t address, int segment) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902DebugWatchpointList* watchpoints = &debugger->watchpoints;
	size_t i;
	for (i = 0; i < LR35902DebugWatchpointListSize(watchpoints); ++i) {
		struct LR35902DebugWatchpoint* watchpoint = LR35902DebugWatchpointListGetPointer(watchpoints, i);
		if (watchpoint->address == address && watchpoint->segment == segment) {
			LR35902DebugWatchpointListShift(watchpoints, i, 1);
		}
	}
	if (!LR35902DebugWatchpointListSize(&debugger->watchpoints)) {
		LR35902DebuggerRemoveMemoryShim(debugger);
	}
}
