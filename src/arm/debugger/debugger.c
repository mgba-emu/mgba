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

static void _destroyBreakpoint(struct ARMDebugBreakpoint* breakpoint) {
	if (breakpoint->condition) {
		parseFree(breakpoint->condition);
		free(breakpoint->condition);
	}
}

static void _destroyWatchpoint(struct ARMDebugWatchpoint* watchpoint) {
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
	if (breakpoint->condition) {
		int32_t value;
		int segment;
		if (!mDebuggerEvaluateParseTree(d->p, breakpoint->condition, &value, &segment) || !(value || segment >= 0)) {
			return;
		}
	}
	struct mDebuggerEntryInfo info = {
		.address = breakpoint->address,
		.type.bp.breakType = BREAKPOINT_HARDWARE
	};
	mDebuggerEnter(d->p, DEBUGGER_ENTER_BREAKPOINT, &info);
}

static void ARMDebuggerInit(void* cpu, struct mDebuggerPlatform* platform);
static void ARMDebuggerDeinit(struct mDebuggerPlatform* platform);

static void ARMDebuggerEnter(struct mDebuggerPlatform* d, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info);

static void ARMDebuggerSetBreakpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
static void ARMDebuggerSetConditionalBreakpoint(struct mDebuggerPlatform*, uint32_t address, int segment, struct ParseTree* condition);
static void ARMDebuggerClearBreakpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
static void ARMDebuggerSetWatchpoint(struct mDebuggerPlatform*, uint32_t address, int segment, enum mWatchpointType type);
static void ARMDebuggerSetConditionalWatchpoint(struct mDebuggerPlatform*, uint32_t address, int segment, enum mWatchpointType type, struct ParseTree* condition);
static void ARMDebuggerClearWatchpoint(struct mDebuggerPlatform*, uint32_t address, int segment);
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
	platform->setConditionalBreakpoint = ARMDebuggerSetConditionalBreakpoint;
	platform->clearBreakpoint = ARMDebuggerClearBreakpoint;
	platform->setWatchpoint = ARMDebuggerSetWatchpoint;
	platform->setConditionalWatchpoint = ARMDebuggerSetConditionalWatchpoint;
	platform->clearWatchpoint = ARMDebuggerClearWatchpoint;
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

	size_t i;
	for (i = 0; i < ARMDebugBreakpointListSize(&debugger->breakpoints); ++i) {
		_destroyBreakpoint(ARMDebugBreakpointListGetPointer(&debugger->breakpoints, i));
	}
	ARMDebugBreakpointListDeinit(&debugger->breakpoints);

	for (i = 0; i < ARMDebugWatchpointListSize(&debugger->watchpoints); ++i) {
		_destroyWatchpoint(ARMDebugWatchpointListGetPointer(&debugger->watchpoints, i));
	}
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

static void ARMDebuggerSetBreakpoint(struct mDebuggerPlatform* d, uint32_t address, int segment) {
	ARMDebuggerSetConditionalBreakpoint(d, address, segment, NULL);
}

static void ARMDebuggerSetConditionalBreakpoint(struct mDebuggerPlatform* d, uint32_t address, int segment, struct ParseTree* condition) {
	UNUSED(segment);
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMDebugBreakpoint* breakpoint = ARMDebugBreakpointListAppend(&debugger->breakpoints);
	breakpoint->condition = condition;
	breakpoint->address = address;
	breakpoint->isSw = false;
}

static void ARMDebuggerClearBreakpoint(struct mDebuggerPlatform* d, uint32_t address, int segment) {
	UNUSED(segment);
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMDebugBreakpointList* breakpoints = &debugger->breakpoints;
	size_t i;
	for (i = 0; i < ARMDebugBreakpointListSize(breakpoints); ++i) {
		if (ARMDebugBreakpointListGetPointer(breakpoints, i)->address == address) {
			_destroyBreakpoint(ARMDebugBreakpointListGetPointer(breakpoints, i));
			ARMDebugBreakpointListShift(breakpoints, i, 1);
		}
	}
}

static bool ARMDebuggerHasBreakpoints(struct mDebuggerPlatform* d) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	return ARMDebugBreakpointListSize(&debugger->breakpoints) || ARMDebugWatchpointListSize(&debugger->watchpoints);
}

static void ARMDebuggerSetWatchpoint(struct mDebuggerPlatform* d, uint32_t address, int segment, enum mWatchpointType type) {
	ARMDebuggerSetConditionalWatchpoint(d, address, segment, type, NULL);
}

static void ARMDebuggerSetConditionalWatchpoint(struct mDebuggerPlatform* d, uint32_t address, int segment, enum mWatchpointType type, struct ParseTree* condition) {
	UNUSED(segment);
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	if (!ARMDebugWatchpointListSize(&debugger->watchpoints)) {
		ARMDebuggerInstallMemoryShim(debugger);
	}
	struct ARMDebugWatchpoint* watchpoint = ARMDebugWatchpointListAppend(&debugger->watchpoints);
	watchpoint->address = address;
	watchpoint->type = type;
	watchpoint->condition = condition;
}

static void ARMDebuggerClearWatchpoint(struct mDebuggerPlatform* d, uint32_t address, int segment) {
	UNUSED(segment);
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMDebugWatchpointList* watchpoints = &debugger->watchpoints;
	size_t i;
	for (i = 0; i < ARMDebugWatchpointListSize(watchpoints); ++i) {
		if (ARMDebugWatchpointListGetPointer(watchpoints, i)->address == address) {
			_destroyWatchpoint(ARMDebugWatchpointListGetPointer(watchpoints, i));
			ARMDebugWatchpointListShift(watchpoints, i, 1);
		}
	}
	if (!ARMDebugWatchpointListSize(&debugger->watchpoints)) {
		ARMDebuggerRemoveMemoryShim(debugger);
	}
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
		int32_t currentCycles = 0;
		if (cpu->executionMode == MODE_ARM) {
			ARM_WRITE_PC;
		} else {
			THUMB_WRITE_PC;
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
			int32_t currentCycles = 0;
			if (cpu->executionMode == MODE_ARM) {
				ARM_WRITE_PC;
			} else {
				THUMB_WRITE_PC;
			}
		}
		return true;
	}
	return false;
}
