#include "debugger.h"

#include "arm.h"

#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "linenoise.h"

struct DebugVector {
	struct DebugVector* next;
	enum DVType {
		ERROR_TYPE,
		INT_TYPE,
		CHAR_TYPE
	} type;
	union {
		int32_t intValue;
		const char* charValue;
	};
};

struct DebugBreakpoint {
	struct DebugBreakpoint* next;
	int32_t address;
};

static const char* ERROR_MISSING_ARGS = "Arguments missing";

static struct ARMDebugger* _activeDebugger;

typedef void (DebuggerComamnd)(struct ARMDebugger*, struct DebugVector*);

static void _breakInto(struct ARMDebugger*, struct DebugVector*);
static void _continue(struct ARMDebugger*, struct DebugVector*);
static void _next(struct ARMDebugger*, struct DebugVector*);
static void _print(struct ARMDebugger*, struct DebugVector*);
static void _printHex(struct ARMDebugger*, struct DebugVector*);
static void _printStatus(struct ARMDebugger*, struct DebugVector*);
static void _quit(struct ARMDebugger*, struct DebugVector*);
static void _readByte(struct ARMDebugger*, struct DebugVector*);
static void _readHalfword(struct ARMDebugger*, struct DebugVector*);
static void _readWord(struct ARMDebugger*, struct DebugVector*);
static void _setBreakpoint(struct ARMDebugger*, struct DebugVector*);

static void _breakIntoDefault(int signal);

struct {
	const char* name;
	DebuggerComamnd* command;
} debuggerCommands[] = {
	{ "b", _setBreakpoint },
	{ "break", _setBreakpoint },
	{ "c", _continue },
	{ "continue", _continue },
	{ "i", _printStatus },
	{ "info", _printStatus },
	{ "n", _next },
	{ "p", _print },
	{ "print", _print },
	{ "p/x", _printHex },
	{ "print/x", _printHex },
	{ "q", _quit },
	{ "quit", _quit },
	{ "rb", _readByte },
	{ "rh", _readHalfword },
	{ "rw", _readWord },
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

static void _breakInto(struct ARMDebugger* debugger, struct DebugVector* dv) {
	(void)(debugger);
	(void)(dv);
	sig_t oldSignal = signal(SIGTRAP, _handleDeath);
	kill(getpid(), SIGTRAP);
	signal(SIGTRAP, oldSignal);
}

static void _continue(struct ARMDebugger* debugger, struct DebugVector* dv) {
	(void)(dv);
	debugger->state = DEBUGGER_RUNNING;
}

static void _next(struct ARMDebugger* debugger, struct DebugVector* dv) {
	(void)(dv);
	ARMRun(debugger->cpu);
	_printStatus(debugger, 0);
}

static void _print(struct ARMDebugger* debugger, struct DebugVector* dv) {
	(void)(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" %u", dv->intValue);
	}
	printf("\n");
}

static void _printHex(struct ARMDebugger* debugger, struct DebugVector* dv) {
	(void)(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" 0x%08X", dv->intValue);
	}
	printf("\n");
}

static inline void _printLine(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	// TODO: write a disassembler
	if (mode == MODE_ARM) {
		uint32_t instruction = debugger->cpu->memory->load32(debugger->cpu->memory, address, 0);
		printf("%08X\n", instruction);
	} else {
		uint16_t instruction = debugger->cpu->memory->loadU16(debugger->cpu->memory, address, 0);
		printf("%04X\n", instruction);
	}
}

