#include "gba-cli.h"

#include "gba-io.h"
#include "gba-thread.h"

static void _GBACLIDebuggerInit(struct CLIDebuggerSystem*);
static void _GBACLIDebuggerDeinit(struct CLIDebuggerSystem*);
static uint32_t _GBACLIDebuggerLookupIdentifier(struct CLIDebuggerSystem*, const char* name, struct CLIDebugVector* dv);

struct CLIDebuggerCommandSummary _GBACLIDebuggerCommands[] = {
	{ 0, 0, 0, 0 }
};

struct GBACLIDebugger* GBACLIDebuggerCreate(struct GBAThread* context) {
	struct GBACLIDebugger* debugger = malloc(sizeof(struct GBACLIDebugger));
	debugger->d.init = _GBACLIDebuggerInit;
	debugger->d.deinit = _GBACLIDebuggerDeinit;
	debugger->d.lookupIdentifier = _GBACLIDebuggerLookupIdentifier;

	debugger->d.name = "Game Boy Advance";
	debugger->d.commands = _GBACLIDebuggerCommands;

	debugger->context = context;

	return debugger;
}

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

