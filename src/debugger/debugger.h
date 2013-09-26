#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "arm.h"

enum DebuggerState {
	DEBUGGER_PAUSED,
	DEBUGGER_RUNNING,
	DEBUGGER_EXITING
};

struct DebugBreakpoint {
	struct DebugBreakpoint* next;
	int32_t address;
};

struct DebugMemoryShim {
	struct ARMMemory d;
	struct ARMMemory* original;

	struct ARMDebugger* p;
	struct DebugBreakpoint* watchpoints;
};

struct ARMDebugger {
	enum DebuggerState state;
	struct ARMCore* cpu;

	char* lastCommand;
	struct DebugBreakpoint* breakpoints;
	struct DebugMemoryShim memoryShim;
};

void ARMDebuggerInit(struct ARMDebugger*, struct ARMCore*);
void ARMDebuggerRun(struct ARMDebugger*);
void ARMDebuggerEnter(struct ARMDebugger*);

#endif
