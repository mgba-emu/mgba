/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/extra/cli.h>

#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/video.h>
#include <mgba/internal/sm83/debugger/cli-debugger.h>

static void _GBCLIDebuggerInit(struct CLIDebuggerSystem*);
static bool _GBCLIDebuggerCustom(struct CLIDebuggerSystem*);

static void _frame(struct CLIDebugger*, struct CLIDebugVector*);
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
static void _load(struct CLIDebugger*, struct CLIDebugVector*);
static void _save(struct CLIDebugger*, struct CLIDebugVector*);
#endif

struct CLIDebuggerCommandSummary _GBCLIDebuggerCommands[] = {
	{ "frame", _frame, "", "Frame advance" },
#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
	{ "load", _load, "*", "Load a savestate" },
	{ "save", _save, "*", "Save a savestate" },
#endif
	{ 0, 0, 0, 0 }
};

struct CLIDebuggerSystem* GBCLIDebuggerCreate(struct mCore* core) {
	UNUSED(core);
	struct GBCLIDebugger* debugger = malloc(sizeof(struct GBCLIDebugger));
	SM83CLIDebuggerCreate(&debugger->d);
	debugger->d.init = _GBCLIDebuggerInit;
	debugger->d.deinit = NULL;
	debugger->d.custom = _GBCLIDebuggerCustom;

	debugger->d.name = "Game Boy";
	debugger->d.commands = _GBCLIDebuggerCommands;
	debugger->d.commandAliases = NULL;

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
		if (!gbDebugger->inVblank && GBRegisterSTATGetMode(((struct GB*) gbDebugger->core->board)->memory.io[GB_REG_STAT]) == 1) {
			mDebuggerEnter(&gbDebugger->d.p->d, DEBUGGER_ENTER_MANUAL, 0);
			gbDebugger->frameAdvance = false;
			return false;
		}
		gbDebugger->inVblank = GBRegisterSTATGetMode(((struct GB*) gbDebugger->core->board)->memory.io[GB_REG_STAT]) == 1;
		return true;
	}
	return true;
}

static void _frame(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = DEBUGGER_CALLBACK;

	struct GBCLIDebugger* gbDebugger = (struct GBCLIDebugger*) debugger->system;
	gbDebugger->frameAdvance = true;
	gbDebugger->inVblank = GBRegisterSTATGetMode(((struct GB*) gbDebugger->core->board)->memory.io[GB_REG_STAT]) == 1;
}

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
static void _load(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct CLIDebuggerBackend* be = debugger->backend;
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		be->printf(be, "%s\n", ERROR_MISSING_ARGS);
		return;
	}

	int state = dv->intValue;
	if (state < 1 || state > 9) {
		be->printf(be, "State %u out of range", state);
	}

	struct GBCLIDebugger* gbDebugger = (struct GBCLIDebugger*) debugger->system;

	mCoreLoadState(gbDebugger->core, dv->intValue, SAVESTATE_SCREENSHOT | SAVESTATE_RTC);
}

static void _save(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct CLIDebuggerBackend* be = debugger->backend;
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		be->printf(be, "%s\n", ERROR_MISSING_ARGS);
		return;
	}

	int state = dv->intValue;
	if (state < 1 || state > 9) {
		be->printf(be, "State %u out of range", state);
	}

	struct GBCLIDebugger* gbDebugger = (struct GBCLIDebugger*) debugger->system;

	mCoreSaveState(gbDebugger->core, dv->intValue, SAVESTATE_SCREENSHOT | SAVESTATE_RTC | SAVESTATE_METADATA);
}
#endif
