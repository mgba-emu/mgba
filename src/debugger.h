#ifndef DEBUGGER_H
#define DEBUGGER_H

enum DebuggerState {
	DEBUGGER_PAUSED,
	DEBUGGER_RUNNING,
	DEBUGGER_EXITING
};

struct ARMDebugger {
	enum DebuggerState state;
	struct ARMCore* cpu;
};

void ARMDebuggerInit(struct ARMDebugger*, struct ARMCore*);
void ARMDebuggerEnter(struct ARMDebugger*);

#endif
