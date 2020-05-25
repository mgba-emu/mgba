/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/arm/debugger/debugger.h>

#include <mgba/core/core.h>
#include <mgba/internal/arm/arm.h>
#include <mgba/internal/arm/decoder.h>
#include <mgba/internal/arm/isa-inlines.h>
#include <mgba/internal/arm/debugger/memory-debugger.h>
#include <mgba/internal/debugger/parser.h>

DEFINE_VECTOR(ARMDebugBreakpointList, struct ARMDebugBreakpoint);

static struct ARMDebugBreakpoint* _lookupBreakpoint(struct ARMDebugBreakpointList* breakpoints, uint32_t address) {
	size_t i;
	for (i = 0; i < ARMDebugBreakpointListSize(breakpoints); ++i) {
		if (ARMDebugBreakpointListGetPointer(breakpoints, i)->d.address == address) {
			return ARMDebugBreakpointListGetPointer(breakpoints, i);
		}
	}
	return 0;
}

static void _destroyBreakpoint(struct ARMDebugBreakpoint* breakpoint) {
	if (breakpoint->d.condition) {
		parseFree(breakpoint->d.condition);
		free(breakpoint->d.condition);
	}
}

static void _destroyWatchpoint(struct mWatchpoint* watchpoint) {
	if (watchpoint->condition) {
		parseFree(watchpoint->condition);
		free(watchpoint->condition);
	}
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
	if (breakpoint->d.condition) {
		int32_t value;
		int segment;
		if (!mDebuggerEvaluateParseTree(d->p, breakpoint->d.condition, &value, &segment) || !(value || segment >= 0)) {
			return;
		}
	}
	struct mDebuggerEntryInfo info = {
		.address = breakpoint->d.address,
		.type.bp.breakType = BREAKPOINT_HARDWARE,
		.pointId = breakpoint->d.id
	};
	mDebuggerEnter(d->p, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void ARMDebuggerInit(void* cpu, struct mDebuggerPlatform* platform);
static void ARMDebuggerDeinit(struct mDebuggerPlatform* platform);

static void ARMDebuggerEnter(struct mDebuggerPlatform* d, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info);

static ssize_t ARMDebuggerSetBreakpoint(struct mDebuggerPlatform*, const struct mBreakpoint*);
static bool ARMDebuggerClearBreakpoint(struct mDebuggerPlatform*, ssize_t id);
static void ARMDebuggerListBreakpoints(struct mDebuggerPlatform*, struct mBreakpointList*);
static ssize_t ARMDebuggerSetWatchpoint(struct mDebuggerPlatform*, const struct mWatchpoint*);
static void ARMDebuggerListWatchpoints(struct mDebuggerPlatform*, struct mWatchpointList*);
static void ARMDebuggerCheckBreakpoints(struct mDebuggerPlatform*);
static bool ARMDebuggerHasBreakpoints(struct mDebuggerPlatform*);
static void ARMDebuggerTrace(struct mDebuggerPlatform*, char* out, size_t* length);
static bool ARMDebuggerGetRegister(struct mDebuggerPlatform*, const char* name, int32_t* value);
static bool ARMDebuggerSetRegister(struct mDebuggerPlatform*, const char* name, int32_t value);

struct mDebuggerPlatform* ARMDebuggerPlatformCreate(void) {
	struct mDebuggerPlatform* platform = (struct mDebuggerPlatform*) malloc(sizeof(struct ARMDebugger));
	platform->entered = ARMDebuggerEnter;
	platform->init = ARMDebuggerInit;
	platform->deinit = ARMDebuggerDeinit;
	platform->setBreakpoint = ARMDebuggerSetBreakpoint;
	platform->listBreakpoints = ARMDebuggerListBreakpoints;
	platform->clearBreakpoint = ARMDebuggerClearBreakpoint;
	platform->setWatchpoint = ARMDebuggerSetWatchpoint;
	platform->listWatchpoints = ARMDebuggerListWatchpoints;
	platform->checkBreakpoints = ARMDebuggerCheckBreakpoints;
	platform->hasBreakpoints = ARMDebuggerHasBreakpoints;
	platform->trace = ARMDebuggerTrace;
	platform->getRegister = ARMDebuggerGetRegister;
	platform->setRegister = ARMDebuggerSetRegister;
	return platform;
}

void ARMDebuggerInit(void* cpu, struct mDebuggerPlatform* platform) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) platform;
	debugger->cpu = cpu;
	debugger->originalMemory = debugger->cpu->memory;
	debugger->nextId = 1;
	ARMDebugBreakpointListInit(&debugger->breakpoints, 0);
	ARMDebugBreakpointListInit(&debugger->swBreakpoints, 0);
	mWatchpointListInit(&debugger->watchpoints, 0);
}