static void _printStatus(struct ARMDebugger* debugger, struct DebugVector* dv) {
	(void)(dv);
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

static void _quit(struct ARMDebugger* debugger, struct DebugVector* dv) {
	(void)(dv);
	debugger->state = DEBUGGER_EXITING;
}

static void _readByte(struct ARMDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint8_t value = debugger->cpu->memory->loadU8(debugger->cpu->memory, address, 0);
	printf(" 0x%02X\n", value);
}

static void _readHalfword(struct ARMDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint16_t value = debugger->cpu->memory->loadU16(debugger->cpu->memory, address, 0);
	printf(" 0x%04X\n", value);
}

static void _readWord(struct ARMDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t value = debugger->cpu->memory->load32(debugger->cpu->memory, address, 0);
	printf(" 0x%08X\n", value);
}

static void _setBreakpoint(struct ARMDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	struct DebugBreakpoint* breakpoint = malloc(sizeof(struct DebugBreakpoint));
	breakpoint->address = address;
	breakpoint->next = debugger->breakpoints;
	debugger->breakpoints = breakpoint;
}

static void _checkBreakpoints(struct ARMDebugger* debugger) {
	struct DebugBreakpoint* breakpoint;
	int instructionLength;
	enum ExecutionMode mode = debugger->cpu->cpsr.t;
	if (mode == MODE_ARM) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	for (breakpoint = debugger->breakpoints; breakpoint; breakpoint = breakpoint->next) {
		if (breakpoint->address + instructionLength == debugger->cpu->gprs[ARM_PC]) {
			debugger->state = DEBUGGER_PAUSED;
			printf("Hit breakpoint\n");
			break;
		}
	}
}

static void _breakIntoDefault(int signal) {
	(void)(signal);
	_activeDebugger->state = DEBUGGER_PAUSED;
}

enum _DVParseState {
	PARSE_ERROR = -1,
	PARSE_ROOT = 0,
	PARSE_EXPECT_REGISTER,
	PARSE_EXPECT_REGISTER_2,
	PARSE_EXPECT_LR,
	PARSE_EXPECT_PC,
	PARSE_EXPECT_SP,
	PARSE_EXPECT_DECIMAL,
	PARSE_EXPECT_HEX,
	PARSE_EXPECT_PREFIX,
	PARSE_EXPECT_SUFFIX,
};

static struct DebugVector* _DVParse(struct ARMDebugger* debugger, const char* string) {
	if (!string || !string[0]) {
		return 0;
	}

	enum _DVParseState state = PARSE_ROOT;
	struct DebugVector dvTemp = { .type = INT_TYPE };
	uint32_t current = 0;

	while (string[0] && string[0] != ' ' && state != PARSE_ERROR) {
		char token = string[0];
		++string;
		switch (state) {
		case PARSE_ROOT:
			switch (token) {
			case 'r':
				state = PARSE_EXPECT_REGISTER;
				break;
			case 'p':
				state = PARSE_EXPECT_PC;
				break;
			case 's':
				state = PARSE_EXPECT_SP;
				break;
			case 'l':
				state = PARSE_EXPECT_LR;
				break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				state = PARSE_EXPECT_DECIMAL;
				current = token - '0';
				break;
			case '0':
				state = PARSE_EXPECT_PREFIX;
				break;
			case '$':
				state = PARSE_EXPECT_HEX;
				current = 0;
				break;
			default:
				state = PARSE_ERROR;
				break;
			};
			break;
		case PARSE_EXPECT_LR:
			switch (token) {
			case 'r':
				current = debugger->cpu->gprs[ARM_LR];
				state = PARSE_EXPECT_SUFFIX;
				break;
			default:
				state = PARSE_ERROR;
				break;
			}
			break;
		case PARSE_EXPECT_PC:
			switch (token) {
			case 'c':
				current = debugger->cpu->gprs[ARM_PC];
				state = PARSE_EXPECT_SUFFIX;
				break;
			default:
				state = PARSE_ERROR;
				break;
			}
			break;
		case PARSE_EXPECT_SP:
			switch (token) {
			case 'p':
				current = debugger->cpu->gprs[ARM_SP];
				state = PARSE_EXPECT_SUFFIX;
				break;
			default:
				state = PARSE_ERROR;
				break;
			}
			break;
		case PARSE_EXPECT_REGISTER:
			switch (token) {
			case '0':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				current = debugger->cpu->gprs[token - '0'];
				state = PARSE_EXPECT_SUFFIX;
				break;
			case '1':
				state = PARSE_EXPECT_REGISTER_2;
				break;
			default:
				state = PARSE_ERROR;
				break;
			}
			break;
		case PARSE_EXPECT_REGISTER_2:
			switch (token) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
				current = debugger->cpu->gprs[token - '0' + 10];
				state = PARSE_EXPECT_SUFFIX;
				break;
			default:
				state = PARSE_ERROR;
				break;
			}
			break;
		case PARSE_EXPECT_DECIMAL:
			switch (token) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				// TODO: handle overflow
				current *= 10;
				current += token - '0';
				break;
			default:
				state = PARSE_ERROR;
			}
			break;
		case PARSE_EXPECT_HEX:
			switch (token) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				// TODO: handle overflow
				current *= 16;
				current += token - '0';
				break;
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
				// TODO: handle overflow
				current *= 16;
				current += token - 'A' + 10;
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
				// TODO: handle overflow
				current *= 16;
				current += token - 'a' + 10;
				break;
			default:
				state = PARSE_ERROR;
				break;
			}
			break;
		case PARSE_EXPECT_PREFIX:
			switch (token) {
			case 'X':
			case 'x':
				current = 0;
				state = PARSE_EXPECT_HEX;
				break;
			default:
				state = PARSE_ERROR;
				break;
			}
			break;
		case PARSE_EXPECT_SUFFIX:
			// TODO
			state = PARSE_ERROR;
			break;
		case PARSE_ERROR:
			// This shouldn't be reached
			break;
		}
	}

	struct DebugVector* dv = malloc(sizeof(struct DebugVector));
	if (state == PARSE_ERROR) {
		dv->type = ERROR_TYPE;
		dv->next = 0;
	} else {
		dvTemp.intValue = current;
		*dv = dvTemp;
		if (string[0] == ' ') {
			dv->next = _DVParse(debugger, string + 1);
		}
	}
	return dv;
}

