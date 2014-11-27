#ifndef GBA_CLI_H
#define GBA_CLI_H

#include "debugger/cli-debugger.h"

struct GBAThread;

struct GBACLIDebugger {
	struct CLIDebuggerSystem d;

	struct GBAThread* context;
};

struct GBACLIDebugger* GBACLIDebuggerCreate(struct GBAThread*);

#endif
