#ifndef CLI_DEBUGGER_H
#define CLI_DEBUGGER_H

#include "util/common.h"

#include "debugger.h"

#include <histedit.h>

struct CLIDebugger {
	struct ARMDebugger d;

	EditLine* elstate;
	History* histate;
};

void CLIDebuggerCreate(struct CLIDebugger*);

#endif