static void _DVFree(struct DebugVector* dv) {
	struct DebugVector* next;
	while (dv) {
		next = dv->next;
		free(dv);
		dv = next;
	}
}

static int _parse(struct ARMDebugger* debugger, const char* line) {
	char* firstSpace = strchr(line, ' ');
	size_t cmdLength;
	struct DebugVector* dv = 0;
	if (firstSpace) {
		cmdLength = firstSpace - line;
		dv = _DVParse(debugger, firstSpace + 1);
		if (dv && dv->type == ERROR_TYPE) {
			printf("Parse error\n");
			_DVFree(dv);
			return 0;
		}
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
			debuggerCommands[i].command(debugger, dv);
			_DVFree(dv);
			return 1;
		}
	}
	_DVFree(dv);
	printf("Command not found\n");
	return 0;
}

static void _commandLine(struct ARMDebugger* debugger) {
	char* line;
	_printStatus(debugger, 0);
	while (debugger->state == DEBUGGER_PAUSED) {
		line = linenoise("> ");
		if (!line) {
			debugger->state = DEBUGGER_EXITING;
			return;
		}
		if (!line[0]) {
			if (debugger->lastCommand) {
				_parse(debugger, debugger->lastCommand);
			}
		} else {
			linenoiseHistoryAdd(line);
			if (_parse(debugger, line)) {
				char* oldLine = debugger->lastCommand;
				debugger->lastCommand = line;
				free(oldLine);
			} else {
				free(line);
			}
		}
	}
}

void ARMDebuggerInit(struct ARMDebugger* debugger, struct ARMCore* cpu) {
	debugger->cpu = cpu;
	debugger->state = DEBUGGER_PAUSED;
	debugger->lastCommand = 0;
	debugger->breakpoints = 0;
	_activeDebugger = debugger;
	signal(SIGINT, _breakIntoDefault);
}

void ARMDebuggerRun(struct ARMDebugger* debugger) {
	while (debugger->state != DEBUGGER_EXITING) {
		if (!debugger->breakpoints) {
			while (debugger->state == DEBUGGER_RUNNING) {
				ARMRun(debugger->cpu);
			}
		} else {
			while (debugger->state == DEBUGGER_RUNNING) {
				ARMRun(debugger->cpu);
				_checkBreakpoints(debugger);
			}
		}
		switch (debugger->state) {
		case DEBUGGER_PAUSED:
			_commandLine(debugger);
			break;
		case DEBUGGER_EXITING:
			return;
		default:
			// Should never be reached
			break;
		}
	}
}

void ARMDebuggerEnter(struct ARMDebugger* debugger) {
	debugger->state = DEBUGGER_PAUSED;
}
