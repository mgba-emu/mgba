/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/lr35902/debugger/debugger.h>

#include <mgba/core/core.h>
#include <mgba/internal/debugger/parser.h>
#include <mgba/internal/lr35902/decoder.h>
#include <mgba/internal/lr35902/lr35902.h>
#include <mgba/internal/lr35902/debugger/memory-debugger.h>

static struct mBreakpoint* _lookupBreakpoint(struct mBreakpointList* breakpoints, struct LR35902Core* cpu) {
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
		free(breakpoint->condition);
	}
}

static void _destroyWatchpoint(struct mWatchpoint* watchpoint) {
	if (watchpoint->condition) {
		parseFree(watchpoint->condition);
		free(watchpoint->condition);
	}
}

static void LR35902DebuggerCheckBreakpoints(struct mDebuggerPlatform* d) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
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
		.address = breakpoint->address
	};
	mDebuggerEnter(d->p, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void LR35902DebuggerInit(void* cpu, struct mDebuggerPlatform* platform);
static void LR35902DebuggerDeinit(struct mDebuggerPlatform* platform);

static void LR35902DebuggerEnter(struct mDebuggerPlatform* d, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info);

static ssize_t LR35902DebuggerSetBreakpoint(struct mDebuggerPlatform*, const struct mBreakpoint*);
static void LR35902DebuggerListBreakpoints(struct mDebuggerPlatform*, struct mBreakpointList*);
static bool LR35902DebuggerClearBreakpoint(struct mDebuggerPlatform*, ssize_t id);
static ssize_t LR35902DebuggerSetWatchpoint(struct mDebuggerPlatform*, const struct mWatchpoint*);
static void LR35902DebuggerListWatchpoints(struct mDebuggerPlatform*, struct mWatchpointList*);
static void LR35902DebuggerCheckBreakpoints(struct mDebuggerPlatform*);
static bool LR35902DebuggerHasBreakpoints(struct mDebuggerPlatform*);
static void LR35902DebuggerTrace(struct mDebuggerPlatform*, char* out, size_t* length);
static bool LR35902DebuggerGetRegister(struct mDebuggerPlatform*, const char* name, int32_t* value);
static bool LR35902DebuggerSetRegister(struct mDebuggerPlatform*, const char* name, int32_t value);

struct mDebuggerPlatform* LR35902DebuggerPlatformCreate(void) {
	struct mDebuggerPlatform* platform = (struct mDebuggerPlatform*) malloc(sizeof(struct LR35902Debugger));
	platform->entered = LR35902DebuggerEnter;
	platform->init = LR35902DebuggerInit;
	platform->deinit = LR35902DebuggerDeinit;
	platform->setBreakpoint = LR35902DebuggerSetBreakpoint;
	platform->listBreakpoints = LR35902DebuggerListBreakpoints;
	platform->clearBreakpoint = LR35902DebuggerClearBreakpoint;
	platform->setWatchpoint = LR35902DebuggerSetWatchpoint;
	platform->listWatchpoints = LR35902DebuggerListWatchpoints;
	platform->checkBreakpoints = LR35902DebuggerCheckBreakpoints;
	platform->hasBreakpoints = LR35902DebuggerHasBreakpoints;
	platform->trace = LR35902DebuggerTrace;
	platform->getRegister = LR35902DebuggerGetRegister;
	platform->setRegister = LR35902DebuggerSetRegister;
	return platform;
}

void LR35902DebuggerInit(void* cpu, struct mDebuggerPlatform* platform) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) platform;
	debugger->cpu = cpu;
	debugger->originalMemory = debugger->cpu->memory;
	mBreakpointListInit(&debugger->breakpoints, 0);
	mWatchpointListInit(&debugger->watchpoints, 0);
	debugger->nextId = 1;
}

void LR35902DebuggerDeinit(struct mDebuggerPlatform* platform) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) platform;
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

static ssize_t LR35902DebuggerSetBreakpoint(struct mDebuggerPlatform* d, const struct mBreakpoint* info) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct mBreakpoint* breakpoint = mBreakpointListAppend(&debugger->breakpoints);
	*breakpoint = *info;
	breakpoint->id = debugger->nextId;
	++debugger->nextId;
	return breakpoint->id;

}

static bool LR35902DebuggerClearBreakpoint(struct mDebuggerPlatform* d, ssize_t id) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
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
				LR35902DebuggerRemoveMemoryShim(debugger);
			}
			return true;
		}
	}
	return false;
}

static bool LR35902DebuggerHasBreakpoints(struct mDebuggerPlatform* d) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	return mBreakpointListSize(&debugger->breakpoints) || mWatchpointListSize(&debugger->watchpoints);
}

static ssize_t LR35902DebuggerSetWatchpoint(struct mDebuggerPlatform* d, const struct mWatchpoint* info) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	if (!mWatchpointListSize(&debugger->watchpoints)) {
		LR35902DebuggerInstallMemoryShim(debugger);
	}
	struct mWatchpoint* watchpoint = mWatchpointListAppend(&debugger->watchpoints);
	*watchpoint = *info;
	watchpoint->id = debugger->nextId;
	++debugger->nextId;
	return watchpoint->id;
}

