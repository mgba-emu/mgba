/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cli.h"

#include "arm/debugger/cli-debugger.h"

#ifdef USE_CLI_DEBUGGER

static void _DSCLIDebuggerInit(struct CLIDebuggerSystem*);
static bool _DSCLIDebuggerCustom(struct CLIDebuggerSystem*);
static uint32_t _DSCLIDebuggerLookupIdentifier(struct CLIDebuggerSystem*, const char* name, struct CLIDebugVector* dv);

static void _frame(struct CLIDebugger*, struct CLIDebugVector*);

struct CLIDebuggerCommandSummary _DSCLIDebuggerCommands[] = {
	{ "frame", _frame, 0, "Frame advance" },
	{ 0, 0, 0, 0 }
};

struct DSCLIDebugger* DSCLIDebuggerCreate(struct mCore* core) {
	struct DSCLIDebugger* debugger = malloc(sizeof(struct DSCLIDebugger));
	ARMCLIDebuggerCreate(&debugger->d);
	debugger->d.init = _DSCLIDebuggerInit;
	debugger->d.deinit = NULL;
	debugger->d.custom = _DSCLIDebuggerCustom;
	debugger->d.lookupIdentifier = _DSCLIDebuggerLookupIdentifier;

	debugger->d.name = "DS";
	debugger->d.commands = _DSCLIDebuggerCommands;

	debugger->core = core;

	return debugger;
}

static void _DSCLIDebuggerInit(struct CLIDebuggerSystem* debugger) {
	struct DSCLIDebugger* dsDebugger = (struct DSCLIDebugger*) debugger;

	dsDebugger->frameAdvance = false;
}

static bool _DSCLIDebuggerCustom(struct CLIDebuggerSystem* debugger) {
	return false;
}

static uint32_t _DSCLIDebuggerLookupIdentifier(struct CLIDebuggerSystem* debugger, const char* name, struct CLIDebugVector* dv) {
	UNUSED(debugger);
	dv->type = CLIDV_ERROR_TYPE;
	return 0;
}

static void _frame(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = DEBUGGER_CUSTOM;

	struct DSCLIDebugger* dsDebugger = (struct DSCLIDebugger*) debugger->system;
	dsDebugger->frameAdvance = true;
}
#endif