void ARMDebuggerDeinit(struct mDebuggerPlatform* platform) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) platform;
	if (debugger->clearSoftwareBreakpoint) {
		// Clear the stack backwards in case any overlap
		size_t b;
		for (b = ARMDebugBreakpointListSize(&debugger->swBreakpoints); b; --b) {
			struct ARMDebugBreakpoint* breakpoint = ARMDebugBreakpointListGetPointer(&debugger->swBreakpoints, b - 1);
			debugger->clearSoftwareBreakpoint(debugger, breakpoint);
		}
	}
	ARMDebuggerRemoveMemoryShim(debugger);

	size_t i;
	for (i = 0; i < ARMDebugBreakpointListSize(&debugger->breakpoints); ++i) {
		_destroyBreakpoint(ARMDebugBreakpointListGetPointer(&debugger->breakpoints, i));
	}
	ARMDebugBreakpointListDeinit(&debugger->breakpoints);

	for (i = 0; i < mWatchpointListSize(&debugger->watchpoints); ++i) {
		_destroyWatchpoint(mWatchpointListGetPointer(&debugger->watchpoints, i));
	}
	ARMDebugBreakpointListDeinit(&debugger->swBreakpoints);
	mWatchpointListDeinit(&debugger->watchpoints);
}

static void ARMDebuggerEnter(struct mDebuggerPlatform* platform, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) platform;
	struct ARMCore* cpu = debugger->cpu;
	cpu->nextEvent = cpu->cycles;
	if (reason == DEBUGGER_ENTER_BREAKPOINT) {
		struct ARMDebugBreakpoint* breakpoint = _lookupBreakpoint(&debugger->swBreakpoints, _ARMPCAddress(cpu));
		if (breakpoint && breakpoint->d.type == BREAKPOINT_SOFTWARE) {
			info->address = breakpoint->d.address;
			info->pointId = breakpoint->d.id;
			if (debugger->clearSoftwareBreakpoint) {
				debugger->clearSoftwareBreakpoint(debugger, breakpoint);
			}

			ARMRunFake(cpu, breakpoint->sw.opcode);

			if (debugger->setSoftwareBreakpoint) {
				debugger->setSoftwareBreakpoint(debugger, breakpoint->d.address, breakpoint->sw.mode, &breakpoint->sw.opcode);
			}
		}
	}
	if (debugger->d.p->entered) {
		debugger->d.p->entered(debugger->d.p, reason, info);
	}
}

ssize_t ARMDebuggerSetSoftwareBreakpoint(struct mDebuggerPlatform* d, uint32_t address, enum ExecutionMode mode) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	uint32_t opcode;
	if (!debugger->setSoftwareBreakpoint || !debugger->setSoftwareBreakpoint(debugger, address, mode, &opcode)) {
		return -1;
	}

	struct ARMDebugBreakpoint* breakpoint = ARMDebugBreakpointListAppend(&debugger->swBreakpoints);
	ssize_t id = debugger->nextId;
	++debugger->nextId;
	breakpoint->d.id = id;
	breakpoint->d.address = address & ~1; // Clear Thumb bit since it's not part of a valid address
	breakpoint->d.segment = -1;
	breakpoint->d.condition = NULL;
	breakpoint->d.type = BREAKPOINT_SOFTWARE;
	breakpoint->sw.opcode = opcode;
	breakpoint->sw.mode = mode;

	return id;
}

static ssize_t ARMDebuggerSetBreakpoint(struct mDebuggerPlatform* d, const struct mBreakpoint* info) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMDebugBreakpoint* breakpoint = ARMDebugBreakpointListAppend(&debugger->breakpoints);
	ssize_t id = debugger->nextId;
	++debugger->nextId;
	breakpoint->d = *info;
	breakpoint->d.address &= ~1; // Clear Thumb bit since it's not part of a valid address
	breakpoint->d.id = id;
	if (info->type == BREAKPOINT_SOFTWARE) {
		// TODO
		abort();
	}
	return id;
}

