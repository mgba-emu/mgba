#include "cli-debugger.h"

#include <signal.h>

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

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

static const char* ERROR_MISSING_ARGS = "Arguments missing";

static struct CLIDebugger* _activeDebugger;

typedef void (DebuggerCommand)(struct CLIDebugger*, struct DebugVector*);

static void _breakInto(struct CLIDebugger*, struct DebugVector*);
static void _continue(struct CLIDebugger*, struct DebugVector*);
static void _next(struct CLIDebugger*, struct DebugVector*);
static void _print(struct CLIDebugger*, struct DebugVector*);
static void _printHex(struct CLIDebugger*, struct DebugVector*);
static void _printStatus(struct CLIDebugger*, struct DebugVector*);
static void _quit(struct CLIDebugger*, struct DebugVector*);
static void _readByte(struct CLIDebugger*, struct DebugVector*);
static void _readHalfword(struct CLIDebugger*, struct DebugVector*);
static void _readWord(struct CLIDebugger*, struct DebugVector*);
static void _setBreakpoint(struct CLIDebugger*, struct DebugVector*);
static void _clearBreakpoint(struct CLIDebugger*, struct DebugVector*);
static void _setWatchpoint(struct CLIDebugger*, struct DebugVector*);

static void _breakIntoDefault(int signal);

static struct {
	const char* name;
	DebuggerCommand* command;
} _debuggerCommands[] = {
	{ "b", _setBreakpoint },
	{ "break", _setBreakpoint },
	{ "c", _continue },
	{ "continue", _continue },
	{ "d", _clearBreakpoint },
	{ "delete", _clearBreakpoint },
	{ "i", _printStatus },
	{ "info", _printStatus },
	{ "n", _next },
	{ "next", _next },
	{ "p", _print },
	{ "p/x", _printHex },
	{ "print", _print },
	{ "print/x", _printHex },
	{ "q", _quit },
	{ "quit", _quit },
	{ "rb", _readByte },
	{ "rh", _readHalfword },
	{ "rw", _readWord },
	{ "status", _printStatus },
	{ "w", _setWatchpoint },
	{ "watch", _setWatchpoint },
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

static void _breakInto(struct CLIDebugger* debugger, struct DebugVector* dv) {
	(void)(debugger);
	(void)(dv);
	struct sigaction sa, osa;
	sa.sa_handler = _handleDeath;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGTRAP);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGTRAP, &sa, &osa);
#ifdef USE_PTHREADS
	pthread_kill(pthread_self(), SIGTRAP);
#else
	kill(getpid(), SIGTRAP);
#endif
	sigaction(SIGTRAP, &osa, 0);
}

static void _continue(struct CLIDebugger* debugger, struct DebugVector* dv) {
	(void)(dv);
	debugger->d.state = DEBUGGER_RUNNING;
}

static void _next(struct CLIDebugger* debugger, struct DebugVector* dv) {
	(void)(dv);
	ARMRun(debugger->d.cpu);
	_printStatus(debugger, 0);
}

static void _print(struct CLIDebugger* debugger, struct DebugVector* dv) {
	(void)(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" %u", dv->intValue);
	}
	printf("\n");
}

static void _printHex(struct CLIDebugger* debugger, struct DebugVector* dv) {
	(void)(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" 0x%08X", dv->intValue);
	}
	printf("\n");
}

static inline void _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	// TODO: write a disassembler
	if (mode == MODE_ARM) {
		uint32_t instruction = debugger->d.cpu->memory.load32(debugger->d.cpu, address, 0);
		printf("%08X\n", instruction);
	} else {
		uint16_t instruction = debugger->d.cpu->memory.loadU16(debugger->d.cpu, address, 0);
		printf("%04X\n", instruction);
	}
}

static void _printStatus(struct CLIDebugger* debugger, struct DebugVector* dv) {
	(void)(dv);
	int r;
	for (r = 0; r < 4; ++r) {
		printf("%08X %08X %08X %08X\n",
			debugger->d.cpu->gprs[r << 2],
			debugger->d.cpu->gprs[(r << 2) + 1],
			debugger->d.cpu->gprs[(r << 2) + 2],
			debugger->d.cpu->gprs[(r << 2) + 3]);
	}
	_printPSR(debugger->d.cpu->cpsr);
	int instructionLength;
	enum ExecutionMode mode = debugger->d.cpu->cpsr.t;
	if (mode == MODE_ARM) {
		instructionLength = WORD_SIZE_ARM;
	} else {
		instructionLength = WORD_SIZE_THUMB;
	}
	_printLine(debugger, debugger->d.cpu->gprs[ARM_PC] - instructionLength, mode);
}

static void _quit(struct CLIDebugger* debugger, struct DebugVector* dv) {
	(void)(dv);
	debugger->d.state = DEBUGGER_SHUTDOWN;
}

static void _readByte(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint8_t value = debugger->d.cpu->memory.loadU8(debugger->d.cpu, address, 0);
	printf(" 0x%02X\n", value);
}

static void _readHalfword(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint16_t value = debugger->d.cpu->memory.loadU16(debugger->d.cpu, address, 0);
	printf(" 0x%04X\n", value);
}

static void _readWord(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t value = debugger->d.cpu->memory.load32(debugger->d.cpu, address, 0);
	printf(" 0x%08X\n", value);
}

static void _setBreakpoint(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerSetBreakpoint(&debugger->d, address);
}

static void _clearBreakpoint(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerClearBreakpoint(&debugger->d, address);
}

static void _setWatchpoint(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerSetWatchpoint(&debugger->d, address);
}

