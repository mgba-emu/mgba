/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cli-debugger.h"

#ifdef USE_CLI_DEBUGGER
#include "core/core.h"
#include "debugger/cli-debugger.h"
#include "lr35902/lr35902.h"

static void _printStatus(struct CLIDebuggerSystem*);

static struct CLIDebuggerCommandSummary _lr35902Commands[] = {
	{ 0, 0, 0, 0 }
};

static inline void _printFlags(union FlagRegister f) {
	printf("[%c%c%c%c]\n",
	       f.z ? 'Z' : '-',
	       f.n ? 'N' : '-',
	       f.h ? 'H' : '-',
	       f.c ? 'C' : '-');
}

static void _printStatus(struct CLIDebuggerSystem* debugger) {
	struct LR35902Core* cpu = debugger->p->d.core->cpu;
	printf("A: %02X F: %02X (AF: %04X)\n", cpu->a, cpu->f.packed, cpu->af);
	printf("B: %02X C: %02X (BC: %04X)\n", cpu->b, cpu->c, cpu->bc);
	printf("D: %02X E: %02X (DE: %04X)\n", cpu->d, cpu->e, cpu->de);
	printf("H: %02X L: %02X (HL: %04X)\n", cpu->h, cpu->l, cpu->hl);
	printf("PC: %04X SP: %04X\n", cpu->pc, cpu->sp);
	_printFlags(cpu->f);
}

static uint32_t _lookupIdentifier(struct mDebugger* debugger, const char* name, struct CLIDebugVector* dv) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	struct LR35902Core* cpu = debugger->core->cpu;
	if (strcmp(name, "a") == 0) {
		return cpu->a;
	}
	if (strcmp(name, "b") == 0) {
		return cpu->b;
	}
	if (strcmp(name, "c") == 0) {
		return cpu->c;
	}
	if (strcmp(name, "d") == 0) {
		return cpu->d;
	}
	if (strcmp(name, "e") == 0) {
		return cpu->e;
	}
	if (strcmp(name, "h") == 0) {
		return cpu->h;
	}
	if (strcmp(name, "l") == 0) {
		return cpu->l;
	}
	if (strcmp(name, "bc") == 0) {
		return cpu->bc;
	}
	if (strcmp(name, "de") == 0) {
		return cpu->de;
	}
	if (strcmp(name, "hl") == 0) {
		return cpu->hl;
	}
	if (strcmp(name, "af") == 0) {
		return cpu->af;
	}
	if (strcmp(name, "pc") == 0) {
		return cpu->pc;
	}
	if (strcmp(name, "sp") == 0) {
		return cpu->pc;
	}
	if (strcmp(name, "f") == 0) {
		return cpu->f.packed;
	}
	if (cliDebugger->system) {
		uint32_t value = cliDebugger->system->lookupIdentifier(cliDebugger->system, name, dv);
		if (dv->type != CLIDV_ERROR_TYPE) {
			return value;
		}
	} else {
		dv->type = CLIDV_ERROR_TYPE;
	}
	return 0;
}

void LR35902CLIDebuggerCreate(struct CLIDebuggerSystem* debugger) {
	debugger->printStatus = _printStatus;
	debugger->disassemble = NULL;
	debugger->platformName = "GB-Z80";
	debugger->platformCommands = NULL;
}

#endif
