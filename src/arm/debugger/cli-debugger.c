/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/arm/debugger/cli-debugger.h>

#include <mgba/core/core.h>
#include <mgba/internal/arm/debugger/debugger.h>
#include <mgba/internal/arm/debugger/memory-debugger.h>
#include <mgba/internal/arm/decoder.h>
#include <mgba/internal/debugger/cli-debugger.h>

static void _printStatus(struct CLIDebuggerSystem*);

static void _disassembleArm(struct CLIDebugger*, struct CLIDebugVector*);
static void _disassembleThumb(struct CLIDebugger*, struct CLIDebugVector*);
static void _setBreakpointARM(struct CLIDebugger*, struct CLIDebugVector*);
static void _setBreakpointThumb(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeRegister(struct CLIDebugger*, struct CLIDebugVector*);

static void _disassembleMode(struct CLIDebugger*, struct CLIDebugVector*, enum ExecutionMode mode);
static uint32_t _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode);

static struct CLIDebuggerCommandSummary _armCommands[] = {
	{ "b/a", _setBreakpointARM, CLIDVParse, "Set a software breakpoint as ARM" },
	{ "b/t", _setBreakpointThumb, CLIDVParse, "Set a software breakpoint as Thumb" },
	{ "break/a", _setBreakpointARM, CLIDVParse, "Set a software breakpoint as ARM" },
	{ "break/t", _setBreakpointThumb, CLIDVParse, "Set a software breakpoint as Thumb" },
	{ "dis/a", _disassembleArm, CLIDVParse, "Disassemble instructions as ARM" },
	{ "dis/t", _disassembleThumb, CLIDVParse, "Disassemble instructions as Thumb" },
	{ "disasm/a", _disassembleArm, CLIDVParse, "Disassemble instructions as ARM" },
	{ "disasm/t", _disassembleThumb, CLIDVParse, "Disassemble instructions as Thumb" },
	{ "disassemble/a", _disassembleArm, CLIDVParse, "Disassemble instructions as ARM" },
	{ "disassemble/t", _disassembleThumb, CLIDVParse, "Disassemble instructions as Thumb" },
	{ "w/r", _writeRegister, CLIDVParse, "Write a register" },
	{ 0, 0, 0, 0 }
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
		dv = dv->next; // TODO: Check for excess args
	}

	int i;
	for (i = 0; i < size; ++i) {
		address += _printLine(debugger, address, mode);
	}
}

static inline uint32_t _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	struct CLIDebuggerBackend* be = debugger->backend;
	char disassembly[48];
	struct ARMInstructionInfo info;
	be->printf(be, "%08X:  ", address);
	if (mode == MODE_ARM) {
		uint32_t instruction = debugger->d.core->busRead32(debugger->d.core, address);
		ARMDecodeARM(instruction, &info);
		ARMDisassemble(&info, address + WORD_SIZE_ARM * 2, disassembly, sizeof(disassembly));
		be->printf(be, "%08X\t%s\n", instruction, disassembly);
		return WORD_SIZE_ARM;
	} else {
		struct ARMInstructionInfo info2;
		struct ARMInstructionInfo combined;
		uint16_t instruction = debugger->d.core->busRead16(debugger->d.core, address);
		uint16_t instruction2 = debugger->d.core->busRead16(debugger->d.core, address + WORD_SIZE_THUMB);
		ARMDecodeThumb(instruction, &info);
		ARMDecodeThumb(instruction2, &info2);
		if (ARMDecodeThumbCombine(&info, &info2, &combined)) {
			ARMDisassemble(&combined, address + WORD_SIZE_THUMB * 2, disassembly, sizeof(disassembly));
			be->printf(be, "%04X %04X\t%s\n", instruction, instruction2, disassembly);
			return WORD_SIZE_THUMB * 2;
		} else {
			ARMDisassemble(&info, address + WORD_SIZE_THUMB * 2, disassembly, sizeof(disassembly));
			be->printf(be, "%04X     \t%s\n", instruction, disassembly);
			return WORD_SIZE_THUMB;
		}
	}
}

static void _printStatus(struct CLIDebuggerSystem* debugger) {
	struct CLIDebuggerBackend* be = debugger->p->backend;
	struct ARMCore* cpu = debugger->p->d.core->cpu;
	int r;
	for (r = 0; r < 4; ++r) {
		be->printf(be, "%08X %08X %08X %08X\n",
		    cpu->gprs[r << 2],
		    cpu->gprs[(r << 2) + 1],
		    cpu->gprs[(r << 2) + 2],
		    cpu->gprs[(r << 2) + 3]);
	}
	_printPSR(be, cpu->cpsr);
	int instructionLength;
	enum ExecutionMode mode = cpu->cpsr.t;
	if (mode == MODE_ARM) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	_printLine(debugger->p, cpu->gprs[ARM_PC] - instructionLength, mode);
}

static void _writeRegister(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct CLIDebuggerBackend* be = debugger->backend;
	struct ARMCore* cpu = debugger->d.core->cpu;
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		be->printf(be, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!dv->next || dv->next->type != CLIDV_INT_TYPE) {
		be->printf(be, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t regid = dv->intValue;
	uint32_t value = dv->next->intValue;
	if (regid >= ARM_PC) {
		return;
	}
	cpu->gprs[regid] = value;
}

static void _setBreakpointARM(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct CLIDebuggerBackend* be = debugger->backend;
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		be->printf(be, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerSetSoftwareBreakpoint(debugger->d.platform, address, MODE_ARM);
}

static void _setBreakpointThumb(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct CLIDebuggerBackend* be = debugger->backend;
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		be->printf(be, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerSetSoftwareBreakpoint(debugger->d.platform, address, MODE_THUMB);
}

static uint32_t _lookupPlatformIdentifier(struct CLIDebuggerSystem* debugger, const char* name, struct CLIDebugVector* dv) {
	struct ARMCore* cpu = debugger->p->d.core->cpu;
	if (strcmp(name, "sp") == 0) {
		return cpu->gprs[ARM_SP];
	}
	if (strcmp(name, "lr") == 0) {
		return cpu->gprs[ARM_LR];
	}
	if (strcmp(name, "pc") == 0) {
		return cpu->gprs[ARM_PC];
	}
	if (strcmp(name, "cpsr") == 0) {
		return cpu->cpsr.packed;
	}
	// TODO: test if mode has SPSR
	if (strcmp(name, "spsr") == 0) {
		return cpu->spsr.packed;
	}
	if (name[0] == 'r' && name[1] >= '0' && name[1] <= '9') {
		int reg = atoi(&name[1]);
		if (reg < 16) {
			return cpu->gprs[reg];
		}
	}
	dv->type = CLIDV_ERROR_TYPE;
	return 0;
}

void ARMCLIDebuggerCreate(struct CLIDebuggerSystem* debugger) {
	debugger->printStatus = _printStatus;
	debugger->disassemble = _disassemble;
	debugger->lookupPlatformIdentifier = _lookupPlatformIdentifier;
	debugger->platformName = "ARM";
	debugger->platformCommands = _armCommands;
}
