/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/arm/debugger/cli-debugger.h>

#include <mgba/core/core.h>
#include <mgba/core/timing.h>
#include <mgba/internal/arm/debugger/debugger.h>
#include <mgba/internal/arm/debugger/memory-debugger.h>
#include <mgba/internal/arm/decoder.h>
#include <mgba/internal/debugger/cli-debugger.h>

static void _printStatus(struct CLIDebuggerSystem*);

static void _disassembleArm(struct CLIDebugger*, struct CLIDebugVector*);
static void _disassembleThumb(struct CLIDebugger*, struct CLIDebugVector*);
static void _setBreakpointARM(struct CLIDebugger*, struct CLIDebugVector*);
static void _setBreakpointThumb(struct CLIDebugger*, struct CLIDebugVector*);

static void _disassembleMode(struct CLIDebugger*, struct CLIDebugVector*, enum ExecutionMode mode);
static uint32_t _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode);

static struct CLIDebuggerCommandSummary _armCommands[] = {
	{ "break/a", _setBreakpointARM, "I", "Set a software breakpoint as ARM" },
	{ "break/t", _setBreakpointThumb, "I", "Set a software breakpoint as Thumb" },
	{ "disassemble/a", _disassembleArm, "Ii", "Disassemble instructions as ARM" },
	{ "disassemble/t", _disassembleThumb, "Ii", "Disassemble instructions as Thumb" },
	{ 0, 0, 0, 0 }
};

static struct CLIDebuggerCommandAlias _armCommandAliases[] = {
	{ "b/a", "break/a" },
	{ "b/t", "break/t" },
	{ "dis/a", "disassemble/a" },
	{ "dis/t", "disassemble/t" },
	{ "disasm/a",  "disassemble/a" },
	{ "disasm/t",  "disassemble/t" },
	{ 0, 0 }
};

static inline void _printPSR(struct CLIDebuggerBackend* be, union PSR psr) {
	be->printf(be, "%08X [%c%c%c%c%c%c%c]\n", psr.packed,
	           psr.n ? 'N' : '-',
	           psr.z ? 'Z' : '-',
	           psr.c ? 'C' : '-',
	           psr.v ? 'V' : '-',
	           psr.i ? 'I' : '-',
	           psr.f ? 'F' : '-',
	           psr.t ? 'T' : '-');
}

static void _disassemble(struct CLIDebuggerSystem* debugger, struct CLIDebugVector* dv) {
	struct ARMCore* cpu = debugger->p->d.core->cpu;
	_disassembleMode(debugger->p, dv, cpu->executionMode);
}

static void _disassembleArm(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_disassembleMode(debugger, dv, MODE_ARM);
}

static void _disassembleThumb(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_disassembleMode(debugger, dv, MODE_THUMB);
}

static void _disassembleMode(struct CLIDebugger* debugger, struct CLIDebugVector* dv, enum ExecutionMode mode) {
	struct ARMCore* cpu = debugger->d.core->cpu;
	uint32_t address;
	int size;
	int wordSize;

	if (mode == MODE_ARM) {
		wordSize = WORD_SIZE_ARM;
	} else {
		wordSize = WORD_SIZE_THUMB;
	}

	if (!dv || dv->type != CLIDV_INT_TYPE) {
		address = cpu->gprs[ARM_PC] - wordSize;
	} else {
		address = dv->intValue;
		dv = dv->next;
	}

	if (!dv || dv->type != CLIDV_INT_TYPE) {
		size = 1;
	} else {
		size = dv->intValue;
		// TODO: Check for excess args
	}

	int i;
	for (i = 0; i < size; ++i) {
		address += _printLine(debugger, address, mode);
	}
}

