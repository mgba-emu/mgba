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

enum DebuggerLogLevel {
	DEBUGGER_LOG_DEBUG = 0x01,
	DEBUGGER_LOG_INFO = 0x02,
	DEBUGGER_LOG_WARN = 0x04,
	DEBUGGER_LOG_ERROR = 0x08
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

	__attribute__((format (printf, 3, 4)))
	void (*log)(struct ARMDebugger*, enum DebuggerLogLevel, const char* format, ...);
};

void ARMDebuggerInit(struct ARMDebugger*, struct ARMCore*);
void ARMDebuggerDeinit(struct ARMDebugger*);
void ARMDebuggerRun(struct ARMDebugger*);
void ARMDebuggerEnter(struct ARMDebugger*, enum DebuggerEntryReason);
void ARMDebuggerSetBreakpoint(struct ARMDebugger* debugger, uint32_t address);
void ARMDebuggerClearBreakpoint(struct ARMDebugger* debugger, uint32_t address);
void ARMDebuggerSetWatchpoint(struct ARMDebugger* debugger, uint32_t address);

#endif
