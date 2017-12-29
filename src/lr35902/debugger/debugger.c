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

static void _destroyBreakpoint(struct LR35902DebugBreakpoint* breakpoint) {
	if (breakpoint->condition) {
		parseFree(breakpoint->condition);
		free(breakpoint->condition);
	}
}

static void _destroyWatchpoint(struct LR35902DebugWatchpoint* watchpoint) {
	if (watchpoint->condition) {
		parseFree(watchpoint->condition);
		free(watchpoint->condition);
	}
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

static void LR35902DebuggerSetBreakpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
static void LR35902DebuggerSetConditionalBreakpoint(struct mDebuggerPlatform*, uint32_t address, int segment, struct ParseTree* condition);
static void LR35902DebuggerClearBreakpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
static void LR35902DebuggerSetWatchpoint(struct mDebuggerPlatform*, uint32_t address, int segment, enum mWatchpointType type);
static void LR35902DebuggerSetConditionalWatchpoint(struct mDebuggerPlatform*, uint32_t address, int segment, enum mWatchpointType type, struct ParseTree* condition);
static void LR35902DebuggerClearWatchpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
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
	platform->setConditionalBreakpoint = LR35902DebuggerSetConditionalBreakpoint;
	platform->clearBreakpoint = LR35902DebuggerClearBreakpoint;
	platform->setWatchpoint = LR35902DebuggerSetWatchpoint;
	platform->setConditionalWatchpoint = LR35902DebuggerSetConditionalWatchpoint;
	platform->clearWatchpoint = LR35902DebuggerClearWatchpoint;
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
	LR35902DebugBreakpointListInit(&debugger->breakpoints, 0);
	LR35902DebugWatchpointListInit(&debugger->watchpoints, 0);
}

void LR35902DebuggerDeinit(struct mDebuggerPlatform* platform) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) platform;
	size_t i;
	for (i = 0; i < LR35902DebugBreakpointListSize(&debugger->breakpoints); ++i) {
		_destroyBreakpoint(LR35902DebugBreakpointListGetPointer(&debugger->breakpoints, i));
	}
	LR35902DebugBreakpointListDeinit(&debugger->breakpoints);

	for (i = 0; i < LR35902DebugWatchpointListSize(&debugger->watchpoints); ++i) {
		_destroyWatchpoint(LR35902DebugWatchpointListGetPointer(&debugger->watchpoints, i));
	}
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
	LR35902DebuggerSetConditionalBreakpoint(d, address, segment, NULL);
}

static void LR35902DebuggerSetConditionalBreakpoint(struct mDebuggerPlatform* d, uint32_t address, int segment, struct ParseTree* condition) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902DebugBreakpoint* breakpoint = LR35902DebugBreakpointListAppend(&debugger->breakpoints);
	breakpoint->address = address;
	breakpoint->segment = segment;
	breakpoint->condition = condition;
}

static void LR35902DebuggerClearBreakpoint(struct mDebuggerPlatform* d, uint32_t address, int segment) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	struct LR35902DebugBreakpointList* breakpoints = &debugger->breakpoints;
	size_t i;
	for (i = 0; i < LR35902DebugBreakpointListSize(breakpoints); ++i) {
		struct LR35902DebugBreakpoint* breakpoint = LR35902DebugBreakpointListGetPointer(breakpoints, i);
		if (breakpoint->address == address && breakpoint->segment == segment) {
			_destroyBreakpoint(LR35902DebugBreakpointListGetPointer(breakpoints, i));
			LR35902DebugBreakpointListShift(breakpoints, i, 1);
		}
	}
}

static bool LR35902DebuggerHasBreakpoints(struct mDebuggerPlatform* d) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	return LR35902DebugBreakpointListSize(&debugger->breakpoints) || LR35902DebugWatchpointListSize(&debugger->watchpoints);
}

static void LR35902DebuggerSetWatchpoint(struct mDebuggerPlatform* d, uint32_t address, int segment, enum mWatchpointType type) {
	LR35902DebuggerSetConditionalWatchpoint(d, address, segment, type, NULL);
}

static void LR35902DebuggerSetConditionalWatchpoint(struct mDebuggerPlatform* d, uint32_t address, int segment, enum mWatchpointType type, struct ParseTree* condition) {
	struct LR35902Debugger* debugger = (struct LR35902Debugger*) d;
	if (!LR35902DebugWatchpointListSize(&debugger->watchpoints)) {
		LR35902DebuggerInstallMemoryShim(debugger);
	}
	struct LR35902DebugWatchpoint* watchpoint = LR35902DebugWatchpointListAppend(&debugger->watchpoints);
	watchpoint->address = address;
	watchpoint->type = type;
	watchpoint->segment = segment;
	watchpoint->condition = condition;
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
	LR35902Disassemble(&info, disPtr, sizeof(disassembly) - (disPtr - disassembly));

	*length = snprintf(out, *length, "A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X SP: %04X PC: %04X | %s",
		               cpu->a, cpu->f.packed, cpu->b, cpu->c,
		               cpu->d, cpu->e, cpu->h, cpu->l,
		               cpu->sp, cpu->pc, disassembly);
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
