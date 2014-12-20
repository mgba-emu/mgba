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
static bool _GBACLIDebuggerCustom(struct CLIDebuggerSystem*);
static uint32_t _GBACLIDebuggerLookupIdentifier(struct CLIDebuggerSystem*, const char* name, struct CLIDebugVector* dv);

static void _frame(struct CLIDebugger*, struct CLIDebugVector*);
static void _load(struct CLIDebugger*, struct CLIDebugVector*);
static void _save(struct CLIDebugger*, struct CLIDebugVector*);

struct CLIDebuggerCommandSummary _GBACLIDebuggerCommands[] = {
	{ "frame", _frame, 0, "Frame advance" },
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
	debugger->d.custom = _GBACLIDebuggerCustom;
	debugger->d.lookupIdentifier = _GBACLIDebuggerLookupIdentifier;

	debugger->d.name = "Game Boy Advance";
	debugger->d.commands = _GBACLIDebuggerCommands;

	debugger->context = context;
#endif

	return debugger;
}

#ifdef USE_CLI_DEBUGGER
static void _GBACLIDebuggerInit(struct CLIDebuggerSystem* debugger) {
	struct GBACLIDebugger* gbaDebugger = (struct GBACLIDebugger*) debugger;

	gbaDebugger->frameAdvance = false;
}

static void _GBACLIDebuggerDeinit(struct CLIDebuggerSystem* debugger) {
	UNUSED(debugger);
}

static bool _GBACLIDebuggerCustom(struct CLIDebuggerSystem* debugger) {
	struct GBACLIDebugger* gbaDebugger = (struct GBACLIDebugger*) debugger;

	if (gbaDebugger->frameAdvance) {
		if (!gbaDebugger->inVblank && GBARegisterDISPSTATIsInVblank(gbaDebugger->context->gba->video.dispstat)) {
			ARMDebuggerEnter(&gbaDebugger->d.p->d, DEBUGGER_ENTER_BREAKPOINT);
			gbaDebugger->frameAdvance = false;
			return false;
		}
		gbaDebugger->inVblank = GBARegisterDISPSTATGetInVblank(gbaDebugger->context->gba->video.dispstat);
		return true;
	}
	return false;
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

static void _frame(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = DEBUGGER_CUSTOM;

	struct GBACLIDebugger* gbaDebugger = (struct GBACLIDebugger*) debugger->system;
	gbaDebugger->frameAdvance = true;
	gbaDebugger->inVblank = GBARegisterDISPSTATGetInVblank(gbaDebugger->context->gba->video.dispstat);
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
