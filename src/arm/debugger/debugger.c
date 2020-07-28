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
#include <mgba/internal/debugger/stack-trace.h>
#include <stdint.h>

DEFINE_VECTOR(ARMDebugBreakpointList, struct ARMDebugBreakpoint);

static inline uint32_t ARMDebuggerGetInstructionLength(struct ARMCore* cpu) {
	return cpu->cpsr.t == MODE_ARM ? WORD_SIZE_ARM : WORD_SIZE_THUMB;
}

static bool ARMDebuggerUpdateStackTraceInternal(struct mDebuggerPlatform* d, uint32_t pc) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct ARMCore* cpu = debugger->cpu;
	struct ARMInstructionInfo info;
	uint32_t instruction = cpu->prefetch[0];
	struct mStackTrace* stack = &d->p->stackTrace;
	bool interrupt = false;
	ARMDecodeARM(instruction, &info);

	if (_ARMModeHasSPSR(cpu->cpsr.priv)) {
		struct mStackFrame* irqFrame = mStackTraceGetFrame(stack, 0);
		// TODO: uint32_t ivtBase = ARMControlRegIsVE(cpu->cp15.r1.c0) ? 0xFFFF0000 : 0x00000000;
		uint32_t ivtBase = 0x00000000;
		if (ivtBase <= pc && pc < ivtBase + 0x20 && !(irqFrame && _ARMModeHasSPSR(((struct ARMRegisterFile*)irqFrame->regs)->cpsr.priv))) {
			// TODO: Potential enhancement opportunity: add break-on-exception mode
			irqFrame = mStackTracePush(stack, pc, pc, cpu->gprs[ARM_SP], &cpu->regs);
			irqFrame->interrupt = true;
			interrupt = true;
		}
	}

	if (info.branchType == ARM_BRANCH_NONE && !interrupt) {
		return false;
	}

	struct mStackFrame* frame = mStackTraceGetFrame(stack, 0);
	bool isCall = (info.branchType & ARM_BRANCH_LINKED);
	uint32_t destAddress;

	if (frame && frame->finished) {
		mStackTracePop(stack);
		frame = NULL;
	}

	if (interrupt && info.branchType == ARM_BRANCH_NONE) {
		// The stack frame was already pushed up above, so there's no
		// action necessary here, but we still want to check for a
		// breakpoint down below.
		//
		// The first instruction could possibly be a call, which would
		// need ANOTHER stack frame, so only skip if it's not.
	} else if (info.operandFormat & ARM_OPERAND_MEMORY_1) {
		// This is most likely ldmia ..., {..., pc}, which is a function return.
		// To find which stack slot holds the return address, count number of set bits.
		// (gcc/clang will convert the loop to intrinsics if available)
		int regCount = 0;
		uint32_t reglist = info.op1.immediate;
		while (reglist != 0) {
			reglist &= reglist - 1;
			regCount++;
		}
		uint32_t baseAddress = cpu->gprs[info.memory.baseReg] + ((regCount - 1) << 2);
		destAddress = cpu->memory.load32(cpu, baseAddress, NULL);
	} else if (info.operandFormat & ARM_OPERAND_IMMEDIATE_1) {
		if (!isCall) {
			return false;
		}
		destAddress = info.op1.immediate + cpu->gprs[ARM_PC];
	} else if (info.operandFormat & ARM_OPERAND_REGISTER_1) {
		if (!isCall && info.op1.reg != ARM_LR && !(_ARMModeHasSPSR(cpu->cpsr.priv) && info.op1.reg == ARM_PC)) {
			return false;
		}
		destAddress = cpu->gprs[info.op1.reg];
	} else {
		abort(); // Should be unreachable
	}

	if (info.branchType & ARM_BRANCH_INDIRECT) {
		destAddress = cpu->memory.load32(cpu, destAddress, NULL);
	}

	if (isCall) {
		int instructionLength = ARMDebuggerGetInstructionLength(debugger->cpu);
		frame = mStackTracePush(stack, pc, destAddress + instructionLength, cpu->gprs[ARM_SP], &cpu->regs);
		if (!(debugger->stackTraceMode & STACK_TRACE_BREAK_ON_CALL)) {
			return false;
		}
	} else {
		frame = mStackTraceGetFrame(stack, 0);
		if (!frame) {
			return false;
		}
		if (!frame->breakWhenFinished && !(debugger->stackTraceMode & STACK_TRACE_BREAK_ON_RETURN)) {
			mStackTracePop(stack);
			return false;
		}
		frame->finished = true;
	}
	struct mDebuggerEntryInfo debuggerInfo = {
		.address = pc,
		.type.st.traceType = isCall ? STACK_TRACE_BREAK_ON_CALL : STACK_TRACE_BREAK_ON_RETURN,
		.pointId = 0
	};
	mDebuggerEnter(d->p, DEBUGGER_ENTER_STACK, &debuggerInfo);
	return true;
}

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
	int instructionLength = ARMDebuggerGetInstructionLength(debugger->cpu);
	uint32_t pc = debugger->cpu->gprs[ARM_PC] - instructionLength;
	if (debugger->stackTraceMode != STACK_TRACE_DISABLED && ARMDebuggerUpdateStackTraceInternal(d, pc)) {
		return;
	}
	struct ARMDebugBreakpoint* breakpoint = _lookupBreakpoint(&debugger->breakpoints, pc);
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
static void ARMDebuggerFormatRegisters(struct ARMRegisterFile* regs, char* out, size_t* length);
static void ARMDebuggerFrameFormatRegisters(struct mStackFrame* frame, char* out, size_t* length);
static bool ARMDebuggerGetRegister(struct mDebuggerPlatform*, const char* name, int32_t* value);
static bool ARMDebuggerSetRegister(struct mDebuggerPlatform*, const char* name, int32_t value);
static uint32_t ARMDebuggerGetStackTraceMode(struct mDebuggerPlatform*);
static void ARMDebuggerSetStackTraceMode(struct mDebuggerPlatform*, uint32_t);
static bool ARMDebuggerUpdateStackTrace(struct mDebuggerPlatform* d);

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
	platform->getStackTraceMode = ARMDebuggerGetStackTraceMode;
	platform->setStackTraceMode = ARMDebuggerSetStackTraceMode;
	platform->updateStackTrace = ARMDebuggerUpdateStackTrace;
	return platform;
}

