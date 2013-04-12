#ifndef DEBUGGER_H
#define DEBUGGER_H

struct ARMDebugger {
	struct ARMCore* cpu;
};

void ARMDebuggerInit(struct ARMDebugger*, struct ARMCore*);
void ARMDebuggerEnter(struct ARMDebugger*);

#endif
