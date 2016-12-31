/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/lr35902/debugger/debugger.h>

#include <mgba/core/core.h>
#include <mgba/internal/lr35902/lr35902.h>

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
	// TODO: Segments
	struct mDebuggerEntryInfo info = {
		.address = breakpoint->address
	};
	mDebuggerEnter(d->p, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void LR35902DebuggerInit(void* cpu, struct mDebuggerPlatform* platform);
static void LR35902DebuggerDeinit(struct mDebuggerPlatform* platform);

static void LR35902DebuggerEnter(struct mDebuggerPlatform* d, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info);

static void LR35902DebuggerSetBreakpoint(struct mDebuggerPlatform*, uint32_t address);
static void LR35902DebuggerClearBreakpoint(struct mDebuggerPlatform*, uint32_t address);
static void LR35902DebuggerCheckBreakpoints(struct mDebuggerPlatform*);
static bool LR35902DebuggerHasBreakpoints(struct mDebuggerPlatform*);

struct mDebuggerPlatform* LR35902DebuggerPlatformCreate(void) {
	struct mDebuggerPlatform* platform = (struct mDebuggerPlatform*) malloc(sizeof(struct LR35902Debugger));
	platform->entered = LR35902DebuggerEnter;
	platform->init = LR35902DebuggerInit;
	platform->deinit = LR35902DebuggerDeinit;
	platform->setBreakpoint = LR35902DebuggerSetBreakpoint;
	platform->clearBreakpoint = LR35902DebuggerClearBreakpoint;
	platform->setWatchpoint = NULL;
	platform->clearWatchpoint = NULL;
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
}

static void LR35902DebuggerSetBreakpoint(struct mDebuggerPlatform* d, uint32_t address) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902DebugBreakpoint* breakpoint = LR35902DebugBreakpointListAppend(&debugger->breakpoints);
	breakpoint->address = address;
	breakpoint->segment = -1;
}

static void LR35902DebuggerClearBreakpoint(struct mDebuggerPlatform* d, uint32_t address) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902DebugBreakpointList* breakpoints = &debugger->breakpoints;
	size_t i;
	for (i = 0; i < LR35902DebugBreakpointListSize(breakpoints); ++i) {
		if (LR35902DebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			LR35902DebugBreakpointListShift(breakpoints, i, 1);
		}
	}
}

static bool LR35902DebuggerHasBreakpoints(struct mDebuggerPlatform* d) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	return LR35902DebugBreakpointListSize(&debugger->breakpoints) || LR35902DebugWatchpointListSize(&debugger->watchpoints);
}