static bool ARMDebuggerClearBreakpoint(struct mDebuggerPlatform* d, ssize_t id) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	size_t i;

	struct ARMDebugBreakpointList* breakpoints = &debugger->breakpoints;
	for (i = 0; i < ARMDebugBreakpointListSize(breakpoints); ++i) {
		if (ARMDebugBreakpointListGetPointer(breakpoints, i)->d.id == id) {
			_destroyBreakpoint(ARMDebugBreakpointListGetPointer(breakpoints, i));
			ARMDebugBreakpointListShift(breakpoints, i, 1);
			return true;
		}
	}

	struct ARMDebugBreakpointList* swBreakpoints = &debugger->swBreakpoints;
	if (debugger->clearSoftwareBreakpoint) {
		for (i = 0; i < ARMDebugBreakpointListSize(swBreakpoints); ++i) {
			if (ARMDebugBreakpointListGetPointer(swBreakpoints, i)->d.id == id) {
				debugger->clearSoftwareBreakpoint(debugger, ARMDebugBreakpointListGetPointer(swBreakpoints, i));
				ARMDebugBreakpointListShift(swBreakpoints, i, 1);
				return true;
			}
		}
	}

	struct mWatchpointList* watchpoints = &debugger->watchpoints;
	for (i = 0; i < mWatchpointListSize(watchpoints); ++i) {
		if (mWatchpointListGetPointer(watchpoints, i)->id == id) {
			_destroyWatchpoint(mWatchpointListGetPointer(watchpoints, i));
			mWatchpointListShift(watchpoints, i, 1);
			if (!mWatchpointListSize(&debugger->watchpoints)) {
				ARMDebuggerRemoveMemoryShim(debugger);
			}
			return true;
		}
	}
	return false;
}

static void ARMDebuggerListBreakpoints(struct mDebuggerPlatform* d, struct mBreakpointList* list) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	mBreakpointListClear(list);
	size_t i, s;
	for (i = 0, s = 0; i < ARMDebugBreakpointListSize(&debugger->breakpoints) || s < ARMDebugBreakpointListSize(&debugger->swBreakpoints);) {
		struct ARMDebugBreakpoint* hw = NULL;
		struct ARMDebugBreakpoint* sw = NULL;
		if (i < ARMDebugBreakpointListSize(&debugger->breakpoints)) {
			hw = ARMDebugBreakpointListGetPointer(&debugger->breakpoints, i);
		}
		if (s < ARMDebugBreakpointListSize(&debugger->swBreakpoints)) {
			sw = ARMDebugBreakpointListGetPointer(&debugger->swBreakpoints, s);
		}
		struct mBreakpoint* b = mBreakpointListAppend(list);
		if (hw && sw) {
			if (hw->d.id < sw->d.id) {
				*b = hw->d;
				++i;
			} else {
				*b = sw->d;
				++s;
			}
		} else if (hw) {
			*b = hw->d;
			++i;
		} else if (sw) {
			*b = sw->d;
			++s;		
		} else {
			abort(); // Should be unreachable
		}
	}
}

static bool ARMDebuggerHasBreakpoints(struct mDebuggerPlatform* d) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	return ARMDebugBreakpointListSize(&debugger->breakpoints) || mWatchpointListSize(&debugger->watchpoints);
}

static ssize_t ARMDebuggerSetWatchpoint(struct mDebuggerPlatform* d, const struct mWatchpoint* info) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	if (!mWatchpointListSize(&debugger->watchpoints)) {
		ARMDebuggerInstallMemoryShim(debugger);
	}
	struct mWatchpoint* watchpoint = mWatchpointListAppend(&debugger->watchpoints);
	ssize_t id = debugger->nextId;
	++debugger->nextId;
	*watchpoint = *info;
	watchpoint->id = id;
	return id;
}

static void ARMDebuggerListWatchpoints(struct mDebuggerPlatform* d, struct mWatchpointList* list) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	mWatchpointListClear(list);
	mWatchpointListCopy(list, &debugger->watchpoints);
}