static inline uint32_t _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	struct CLIDebuggerBackend* be = debugger->backend;
	struct mCore* core = debugger->d.core;
	char disassembly[64];
	struct ARMInstructionInfo info;
	address &= ~(WORD_SIZE_THUMB - 1);
	be->printf(be, "%08X:  ", address);
	if (mode == MODE_ARM) {
		uint32_t instruction = core->busRead32(core, address & ~(WORD_SIZE_ARM - 1));
		ARMDecodeARM(instruction, &info);
		ARMDisassemble(&info, core->cpu, core->symbolTable, address + WORD_SIZE_ARM * 2, disassembly, sizeof(disassembly));
		be->printf(be, "%08X\t%s\n", instruction, disassembly);
		return WORD_SIZE_ARM;
	} else {
		struct ARMInstructionInfo info2;
		struct ARMInstructionInfo combined;
		uint16_t instruction = core->busRead16(core, address);
		uint16_t instruction2 = core->busRead16(core, address + WORD_SIZE_THUMB);
		ARMDecodeThumb(instruction, &info);
		ARMDecodeThumb(instruction2, &info2);
		if (ARMDecodeThumbCombine(&info, &info2, &combined)) {
			ARMDisassemble(&combined, core->cpu, core->symbolTable, address + WORD_SIZE_THUMB * 2, disassembly, sizeof(disassembly));
			be->printf(be, "%04X %04X\t%s\n", instruction, instruction2, disassembly);
			return WORD_SIZE_THUMB * 2;
		} else {
			ARMDisassemble(&info, core->cpu, core->symbolTable, address + WORD_SIZE_THUMB * 2, disassembly, sizeof(disassembly));
			be->printf(be, "%04X     \t%s\n", instruction, disassembly);
			return WORD_SIZE_THUMB;
		}
	}
}

static void _printStatus(struct CLIDebuggerSystem* debugger) {
	struct CLIDebuggerBackend* be = debugger->p->backend;
	struct ARMCore* cpu = debugger->p->d.core->cpu;
	int r;
	for (r = 0; r < 16; r += 4) {
		be->printf(be, "%sr%i: %08X  %sr%i: %08X  %sr%i: %08X  %sr%i: %08X\n",
		    r < 10 ? " " : "", r, cpu->gprs[r],
		    r < 9 ? " " : "", r + 1, cpu->gprs[r + 1],
		    r < 8 ? " " : "", r + 2, cpu->gprs[r + 2],
		    r < 7 ? " " : "", r + 3, cpu->gprs[r + 3]);
	}
	be->printf(be, "cpsr: ");
	_printPSR(be, cpu->cpsr);
	be->printf(be, "Cycle: %" PRIu64 "\n", mTimingGlobalTime(debugger->p->d.core->timing));
	int instructionLength;
	enum ExecutionMode mode = cpu->cpsr.t;
	if (mode == MODE_ARM) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	_printLine(debugger->p, cpu->gprs[ARM_PC] - instructionLength, mode);
}

static void _setBreakpointARM(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct CLIDebuggerBackend* be = debugger->backend;
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		be->printf(be, "%s", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ssize_t id = ARMDebuggerSetSoftwareBreakpoint(debugger->d.platform, address, MODE_ARM);
	if (id > 0) {
		debugger->backend->printf(debugger->backend, INFO_BREAKPOINT_ADDED, id);
	}
}

static void _setBreakpointThumb(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct CLIDebuggerBackend* be = debugger->backend;
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		be->printf(be, "%s", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ssize_t id = ARMDebuggerSetSoftwareBreakpoint(debugger->d.platform, address, MODE_THUMB);
	if (id > 0) {
		debugger->backend->printf(debugger->backend, INFO_BREAKPOINT_ADDED, id);
	}
}

void ARMCLIDebuggerCreate(struct CLIDebuggerSystem* debugger) {
	debugger->printStatus = _printStatus;
	debugger->disassemble = _disassemble;
	debugger->platformName = "ARM";
	debugger->platformCommands = _armCommands;
	debugger->platformCommandAliases = _armCommandAliases;
}
