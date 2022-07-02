/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/sm83/debugger/debugger.h>

#include <mgba/core/core.h>
#include <mgba/internal/debugger/parser.h>
#include <mgba/internal/sm83/decoder.h>
#include <mgba/internal/sm83/sm83.h>
#include <mgba/internal/sm83/debugger/memory-debugger.h>

static struct mBreakpoint* _lookupBreakpoint(struct mBreakpointList* breakpoints, struct SM83Core* cpu) {
	size_t i;
	for (i = 0; i < mBreakpointListSize(breakpoints); ++i) {
		struct mBreakpoint* breakpoint = mBreakpointListGetPointer(breakpoints, i);
		if (breakpoint->address != cpu->pc) {
			continue;
		}
		if (breakpoint->segment < 0 || breakpoint->segment == cpu->memory.currentSegment(cpu, breakpoint->address)) {
			return breakpoint;
		}
	}
	return NULL;
}

static void _destroyBreakpoint(struct mBreakpoint* breakpoint) {
	if (breakpoint->condition) {
		parseFree(breakpoint->condition);
	}
}

static void _destroyWatchpoint(struct mWatchpoint* watchpoint) {
	if (watchpoint->condition) {
		parseFree(watchpoint->condition);
	}
}

static void SM83DebuggerCheckBreakpoints(struct mDebuggerPlatform* d) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) d;
	struct mBreakpoint* breakpoint = _lookupBreakpoint(&debugger->breakpoints, debugger->cpu);
	if (!breakpoint) {
		return;
	}
	if (breakpoint->condition) {
		int32_t value;
		int segment;
		if (!mDebuggerEvaluateParseTree(d->p, breakpoint->condition, &value, &segment) || !(value || segment >= 0)) {
			return;
		}
	}
	struct mDebuggerEntryInfo info = {
		.address = breakpoint->address,
		.pointId = breakpoint->id
	};
	mDebuggerEnter(d->p, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void SM83DebuggerInit(void* cpu, struct mDebuggerPlatform* platform);
static void SM83DebuggerDeinit(struct mDebuggerPlatform* platform);

static void SM83DebuggerEnter(struct mDebuggerPlatform* d, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info);

static ssize_t SM83DebuggerSetBreakpoint(struct mDebuggerPlatform*, const struct mBreakpoint*);
static void SM83DebuggerListBreakpoints(struct mDebuggerPlatform*, struct mBreakpointList*);
static bool SM83DebuggerClearBreakpoint(struct mDebuggerPlatform*, ssize_t id);
static ssize_t SM83DebuggerSetWatchpoint(struct mDebuggerPlatform*, const struct mWatchpoint*);
static void SM83DebuggerListWatchpoints(struct mDebuggerPlatform*, struct mWatchpointList*);
static void SM83DebuggerCheckBreakpoints(struct mDebuggerPlatform*);
static bool SM83DebuggerHasBreakpoints(struct mDebuggerPlatform*);
static void SM83DebuggerTrace(struct mDebuggerPlatform*, char* out, size_t* length);

struct mDebuggerPlatform* SM83DebuggerPlatformCreate(void) {
	struct SM83Debugger* platform = malloc(sizeof(struct SM83Debugger));
	platform->d.entered = SM83DebuggerEnter;
	platform->d.init = SM83DebuggerInit;
	platform->d.deinit = SM83DebuggerDeinit;
	platform->d.setBreakpoint = SM83DebuggerSetBreakpoint;
	platform->d.listBreakpoints = SM83DebuggerListBreakpoints;
	platform->d.clearBreakpoint = SM83DebuggerClearBreakpoint;
	platform->d.setWatchpoint = SM83DebuggerSetWatchpoint;
	platform->d.listWatchpoints = SM83DebuggerListWatchpoints;
	platform->d.checkBreakpoints = SM83DebuggerCheckBreakpoints;
	platform->d.hasBreakpoints = SM83DebuggerHasBreakpoints;
	platform->d.trace = SM83DebuggerTrace;
	platform->d.getStackTraceMode = NULL;
	platform->d.setStackTraceMode = NULL;
	platform->d.updateStackTrace = NULL;
	platform->printStatus = NULL;
	return &platform->d;
}

void SM83DebuggerInit(void* cpu, struct mDebuggerPlatform* platform) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) platform;
	debugger->cpu = cpu;
	debugger->originalMemory = debugger->cpu->memory;
	mBreakpointListInit(&debugger->breakpoints, 0);
	mWatchpointListInit(&debugger->watchpoints, 0);
	debugger->nextId = 1;
}

