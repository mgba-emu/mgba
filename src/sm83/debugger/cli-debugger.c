/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/sm83/debugger/cli-debugger.h>

#include <mgba/core/core.h>
#include <mgba/core/timing.h>
#include <mgba/internal/debugger/cli-debugger.h>
#include <mgba/internal/sm83/decoder.h>
#include <mgba/internal/sm83/debugger/debugger.h>
#include <mgba/internal/sm83/sm83.h>

static void _printStatus(struct CLIDebuggerSystem*);

static void _disassemble(struct CLIDebuggerSystem* debugger, struct CLIDebugVector* dv);
static uint16_t _printLine(struct CLIDebugger* debugger, uint16_t address, int segment);

static inline void _printFlags(struct CLIDebuggerBackend* be, union FlagRegister f) {
	be->printf(be, "F: [%c%c%c%c]\n",
	           f.z ? 'Z' : '-',
	           f.n ? 'N' : '-',
	           f.h ? 'H' : '-',
	           f.c ? 'C' : '-');
}

static void _disassemble(struct CLIDebuggerSystem* debugger, struct CLIDebugVector* dv) {
	struct SM83Core* cpu = debugger->p->d.core->cpu;

	uint16_t address;
	int segment = -1;
	size_t size;
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		address = cpu->pc;
	} else {
		address = dv->intValue;
		segment = dv->segmentValue;
		dv = dv->next;
	}

	if (!dv || dv->type != CLIDV_INT_TYPE) {
		size = 1;
	} else {
		size = dv->intValue;
		// TODO: Check for excess args
	}

	size_t i;
	for (i = 0; i < size; ++i) {
		address = _printLine(debugger->p, address, segment);
	}
}

static inline uint16_t _printLine(struct CLIDebugger* debugger, uint16_t address, int segment) {
	struct CLIDebuggerBackend* be = debugger->backend;
	struct SM83InstructionInfo info = {{0}, 0};
	char disassembly[48];
	char* disPtr = disassembly;
	if (segment >= 0) {
		be->printf(be, "%02X:", segment);
	}
	be->printf(be, "%04X:  ", address);
	uint8_t instruction;
	size_t bytesRemaining = 1;
	for (bytesRemaining = 1; bytesRemaining; --bytesRemaining) {
		instruction = debugger->d.core->rawRead8(debugger->d.core, address, segment);
		disPtr += snprintf(disPtr, sizeof(disassembly) - (disPtr - disassembly), "%02X", instruction);
		++address;
		bytesRemaining += SM83Decode(instruction, &info);
	};
	disPtr[0] = '\t';
	++disPtr;
	SM83Disassemble(&info, address, disPtr, sizeof(disassembly) - (disPtr - disassembly));
	be->printf(be, "%s\n", disassembly);
	return address;
}

static void _printStatus(struct CLIDebuggerSystem* debugger) {
	struct CLIDebuggerBackend* be = debugger->p->backend;
	struct SM83Core* cpu = debugger->p->d.core->cpu;
	be->printf(be, "A: %02X  F: %02X  (AF: %04X)\n", cpu->a, cpu->f.packed, cpu->af);
	be->printf(be, "B: %02X  C: %02X  (BC: %04X)\n", cpu->b, cpu->c, cpu->bc);
	be->printf(be, "D: %02X  E: %02X  (DE: %04X)\n", cpu->d, cpu->e, cpu->de);
	be->printf(be, "H: %02X  L: %02X  (HL: %04X)\n", cpu->h, cpu->l, cpu->hl);
	be->printf(be, "PC: %04X  SP: %04X\n", cpu->pc, cpu->sp);
	_printFlags(be, cpu->f);
	be->printf(be, "T-cycle: %" PRIu64 "\n", mTimingGlobalTime(debugger->p->d.core->timing));

	struct SM83Debugger* platDebugger = (struct SM83Debugger*) debugger->p->d.platform;
	size_t i;
	for (i = 0; platDebugger->segments[i].name; ++i) {
		be->printf(be, "%s%s: %02X", i ? "  " : "", platDebugger->segments[i].name, cpu->memory.currentSegment(cpu, platDebugger->segments[i].start));
	}
	if (i) {
		be->printf(be, "\n");
	}
	if (platDebugger->printStatus) {
		platDebugger->printStatus(debugger);
	}
	_printLine(debugger->p, cpu->pc, cpu->memory.currentSegment(cpu, cpu->pc));
}

void SM83CLIDebuggerCreate(struct CLIDebuggerSystem* debugger) {
	debugger->printStatus = _printStatus;
	debugger->disassemble = _disassemble;
	debugger->platformName = "SM83";
	debugger->platformCommands = NULL;
	debugger->platformCommandAliases = NULL;
}
