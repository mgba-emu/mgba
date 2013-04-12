#include "debugger.h"

#include "arm.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include "linenoise.h"

enum {
	CMD_QUIT,
	CMD_NEXT
};

static inline void _printPSR(union PSR psr) {
	printf("%08x [%c%c%c%c%c%c%c]\n", psr.packed,
		psr.n ? 'N' : '-',
		psr.z ? 'Z' : '-',
		psr.c ? 'C' : '-',
		psr.v ? 'V' : '-',
		psr.i ? 'I' : '-',
		psr.f ? 'F' : '-',
		psr.t ? 'T' : '-');
}

static void _printStatus(struct ARMDebugger* debugger) {
	int r;
	for (r = 0; r < 4; ++r) {
		printf("%08x %08x %08x %08x\n",
			debugger->cpu->gprs[r << 2],
			debugger->cpu->gprs[(r << 2) + 1],
			debugger->cpu->gprs[(r << 2) + 1],
			debugger->cpu->gprs[(r << 2) + 3]);
	}
	_printPSR(debugger->cpu->cpsr);
}

static int _parse(struct ARMDebugger* debugger, const char* line) {
	if (strcasecmp(line, "q") == 0 || strcasecmp(line, "quit") == 0) {
		return CMD_QUIT;
	}
	ARMRun(debugger->cpu);
	_printStatus(debugger);
	return CMD_NEXT;
}

void ARMDebuggerInit(struct ARMDebugger* debugger, struct ARMCore* cpu) {
	debugger->cpu = cpu;
}

void ARMDebuggerEnter(struct ARMDebugger* debugger) {
	char* line;
	_printStatus(debugger);
	while ((line = linenoise("> "))) {
		if (_parse(debugger, line) == CMD_QUIT) {
			break;
		}
		free(line);
	}
}