static void _breakIntoDefault(int signal) {
	(void)(signal);
	ARMDebuggerEnter(&_activeDebugger->d, DEBUGGER_ENTER_MANUAL);
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

static struct DebugVector* _DVParse(struct CLIDebugger* debugger, const char* string, size_t length) {
	if (!string || length < 1) {
		return 0;
	}

	enum _DVParseState state = PARSE_ROOT;
	struct DebugVector dvTemp = { .type = INT_TYPE };
	uint32_t current = 0;

	while (length > 0 && string[0] && string[0] != ' ' && state != PARSE_ERROR) {
		char token = string[0];
		++string;
		--length;
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
				current = debugger->d.cpu->gprs[ARM_LR];
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
				current = debugger->d.cpu->gprs[ARM_PC];
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
				current = debugger->d.cpu->gprs[ARM_SP];
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
				current = debugger->d.cpu->gprs[token - '0'];
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
				current = debugger->d.cpu->gprs[token - '0' + 10];
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
			dv->next = _DVParse(debugger, string + 1, length - 1);
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

static int _parse(struct CLIDebugger* debugger, const char* line, size_t count) {
	const char* firstSpace = strchr(line, ' ');
	size_t cmdLength;
	struct DebugVector* dv = 0;
	if (firstSpace) {
		cmdLength = firstSpace - line;
		dv = _DVParse(debugger, firstSpace + 1, count - cmdLength - 1);
		if (dv && dv->type == ERROR_TYPE) {
			printf("Parse error\n");
			_DVFree(dv);
			return 0;
		}
	} else {
		cmdLength = count;
	}

	int i;
	const char* name;
	for (i = 0; (name = _debuggerCommands[i].name); ++i) {
		if (strlen(name) != cmdLength) {
			continue;
		}
		if (strncasecmp(name, line, cmdLength) == 0) {
			_debuggerCommands[i].command(debugger, dv);
			_DVFree(dv);
			return 1;
		}
	}
	_DVFree(dv);
	printf("Command not found\n");
	return 0;
}

static char* _prompt(EditLine* el) {
	(void)(el);
	return "> ";
}

static void _commandLine(struct ARMDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	const char* line;
	_printStatus(cliDebugger, 0);
	int count = 0;
	HistEvent ev;
	while (debugger->state == DEBUGGER_PAUSED) {
		line = el_gets(cliDebugger->elstate, &count);
		if (!line) {
			debugger->state = DEBUGGER_EXITING;
			return;
		}
		if (line[0] == '\n') {
			if (history(cliDebugger->histate, &ev, H_FIRST) >= 0) {
				_parse(cliDebugger, ev.str, strlen(ev.str) - 1);
			}
		} else {
			if (_parse(cliDebugger, line, count - 1)) {
				history(cliDebugger->histate, &ev, H_ENTER, line);
			}
		}
	}
}

static void _reportEntry(struct ARMDebugger* debugger, enum DebuggerEntryReason reason) {
	(void) (debugger);
	switch (reason) {
	case DEBUGGER_ENTER_MANUAL:
	case DEBUGGER_ENTER_ATTACHED:
		break;
	case DEBUGGER_ENTER_BREAKPOINT:
		printf("Hit breakpoint\n");
		break;
	case DEBUGGER_ENTER_WATCHPOINT:
		printf("Hit watchpoint\n");
		break;
	case DEBUGGER_ENTER_ILLEGAL_OP:
		printf("Hit illegal opcode\n");
		break;
	}
}

static unsigned char _tabComplete(EditLine* elstate, int ch) {
	(void)(ch);
	const LineInfo* li = el_line(elstate);
	const char* commandPtr;
	int cmd = 0, len = 0;
	const char* name = 0;
	for (commandPtr = li->buffer; commandPtr <= li->cursor; ++commandPtr, ++len) {
		for (; (name = _debuggerCommands[cmd].name); ++cmd) {
			int cmp = strncasecmp(name, li->buffer, len);
			if (cmp > 0) {
				return CC_ERROR;
			}
			if (cmp == 0) {
				break;
			}
		}
	}
	if (_debuggerCommands[cmd + 1].name && strncasecmp(_debuggerCommands[cmd + 1].name, li->buffer, len - 1) == 0) {
		return CC_ERROR;
	}
	name += len - 1;
	el_insertstr(elstate, name);
	el_insertstr(elstate, " ");
	return CC_REDISPLAY;
}

static void _cliDebuggerInit(struct ARMDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	// TODO: get argv[0]
	cliDebugger->elstate = el_init("gbac", stdin, stdout, stderr);
	el_set(cliDebugger->elstate, EL_PROMPT, _prompt);
	el_set(cliDebugger->elstate, EL_EDITOR, "emacs");

	el_set(cliDebugger->elstate, EL_CLIENTDATA, cliDebugger);
	el_set(cliDebugger->elstate, EL_ADDFN, "tab-complete", "Tab completion", _tabComplete);
	el_set(cliDebugger->elstate, EL_BIND, "\t", "tab-complete", 0);
	cliDebugger->histate = history_init();
	HistEvent ev;
	history(cliDebugger->histate, &ev, H_SETSIZE, 200);
	el_set(cliDebugger->elstate, EL_HIST, history, cliDebugger->histate);
	_activeDebugger = cliDebugger;
	signal(SIGINT, _breakIntoDefault);
}

static void _cliDebuggerDeinit(struct ARMDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	history_end(cliDebugger->histate);
	el_end(cliDebugger->elstate);
}

void CLIDebuggerCreate(struct CLIDebugger* debugger) {
	ARMDebuggerCreate(&debugger->d);
	debugger->d.init = _cliDebuggerInit;
	debugger->d.deinit = _cliDebuggerDeinit;
	debugger->d.paused = _commandLine;
	debugger->d.entered = _reportEntry;
}