void SM83DebuggerDeinit(struct mDebuggerPlatform* platform) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) platform;
	size_t i;
	for (i = 0; i < mBreakpointListSize(&debugger->breakpoints); ++i) {
		_destroyBreakpoint(mBreakpointListGetPointer(&debugger->breakpoints, i));
	}
	mBreakpointListDeinit(&debugger->breakpoints);

	for (i = 0; i < mWatchpointListSize(&debugger->watchpoints); ++i) {
		_destroyWatchpoint(mWatchpointListGetPointer(&debugger->watchpoints, i));
	}
	mWatchpointListDeinit(&debugger->watchpoints);
}

static void SM83DebuggerEnter(struct mDebuggerPlatform* platform, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	UNUSED(reason);
	UNUSED(info);
	struct SM83Debugger* debugger = (struct SM83Debugger*) platform;
	struct SM83Core* cpu = debugger->cpu;
	cpu->nextEvent = cpu->cycles;

	if (debugger->d.p->entered) {
		debugger->d.p->entered(debugger->d.p, reason, info);
	}
}

static ssize_t SM83DebuggerSetBreakpoint(struct mDebuggerPlatform* d, const struct mBreakpoint* info) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) d;
	struct mBreakpoint* breakpoint = mBreakpointListAppend(&debugger->breakpoints);
	*breakpoint = *info;
	breakpoint->id = debugger->nextId;
	++debugger->nextId;
	return breakpoint->id;

}

static bool SM83DebuggerClearBreakpoint(struct mDebuggerPlatform* d, ssize_t id) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) d;
	size_t i;

	struct mBreakpointList* breakpoints = &debugger->breakpoints;
	for (i = 0; i < mBreakpointListSize(breakpoints); ++i) {
		struct mBreakpoint* breakpoint = mBreakpointListGetPointer(breakpoints, i);
		if (breakpoint->id == id) {
			_destroyBreakpoint(breakpoint);
			mBreakpointListShift(breakpoints, i, 1);
			return true;
		}
	}

	struct mWatchpointList* watchpoints = &debugger->watchpoints;
	for (i = 0; i < mWatchpointListSize(watchpoints); ++i) {
		struct mWatchpoint* watchpoint = mWatchpointListGetPointer(watchpoints, i);
		if (watchpoint->id == id) {
			_destroyWatchpoint(watchpoint);
			mWatchpointListShift(watchpoints, i, 1);
			if (!mWatchpointListSize(&debugger->watchpoints)) {
				SM83DebuggerRemoveMemoryShim(debugger);
			}
			return true;
		}
	}
	return false;
}

static bool SM83DebuggerHasBreakpoints(struct mDebuggerPlatform* d) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) d;
	return mBreakpointListSize(&debugger->breakpoints) || mWatchpointListSize(&debugger->watchpoints);
}

static ssize_t SM83DebuggerSetWatchpoint(struct mDebuggerPlatform* d, const struct mWatchpoint* info) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) d;
	if (!mWatchpointListSize(&debugger->watchpoints)) {
		SM83DebuggerInstallMemoryShim(debugger);
	}
	struct mWatchpoint* watchpoint = mWatchpointListAppend(&debugger->watchpoints);
	*watchpoint = *info;
	watchpoint->id = debugger->nextId;
	++debugger->nextId;
	return watchpoint->id;
}

static void SM83DebuggerListBreakpoints(struct mDebuggerPlatform* d, struct mBreakpointList* list) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) d;
	mBreakpointListClear(list);
	mBreakpointListCopy(list, &debugger->breakpoints);
}

static void SM83DebuggerListWatchpoints(struct mDebuggerPlatform* d, struct mWatchpointList* list) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) d;
	mWatchpointListClear(list);
	mWatchpointListCopy(list, &debugger->watchpoints);
}

static void SM83DebuggerTrace(struct mDebuggerPlatform* d, char* out, size_t* length) {
	struct SM83Debugger* debugger = (struct SM83Debugger*) d;
	struct SM83Core* cpu = debugger->cpu;

	char disassembly[64];

	struct SM83InstructionInfo info = {{0}};
	char* disPtr = disassembly;
	uint8_t instruction;
	uint16_t address = cpu->pc;
	size_t bytesRemaining = 1;
	for (bytesRemaining = 1; bytesRemaining; --bytesRemaining) {
		instruction = debugger->d.p->core->rawRead8(debugger->d.p->core, address, -1);
		disPtr += snprintf(disPtr, sizeof(disassembly) - (disPtr - disassembly), "%02X", instruction);
		++address;
		bytesRemaining += SM83Decode(instruction, &info);
	};
	disPtr[0] = ':';
	disPtr[1] = ' ';
	disPtr += 2;
	SM83Disassemble(&info, address, disPtr, sizeof(disassembly) - (disPtr - disassembly));

	*length = snprintf(out, *length, "A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X SP: %04X PC: %02X:%04X | %s",
		               cpu->a, cpu->f.packed, cpu->b, cpu->c,
		               cpu->d, cpu->e, cpu->h, cpu->l,
		               cpu->sp, cpu->memory.currentSegment(cpu, cpu->pc), cpu->pc, disassembly);
}
