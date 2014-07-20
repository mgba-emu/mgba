#ifndef MEMORY_DEBUGGER_H
#define MEMORY_DEBUGGER_H

#include "common.h"

#include "arm.h"

struct ARMDebugger;

void ARMDebuggerInstallMemoryShim(struct ARMDebugger* debugger);

#endif
