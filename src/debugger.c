#include "debugger.h"

#include "arm.h"

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include "linenoise.h"

typedef void (DebuggerComamnd)(struct ARMDebugger*);

static void _breakInto(struct ARMDebugger*);
static void _printStatus(struct ARMDebugger*);
static void _quit(struct ARMDebugger*);

struct {
	const char* name;
	DebuggerComamnd* command;
} debuggerCommands[] = {
	{ "i", _printStatus },
	{ "info", _printStatus },
	{ "q", _quit },
	{ "quit", _quit },
	{ "status", _printStatus },
	{ "x", _breakInto },
	{ 0, 0 }
};

static inline void _printPSR(union PSR psr) {
	printf("%08X [%c%c%c%c%c%c%c]\n", psr.packed,
		psr.n ? 'N' : '-',
		psr.z ? 'Z' : '-',
		psr.c ? 'C' : '-',
		psr.v ? 'V' : '-',
		psr.i ? 'I' : '-',
		psr.f ? 'F' : '-',
		psr.t ? 'T' : '-');
}

static void _handleDeath(int sig) {
	(void)(sig);
	printf("No debugger attached!\n");
}

static void _breakInto(struct ARMDebugger* debugger) {
	(void)(debugger);
	sig_t oldSignal = signal(SIGTRAP, _handleDeath);
	kill(getpid(), SIGTRAP);
	signal(SIGTRAP, oldSignal);
}

static inline void _printLine(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	// TODO: write a disassembler
	if (mode == MODE_ARM) {
		uint32_t instruction = debugger->cpu->memory->load32(debugger->cpu->memory, address);
		printf("%08X\n", instruction);
	} else {
		uint16_t instruction = debugger->cpu->memory->loadU16(debugger->cpu->memory, address);
		printf("%04X\n", instruction);
	}
}

static void _printStatus(struct ARMDebugger* debugger) {
	int r;
	for (r = 0; r < 4; ++r) {
		printf("%08X %08X %08X %08X\n",
			debugger->cpu->gprs[r << 2],
			debugger->cpu->gprs[(r << 2) + 1],
			debugger->cpu->gprs[(r << 2) + 2],
			debugger->cpu->gprs[(r << 2) + 3]);
	}
	_printPSR(debugger->cpu->cpsr);
	int instructionLength;
	enum ExecutionMode mode = debugger->cpu->cpsr.t;
	if (mode == MODE_ARM) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	_printLine(debugger, debugger->cpu->gprs[ARM_PC] - instructionLength, mode);
}

static void _quit(struct ARMDebugger* debugger) {
	debugger->state = DEBUGGER_EXITING;
}

static void _parse(struct ARMDebugger* debugger, const char* line) {
	char* firstSpace = strchr(line, ' ');
	size_t cmdLength;
	if (firstSpace) {
		cmdLength = line - firstSpace;
	} else {
		cmdLength = strlen(line);
	}

	int i;
	const char* name;
	for (i = 0; (name = debuggerCommands[i].name); ++i) {
		if (strlen(name) != cmdLength) {
			continue;
		}
		if (strncasecmp(name, line, cmdLength) == 0) {
			debuggerCommands[i].command(debugger);
			return;
		}
	}
	ARMRun(debugger->cpu);
	_printStatus(debugger);
}

void ARMDebuggerInit(struct ARMDebugger* debugger, struct ARMCore* cpu) {
	debugger->cpu = cpu;
}

void ARMDebuggerEnter(struct ARMDebugger* debugger) {
	char* line;
	_printStatus(debugger);
	while ((line = linenoise("> "))) {
		_parse(debugger, line);
		free(line);
		switch (debugger->state) {
		case DEBUGGER_EXITING:
			return;
		default:
			break;
		}
	}
}
