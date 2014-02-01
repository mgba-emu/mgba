#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "arm.h"

enum DebuggerState {
	DEBUGGER_PAUSED,
	DEBUGGER_RUNNING,
	DEBUGGER_EXITING,
	DEBUGGER_SHUTDOWN
};

struct DebugBreakpoint {
	struct DebugBreakpoint* next;
	uint32_t address;
};

struct DebugMemoryShim {
	struct ARMMemory d;
	struct ARMMemory* original;

	struct ARMDebugger* p;
	struct DebugBreakpoint* watchpoints;
};

enum DebuggerEntryReason {
	DEBUGGER_ENTER_MANUAL,
	DEBUGGER_ENTER_ATTACHED,
	DEBUGGER_ENTER_BREAKPOINT,
	DEBUGGER_ENTER_WATCHPOINT,
	DEBUGGER_ENTER_ILLEGAL_OP
};

struct ARMDebugger {
	enum DebuggerState state;
	struct ARMCore* cpu;

	struct DebugBreakpoint* breakpoints;
	struct DebugMemoryShim memoryShim;

	void (*init)(struct ARMDebugger*);
	void (*deinit)(struct ARMDebugger*);
	void (*paused)(struct ARMDebugger*);
	void (*entered)(struct ARMDebugger*, enum DebuggerEntryReason);
};

void ARMDebuggerInit(struct ARMDebugger*, struct ARMCore*);
void ARMDebuggerDeinit(struct ARMDebugger*);
void ARMDebuggerRun(struct ARMDebugger*);
void ARMDebuggerEnter(struct ARMDebugger*, enum DebuggerEntryReason);
void ARMDebuggerSetBreakpoint(struct ARMDebugger* debugger, uint32_t address);
void ARMDebuggerClearBreakpoint(struct ARMDebugger* debugger, uint32_t address);
void ARMDebuggerSetWatchpoint(struct ARMDebugger* debugger, uint32_t address);

#endif
