#ifndef DEBUGGER_H
#define DEBUGGER_H

#ifdef USE_DEBUGGER
#include <histedit.h>

#include "arm.h"
#endif

enum DebuggerState {
	DEBUGGER_PAUSED,
	DEBUGGER_RUNNING,
	DEBUGGER_EXITING,
	DEBUGGER_SHUTDOWN
};

#ifdef USE_DEBUGGER

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

	EditLine* elstate;
	History* histate;

	struct DebugBreakpoint* breakpoints;
	struct DebugMemoryShim memoryShim;
};

void ARMDebuggerInit(struct ARMDebugger*, struct ARMCore*);
void ARMDebuggerDeinit(struct ARMDebugger*);
void ARMDebuggerRun(struct ARMDebugger*);
void ARMDebuggerEnter(struct ARMDebugger*);

#else

struct ARMDebugger {
	enum DebuggerState state;
};

#endif

#endif