void ARMDebuggerInit(void* cpu, struct mDebuggerPlatform* platform) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) platform;
	debugger->cpu = cpu;
	debugger->originalMemory = debugger->cpu->memory;
	debugger->nextId = 1;
	debugger->stackTraceMode = STACK_TRACE_DISABLED;
	ARMDebugBreakpointListInit(&debugger->breakpoints, 0);
	ARMDebugBreakpointListInit(&debugger->swBreakpoints, 0);
	mWatchpointListInit(&debugger->watchpoints, 0);
	struct mStackTrace* stack = &platform->p->stackTrace;
	mStackTraceInit(stack, sizeof(struct ARMRegisterFile));
	stack->formatRegisters = ARMDebuggerFrameFormatRegisters;
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

	mStackTraceDeinit(&platform->p->stackTrace);
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
	return ARMDebugBreakpointListSize(&debugger->breakpoints) || mWatchpointListSize(&debugger->watchpoints) || debugger->stackTraceMode != STACK_TRACE_DISABLED;
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

	size_t regStringLen = *length;
	ARMDebuggerFormatRegisters(&cpu->regs, out, &regStringLen);
	*length = regStringLen + snprintf(out + regStringLen, *length - regStringLen, " | %s", disassembly);
}

static void ARMDebuggerFormatRegisters(struct ARMRegisterFile* regs, char* out, size_t* length) {
	*length = snprintf(out, *length, "%08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X %08X cpsr: %08X",
		               regs->gprs[0],  regs->gprs[1],  regs->gprs[2],  regs->gprs[3],
		               regs->gprs[4],  regs->gprs[5],  regs->gprs[6],  regs->gprs[7],
		               regs->gprs[8],  regs->gprs[9],  regs->gprs[10], regs->gprs[11],
		               regs->gprs[12], regs->gprs[13], regs->gprs[14], regs->gprs[15],
		               regs->cpsr.packed);
}

static void ARMDebuggerFrameFormatRegisters(struct mStackFrame* frame, char* out, size_t* length) {
	ARMDebuggerFormatRegisters((struct ARMRegisterFile*)frame->regs, out, length);
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

static uint32_t ARMDebuggerGetStackTraceMode(struct mDebuggerPlatform* d) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	return debugger->stackTraceMode;
}

static void ARMDebuggerSetStackTraceMode(struct mDebuggerPlatform* d, uint32_t mode) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	struct mStackTrace* stack = &d->p->stackTrace;
	if (mode == STACK_TRACE_DISABLED && debugger->stackTraceMode != STACK_TRACE_DISABLED) {
		mStackTraceClear(stack);
	}
	debugger->stackTraceMode = mode;
}

static bool ARMDebuggerUpdateStackTrace(struct mDebuggerPlatform* d) {
	struct ARMDebugger* debugger = (struct ARMDebugger*) d;
	int instructionLength = ARMDebuggerGetInstructionLength(debugger->cpu);
	uint32_t pc = debugger->cpu->gprs[ARM_PC] - instructionLength;
	if (debugger->stackTraceMode != STACK_TRACE_DISABLED) {
		return ARMDebuggerUpdateStackTraceInternal(d, pc);
	} else {
		return false;
	}
}
