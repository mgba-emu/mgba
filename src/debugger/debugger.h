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

enum DebuggerEntryReason {
	DEBUGGER_ENTER_MANUAL,
	DEBUGGER_ENTER_BREAKPOINT,
	DEBUGGER_ENTER_WATCHPOINT,
	DEBUGGER_ENTER_ILLEGAL_OP
};

struct ARMDebugger {
	enum DebuggerState state;
	struct ARMCore* cpu;

	EditLine* elstate;
	History* histate;

	struct DebugBreakpoint* breakpoints;
	struct DebugMemoryShim memoryShim;

	void (*init)(struct ARMDebugger*, struct ARMCore*);
	void (*deinit)(struct ARMDebugger*);
	void (*paused)(struct ARMDebugger*);
	void (*entered)(struct ARMDebugger*, enum DebuggerEntryReason);
};

void ARMDebuggerInit(struct ARMDebugger*, struct ARMCore*);
void ARMDebuggerDeinit(struct ARMDebugger*);
void ARMDebuggerRun(struct ARMDebugger*);
void ARMDebuggerEnter(struct ARMDebugger*, enum DebuggerEntryReason);

#else

struct ARMDebugger {
	enum DebuggerState state;
};

#endif

#endif
