/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba-cli.h"

#include "gba-io.h"
#include "gba-serialize.h"
#include "gba-thread.h"

#ifdef USE_CLI_DEBUGGER

static const char* ERROR_MISSING_ARGS = "Arguments missing"; // TODO: share

static void _GBACLIDebuggerInit(struct CLIDebuggerSystem*);
static void _GBACLIDebuggerDeinit(struct CLIDebuggerSystem*);
static uint32_t _GBACLIDebuggerLookupIdentifier(struct CLIDebuggerSystem*, const char* name, struct CLIDebugVector* dv);

static void _load(struct CLIDebugger*, struct CLIDebugVector*);
static void _save(struct CLIDebugger*, struct CLIDebugVector*);

struct CLIDebuggerCommandSummary _GBACLIDebuggerCommands[] = {
	{ "load", _load, CLIDVParse, "Load a savestate" },
	{ "save", _save, CLIDVParse, "Save a savestate" },
	{ 0, 0, 0, 0 }
};
#endif

struct GBACLIDebugger* GBACLIDebuggerCreate(struct GBAThread* context) {
	struct GBACLIDebugger* debugger = malloc(sizeof(struct GBACLIDebugger));
#ifdef USE_CLI_DEBUGGER
	debugger->d.init = _GBACLIDebuggerInit;
	debugger->d.deinit = _GBACLIDebuggerDeinit;
	debugger->d.lookupIdentifier = _GBACLIDebuggerLookupIdentifier;

	debugger->d.name = "Game Boy Advance";
	debugger->d.commands = _GBACLIDebuggerCommands;

	debugger->context = context;
#endif

	return debugger;
}

#ifdef USE_CLI_DEBUGGER
static void _GBACLIDebuggerInit(struct CLIDebuggerSystem* debugger) {
	UNUSED(debugger);
}

static void _GBACLIDebuggerDeinit(struct CLIDebuggerSystem* debugger) {
	UNUSED(debugger);
}

static uint32_t _GBACLIDebuggerLookupIdentifier(struct CLIDebuggerSystem* debugger, const char* name, struct CLIDebugVector* dv) {
	struct GBACLIDebugger* gbaDebugger = (struct GBACLIDebugger*) debugger;
	int i;
	for (i = 0; i < REG_MAX; i += 2) {
		const char* reg = GBAIORegisterNames[i >> 1];
		if (reg && strcasecmp(reg, name) == 0) {
			return GBALoad16(gbaDebugger->context->gba->cpu, BASE_IO | i, 0);
		}
	}
	dv->type = CLIDV_ERROR_TYPE;
	return 0;
}

static void _load(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}

	int state = dv->intValue;
	if (state < 1 || state > 9) {
		printf("State %u out of range", state);
	}

	struct GBACLIDebugger* gbaDebugger = (struct GBACLIDebugger*) debugger->system;

	GBALoadState(gbaDebugger->context->gba, gbaDebugger->context->stateDir, dv->intValue);
}

static void _save(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}

	int state = dv->intValue;
	if (state < 1 || state > 9) {
		printf("State %u out of range", state);
	}

	struct GBACLIDebugger* gbaDebugger = (struct GBACLIDebugger*) debugger->system;

	GBASaveState(gbaDebugger->context->gba, gbaDebugger->context->stateDir, dv->intValue, true);
}
#endif
