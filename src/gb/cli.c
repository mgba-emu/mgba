/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cli.h"

#include "lr35902/cli-debugger.h"

#ifdef USE_CLI_DEBUGGER

struct CLIDebuggerCommandSummary _GBCLIDebuggerCommands[] = {
	{ 0, 0, 0, 0 }
};

struct CLIDebuggerSystem* GBCLIDebuggerCreate(struct mCore* core) {
	UNUSED(core);
	struct CLIDebuggerSystem* debugger = malloc(sizeof(struct CLIDebuggerSystem));
	LR35902CLIDebuggerCreate(debugger);
	debugger->init = NULL;
	debugger->deinit = NULL;
	debugger->custom = NULL;
	debugger->lookupIdentifier = NULL;

	debugger->name = "Game Boy";
	debugger->commands = _GBCLIDebuggerCommands;

	return debugger;
}

#endif
