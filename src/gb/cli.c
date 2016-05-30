/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cli.h"

#include "core/core.h"
#include "core/serialize.h"
#include "gb/gb.h"
#include "gb/io.h"
#include "gb/video.h"
#include "lr35902/debugger/cli-debugger.h"

#ifdef USE_CLI_DEBUGGER

static void _GBCLIDebuggerInit(struct CLIDebuggerSystem*);
static bool _GBCLIDebuggerCustom(struct CLIDebuggerSystem*);
static uint32_t _GBCLIDebuggerLookupIdentifier(struct CLIDebuggerSystem*, const char* name, struct CLIDebugVector* dv);

static void _frame(struct CLIDebugger*, struct CLIDebugVector*);

struct CLIDebuggerCommandSummary _GBCLIDebuggerCommands[] = {
	{ "frame", _frame, 0, "Frame advance" },
	{ 0, 0, 0, 0 }
};

struct CLIDebuggerSystem* GBCLIDebuggerCreate(struct mCore* core) {
	UNUSED(core);
	struct GBCLIDebugger* debugger = malloc(sizeof(struct GBCLIDebugger));
	LR35902CLIDebuggerCreate(&debugger->d);
	debugger->d.init = _GBCLIDebuggerInit;
	debugger->d.deinit = NULL;
	debugger->d.custom = _GBCLIDebuggerCustom;
	debugger->d.lookupIdentifier = _GBCLIDebuggerLookupIdentifier;

	debugger->d.name = "Game Boy";
	debugger->d.commands = _GBCLIDebuggerCommands;

	debugger->core = core;

	return &debugger->d;
}

static void _GBCLIDebuggerInit(struct CLIDebuggerSystem* debugger) {
	struct GBCLIDebugger* gbDebugger = (struct GBCLIDebugger*) debugger;

	gbDebugger->frameAdvance = false;
}

static bool _GBCLIDebuggerCustom(struct CLIDebuggerSystem* debugger) {
	struct GBCLIDebugger* gbDebugger = (struct GBCLIDebugger*) debugger;

	if (gbDebugger->frameAdvance) {
		if (!gbDebugger->inVblank && GBRegisterSTATGetMode(((struct GB*) gbDebugger->core->board)->memory.io[REG_STAT]) == 1) {
			mDebuggerEnter(&gbDebugger->d.p->d, DEBUGGER_ENTER_MANUAL, 0);
			gbDebugger->frameAdvance = false;
			return false;
		}
		gbDebugger->inVblank = GBRegisterSTATGetMode(((struct GB*) gbDebugger->core->board)->memory.io[REG_STAT]) == 1;
		return true;
	}
	return false;
}

static uint32_t _GBCLIDebuggerLookupIdentifier(struct CLIDebuggerSystem* debugger, const char* name, struct CLIDebugVector* dv) {
	UNUSED(debugger);
	int i;
	for (i = 0; i < REG_MAX; ++i) {
		const char* reg = GBIORegisterNames[i];
		if (reg && strcasecmp(reg, name) == 0) {
			return GB_BASE_IO | i;
		}
	}
	dv->type = CLIDV_ERROR_TYPE;
	return 0;
}

static void _frame(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = DEBUGGER_CUSTOM;

	struct GBCLIDebugger* gbDebugger = (struct GBCLIDebugger*) debugger->system;
	gbDebugger->frameAdvance = true;
	gbDebugger->inVblank = GBRegisterSTATGetMode(((struct GB*) gbDebugger->core->board)->memory.io[REG_STAT]) == 1;
}

#endif