static void ARMDebuggerTrace(struct mDebuggerPlatform* d, char* out, size_t* length) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMCore* cpu = debugger->cpu;

	char disassembly[64];

	struct ARMInstructionInfo info;
	if (cpu->executionMode == MODE_ARM) {
		uint32_t instruction = cpu->prefetch[0];
		sprintf(disassembly, "%08X: ", instruction);
		ARMDecodeARM(instruction, &info);
		ARMDisassemble(&info, cpu->gprs[ARM_PC], disassembly + strlen("00000000: "), sizeof(disassembly) - strlen("00000000: "));
	} else {
		struct ARMInstructionInfo info2;
		struct ARMInstructionInfo combined;
		uint16_t instruction = cpu->prefetch[0];
		uint16_t instruction2 = cpu->prefetch[1];
		ARMDecodeThumb(instruction, &info);
		ARMDecodeThumb(instruction2, &info2);
		if (ARMDecodeThumbCombine(&info, &info2, &combined)) {
			sprintf(disassembly, "%04X%04X: ", instruction, instruction2);
			ARMDisassemble(&combined, cpu->gprs[ARM_PC], disassembly + strlen("00000000: "), sizeof(disassembly) - strlen("00000000: "));
		} else {
			sprintf(disassembly, "    %04X: ", instruction);
			ARMDisassemble(&info, cpu->gprs[ARM_PC], disassembly + strlen("00000000: "), sizeof(disassembly) - strlen("00000000: "));
		}
	}

	*length = snprintf(out, *length, "%08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X cpsr: %08X | %s",
		               cpu->gprs[0],  cpu->gprs[1],  cpu->gprs[2],  cpu->gprs[3],
		               cpu->gprs[4],  cpu->gprs[5],  cpu->gprs[6],  cpu->gprs[7],
		               cpu->gprs[8],  cpu->gprs[9],  cpu->gprs[10], cpu->gprs[11],
		               cpu->gprs[12], cpu->gprs[13], cpu->gprs[14], cpu->gprs[15],
		               cpu->cpsr.packed, disassembly);
}

bool ARMDebuggerGetRegister(struct mDebuggerPlatform* d, const char* name, int32_t* value) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMCore* cpu = debugger->cpu;

	if (strcmp(name, "sp") == 0) {
		*value = cpu->gprs[ARM_SP];
		return true;
	}
	if (strcmp(name, "lr") == 0) {
		*value = cpu->gprs[ARM_LR];
		return true;
	}
	if (strcmp(name, "pc") == 0) {
		*value = cpu->gprs[ARM_PC];
		return true;
	}
	if (strcmp(name, "cpsr") == 0) {
		*value = cpu->cpsr.packed;
		return true;
	}
	// TODO: test if mode has SPSR
	if (strcmp(name, "spsr") == 0) {
		*value = cpu->spsr.packed;
		return true;
	}
	if (name[0] == 'r') {
		char* end;
		uint32_t reg = strtoul(&name[1], &end, 10);
		if (reg <= ARM_PC) {
			*value = cpu->gprs[reg];
			return true;
		}
	}
	return false;
}

bool ARMDebuggerSetRegister(struct mDebuggerPlatform* d, const char* name, int32_t value) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMCore* cpu = debugger->cpu;

	if (strcmp(name, "sp") == 0) {
		cpu->gprs[ARM_SP] = value;
		return true;
	}
	if (strcmp(name, "lr") == 0) {
		cpu->gprs[ARM_LR] = value;
		return true;
	}
	if (strcmp(name, "pc") == 0) {
		cpu->gprs[ARM_PC] = value;
		if (cpu->executionMode == MODE_ARM) {
			ARMWritePC(cpu);
		} else {
			ThumbWritePC(cpu);
		}
		return true;
	}
	if (name[0] == 'r') {
		char* end;
		uint32_t reg = strtoul(&name[1], &end, 10);
		if (reg > ARM_PC) {
			return false;
		}
		cpu->gprs[reg] = value;
		if (reg == ARM_PC) {
			if (cpu->executionMode == MODE_ARM) {
				ARMWritePC(cpu);
			} else {
				ThumbWritePC(cpu);
			}
		}
		return true;
	}
	return false;
}