static void LR35902DebuggerListBreakpoints(struct mDebuggerPlatform* d, struct mBreakpointList* list) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	mBreakpointListClear(list);
	mBreakpointListCopy(list, &debugger->breakpoints);
}

static void LR35902DebuggerListWatchpoints(struct mDebuggerPlatform* d, struct mWatchpointList* list) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	mWatchpointListClear(list);
	mWatchpointListCopy(list, &debugger->watchpoints);
}

static void LR35902DebuggerTrace(struct mDebuggerPlatform* d, char* out, size_t* length) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902Core* cpu = debugger->cpu;

	char disassembly[64];

	struct LR35902InstructionInfo info = {{0}};
	char* disPtr = disassembly;
	uint8_t instruction;
	uint16_t address = cpu->pc;
	size_t bytesRemaining = 1;
	for (bytesRemaining = 1; bytesRemaining; --bytesRemaining) {
		instruction = debugger->d.p->core->rawRead8(debugger->d.p->core, address, -1);
		disPtr += snprintf(disPtr, sizeof(disassembly) - (disPtr - disassembly), "%02X", instruction);
		++address;
		bytesRemaining += LR35902Decode(instruction, &info);
	};
	disPtr[0] = ':';
	disPtr[1] = ' ';
	disPtr += 2;
	LR35902Disassemble(&info, address, disPtr, sizeof(disassembly) - (disPtr - disassembly));

	*length = snprintf(out, *length, "A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X SP: %04X PC: %02X:%04X | %s",
		               cpu->a, cpu->f.packed, cpu->b, cpu->c,
		               cpu->d, cpu->e, cpu->h, cpu->l,
		               cpu->sp, cpu->memory.currentSegment(cpu, cpu->pc), cpu->pc, disassembly);
}

bool LR35902DebuggerGetRegister(struct mDebuggerPlatform* d, const char* name, int32_t* value) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902Core* cpu = debugger->cpu;

	if (strcmp(name, "a") == 0) {
		*value = cpu->a;
		return true;
	}
	if (strcmp(name, "b") == 0) {
		*value = cpu->b;
		return true;
	}
	if (strcmp(name, "c") == 0) {
		*value = cpu->c;
		return true;
	}
	if (strcmp(name, "d") == 0) {
		*value = cpu->d;
		return true;
	}
	if (strcmp(name, "e") == 0) {
		*value = cpu->e;
		return true;
	}
	if (strcmp(name, "h") == 0) {
		*value = cpu->h;
		return true;
	}
	if (strcmp(name, "l") == 0) {
		*value = cpu->l;
		return true;
	}
	if (strcmp(name, "bc") == 0) {
		*value = cpu->bc;
		return true;
	}
	if (strcmp(name, "de") == 0) {
		*value = cpu->de;
		return true;
	}
	if (strcmp(name, "hl") == 0) {
		*value = cpu->hl;
		return true;
	}
	if (strcmp(name, "af") == 0) {
		*value = cpu->af;
		return true;
	}
	if (strcmp(name, "pc") == 0) {
		*value = cpu->pc;
		return true;
	}
	if (strcmp(name, "sp") == 0) {
		*value = cpu->sp;
		return true;
	}
	if (strcmp(name, "f") == 0) {
		*value = cpu->f.packed;
		return true;
	}
	return false;
}

bool LR35902DebuggerSetRegister(struct mDebuggerPlatform* d, const char* name, int32_t value) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902Core* cpu = debugger->cpu;

	if (strcmp(name, "a") == 0) {
		cpu->a = value;
		return true;
	}
	if (strcmp(name, "b") == 0) {
		cpu->b = value;
		return true;
	}
	if (strcmp(name, "c") == 0) {
		cpu->c = value;
		return true;
	}
	if (strcmp(name, "d") == 0) {
		cpu->d = value;
		return true;
	}
	if (strcmp(name, "e") == 0) {
		cpu->e = value;
		return true;
	}
	if (strcmp(name, "h") == 0) {
		cpu->h = value;
		return true;
	}
	if (strcmp(name, "l") == 0) {
		cpu->l = value;
		return true;
	}
	if (strcmp(name, "bc") == 0) {
		cpu->bc = value;
		return true;
	}
	if (strcmp(name, "de") == 0) {
		cpu->de = value;
		return true;
	}
	if (strcmp(name, "hl") == 0) {
		cpu->hl = value;
		return true;
	}
	if (strcmp(name, "af") == 0) {
		cpu->af = value;
		cpu->f.packed &= 0xF0;
		return true;
	}
	if (strcmp(name, "pc") == 0) {
		cpu->pc = value;
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		return true;
	}
	if (strcmp(name, "sp") == 0) {
		cpu->sp = value;
		return true;
	}
	if (strcmp(name, "f") == 0) {
		cpu->f.packed = value & 0xF0;
		return true;
	}
	return false;
}
