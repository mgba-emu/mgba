#include "cli-debugger.h"
#include "decoder.h"
#include "parser.h"

#include <signal.h>

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

struct DebugVector {
	struct DebugVector* next;
	enum DVType {
		DV_ERROR_TYPE,
		DV_INT_TYPE,
		DV_CHAR_TYPE
	} type;
	union {
		int32_t intValue;
		char* charValue;
	};
};

static const char* ERROR_MISSING_ARGS = "Arguments missing";

static struct CLIDebugger* _activeDebugger;

typedef void (*DebuggerCommand)(struct CLIDebugger*, struct DebugVector*);
typedef struct DebugVector* (*DVParser)(struct CLIDebugger* debugger, const char* string, size_t length);

static struct DebugVector* _DVParse(struct CLIDebugger* debugger, const char* string, size_t length);
static struct DebugVector* _DVStringParse(struct CLIDebugger* debugger, const char* string, size_t length);

static void _breakInto(struct CLIDebugger*, struct DebugVector*);
static void _continue(struct CLIDebugger*, struct DebugVector*);
static void _disassemble(struct CLIDebugger*, struct DebugVector*);
static void _disassembleArm(struct CLIDebugger*, struct DebugVector*);
static void _disassembleThumb(struct CLIDebugger*, struct DebugVector*);
static void _next(struct CLIDebugger*, struct DebugVector*);
static void _print(struct CLIDebugger*, struct DebugVector*);
static void _printBin(struct CLIDebugger*, struct DebugVector*);
static void _printHex(struct CLIDebugger*, struct DebugVector*);
static void _printStatus(struct CLIDebugger*, struct DebugVector*);
static void _printHelp(struct CLIDebugger*, struct DebugVector*);
static void _quit(struct CLIDebugger*, struct DebugVector*);
static void _readByte(struct CLIDebugger*, struct DebugVector*);
static void _readHalfword(struct CLIDebugger*, struct DebugVector*);
static void _readWord(struct CLIDebugger*, struct DebugVector*);
static void _setBreakpoint(struct CLIDebugger*, struct DebugVector*);
static void _clearBreakpoint(struct CLIDebugger*, struct DebugVector*);
static void _setWatchpoint(struct CLIDebugger*, struct DebugVector*);

static void _breakIntoDefault(int signal);
static void _disassembleMode(struct CLIDebugger*, struct DebugVector*, enum ExecutionMode mode);
static void _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode);

static struct {
	const char* name;
	DebuggerCommand command;
	DVParser parser;
	const char* summary;
} _debuggerCommands[] = {
	{ "b", _setBreakpoint, _DVParse, "Set a breakpoint" },
	{ "break", _setBreakpoint, _DVParse, "Set a breakpoint" },
	{ "c", _continue, 0, "Continue execution" },
	{ "continue", _continue, 0, "Continue execution" },
	{ "d", _clearBreakpoint, _DVParse, "Delete a breakpoint" },
	{ "delete", _clearBreakpoint, _DVParse, "Delete a breakpoint" },
	{ "dis", _disassemble, _DVParse, "Disassemble instructions" },
	{ "dis/a", _disassembleArm, _DVParse, "Disassemble instructions as ARM" },
	{ "dis/t", _disassembleThumb, _DVParse, "Disassemble instructions as Thumb" },
	{ "disasm", _disassemble, _DVParse, "Disassemble instructions" },
	{ "disasm/a", _disassembleArm, _DVParse, "Disassemble instructions as ARM" },
	{ "disasm/t", _disassembleThumb, _DVParse, "Disassemble instructions as Thumb" },
	{ "h", _printHelp, _DVStringParse, "Print help" },
	{ "help", _printHelp, _DVStringParse, "Print help" },
	{ "i", _printStatus, 0, "Print the current status" },
	{ "info", _printStatus, 0, "Print the current status" },
	{ "n", _next, 0, "Execute next instruction" },
	{ "next", _next, 0, "Execute next instruction" },
	{ "p", _print, _DVParse, "Print a value" },
	{ "p/t", _printBin, _DVParse, "Print a value as binary" },
	{ "p/x", _printHex, _DVParse, "Print a value as hexadecimal" },
	{ "print", _print, _DVParse, "Print a value" },
	{ "print/t", _printBin, _DVParse, "Print a value as binary" },
	{ "print/x", _printHex, _DVParse, "Print a value as hexadecimal" },
	{ "q", _quit, 0, "Quit the emulator" },
	{ "quit", _quit, 0, "Quit the emulator"  },
	{ "rb", _readByte, _DVParse, "Read a byte from a specified offset" },
	{ "rh", _readHalfword, _DVParse, "Read a halfword from a specified offset" },
	{ "rw", _readWord, _DVParse, "Read a word from a specified offset" },
	{ "status", _printStatus, 0, "Print the current status" },
	{ "w", _setWatchpoint, _DVParse, "Set a watchpoint" },
	{ "watch", _setWatchpoint, _DVParse, "Set a watchpoint" },
	{ "x", _breakInto, 0, "Break into attached debugger (for developers)" },
	{ 0, 0, 0, 0 }
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
	UNUSED(sig);
	printf("No debugger attached!\n");
}

static void _breakInto(struct CLIDebugger* debugger, struct DebugVector* dv) {
	UNUSED(debugger);
	UNUSED(dv);
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
	UNUSED(dv);
	debugger->d.state = DEBUGGER_RUNNING;
}

static void _next(struct CLIDebugger* debugger, struct DebugVector* dv) {
	UNUSED(dv);
	ARMRun(debugger->d.cpu);
	_printStatus(debugger, 0);
}

static void _disassemble(struct CLIDebugger* debugger, struct DebugVector* dv) {
	_disassembleMode(debugger, dv, debugger->d.cpu->executionMode);
}

static void _disassembleArm(struct CLIDebugger* debugger, struct DebugVector* dv) {
	_disassembleMode(debugger, dv, MODE_ARM);
}

static void _disassembleThumb(struct CLIDebugger* debugger, struct DebugVector* dv) {
	_disassembleMode(debugger, dv, MODE_THUMB);
}

static void _disassembleMode(struct CLIDebugger* debugger, struct DebugVector* dv, enum ExecutionMode mode) {
	uint32_t address;
	int size;
	int wordSize;

	if (mode == MODE_ARM) {
		wordSize = WORD_SIZE_ARM;
	} else {
		wordSize = WORD_SIZE_THUMB;
	}

	if (!dv || dv->type != DV_INT_TYPE) {
		address = debugger->d.cpu->gprs[ARM_PC] - wordSize;
	} else {
		address = dv->intValue;
		dv = dv->next;
	}

	if (!dv || dv->type != DV_INT_TYPE) {
		size = 1;
	} else {
		size = dv->intValue;
		dv = dv->next; // TODO: Check for excess args
	}

	int i;
	for (i = 0; i < size; ++i) {
		_printLine(debugger, address, mode);
		address += wordSize;
	}
}

static void _print(struct CLIDebugger* debugger, struct DebugVector* dv) {
	UNUSED(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" %u", dv->intValue);
	}
	printf("\n");
}

static void _printBin(struct CLIDebugger* debugger, struct DebugVector* dv) {
	UNUSED(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" 0b");
		int i = 32;
		while (i--) {
			printf("%u", (dv->intValue >> i) & 1);
		}
	}
	printf("\n");
}

static void _printHex(struct CLIDebugger* debugger, struct DebugVector* dv) {
	UNUSED(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" 0x%08X", dv->intValue);
	}
	printf("\n");
}

static void _printHelp(struct CLIDebugger* debugger, struct DebugVector* dv) {
	UNUSED(debugger);
	UNUSED(dv);
	if (!dv) {
		int i;
		for (i = 0; _debuggerCommands[i].name; ++i) {
			printf("%-10s %s\n", _debuggerCommands[i].name, _debuggerCommands[i].summary);
		}
	} else {
		int i;
		for (i = 0; _debuggerCommands[i].name; ++i) {
			if (strcmp(_debuggerCommands[i].name, dv->charValue) == 0) {
				printf(" %s\n", _debuggerCommands[i].summary);
			}
		}
	}
}

static inline void _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	char disassembly[48];
	struct ARMInstructionInfo info;
	if (mode == MODE_ARM) {
		uint32_t instruction = debugger->d.cpu->memory.load32(debugger->d.cpu, address, 0);
		ARMDecodeARM(instruction, &info);
		ARMDisassemble(&info, address + WORD_SIZE_ARM * 2, disassembly, sizeof(disassembly));
		printf("%08X: %s\n", instruction, disassembly);
	} else {
		uint16_t instruction = debugger->d.cpu->memory.loadU16(debugger->d.cpu, address, 0);
		ARMDecodeThumb(instruction, &info);
		ARMDisassemble(&info, address + WORD_SIZE_THUMB * 2, disassembly, sizeof(disassembly));
		printf("%04X: %s\n", instruction, disassembly);
	}
}

static void _printStatus(struct CLIDebugger* debugger, struct DebugVector* dv) {
	UNUSED(dv);
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
	UNUSED(dv);
	debugger->d.state = DEBUGGER_SHUTDOWN;
}

static void _readByte(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != DV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint8_t value = debugger->d.cpu->memory.loadU8(debugger->d.cpu, address, 0);
	printf(" 0x%02X\n", value);
}

static void _readHalfword(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != DV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint16_t value = debugger->d.cpu->memory.loadU16(debugger->d.cpu, address, 0);
	printf(" 0x%04X\n", value);
}

static void _readWord(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != DV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t value = debugger->d.cpu->memory.load32(debugger->d.cpu, address, 0);
	printf(" 0x%08X\n", value);
}

static void _setBreakpoint(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != DV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerSetBreakpoint(&debugger->d, address);
}

static void _clearBreakpoint(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != DV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerClearBreakpoint(&debugger->d, address);
}

static void _setWatchpoint(struct CLIDebugger* debugger, struct DebugVector* dv) {
	if (!dv || dv->type != DV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerSetWatchpoint(&debugger->d, address);
}

static void _breakIntoDefault(int signal) {
	UNUSED(signal);
	ARMDebuggerEnter(&_activeDebugger->d, DEBUGGER_ENTER_MANUAL);
}

static uint32_t _performOperation(enum Operation operation, uint32_t current, uint32_t next, struct DebugVector* dv) {
	switch (operation) {
	case OP_ASSIGN:
		current = next;
		break;
	case OP_ADD:
		current += next;
		break;
	case OP_SUBTRACT:
		current -= next;
		break;
	case OP_MULTIPLY:
		current *= next;
		break;
	case OP_DIVIDE:
		if (next != 0) {
			current /= next;
		} else {
			dv->type = DV_ERROR_TYPE;
			return 0;
		}
		break;
	}
	return current;
}

static uint32_t _lookupIdentifier(struct ARMDebugger* debugger, const char* name, struct DebugVector* dv) {
	if (strcmp(name, "sp") == 0) {
		return debugger->cpu->gprs[ARM_SP];
	}
	if (strcmp(name, "lr") == 0) {
		return debugger->cpu->gprs[ARM_LR];
	}
	if (strcmp(name, "pc") == 0) {
		return debugger->cpu->gprs[ARM_PC];
	}
	if (strcmp(name, "cpsr") == 0) {
		return debugger->cpu->cpsr.packed;
	}
	// TODO: test if mode has SPSR
	if (strcmp(name, "spsr") == 0) {
		return debugger->cpu->spsr.packed;
	}
	if (name[0] == 'r' && name[1] >= '0' && name[1] <= '9') {
		int reg = atoi(&name[1]);
		if (reg < 16) {
			return debugger->cpu->gprs[reg];
		}
	}
	dv->type = DV_ERROR_TYPE;
	return 0;
}

static uint32_t _evaluateParseTree(struct ARMDebugger* debugger, struct ParseTree* tree, struct DebugVector* dv) {
	switch (tree->token.type) {
	case TOKEN_UINT_TYPE:
		return tree->token.uintValue;
	case TOKEN_OPERATOR_TYPE:
		return _performOperation(tree->token.operatorValue, _evaluateParseTree(debugger, tree->lhs, dv), _evaluateParseTree(debugger, tree->rhs, dv), dv);
	case TOKEN_IDENTIFIER_TYPE:
		return _lookupIdentifier(debugger, tree->token.identifierValue, dv);
	case TOKEN_ERROR_TYPE:
	default:
		dv->type = DV_ERROR_TYPE;
	}
	return 0;
}

static struct DebugVector* _DVParse(struct CLIDebugger* debugger, const char* string, size_t length) {
	if (!string || length < 1) {
		return 0;
	}

	struct DebugVector dvTemp = { .type = DV_INT_TYPE };

	struct LexVector lv = { .next = 0 };
	size_t adjusted = lexExpression(&lv, string, length);
	if (adjusted > length) {
		dvTemp.type = DV_ERROR_TYPE;
		lexFree(lv.next);
	}

	struct ParseTree tree;
	parseLexedExpression(&tree, &lv);
	if (tree.token.type == TOKEN_ERROR_TYPE) {
		dvTemp.type = DV_ERROR_TYPE;
	} else {
		dvTemp.intValue = _evaluateParseTree(&debugger->d, &tree, &dvTemp);
	}

	parseFree(tree.lhs);
	parseFree(tree.rhs);

	length -= adjusted;
	string += adjusted;

	struct DebugVector* dv = malloc(sizeof(struct DebugVector));
	if (dvTemp.type == DV_ERROR_TYPE) {
		dv->type = DV_ERROR_TYPE;
		dv->next = 0;
	} else {
		*dv = dvTemp;
		if (string[0] == ' ') {
			dv->next = _DVParse(debugger, string + 1, length - 1);
			if (dv->next && dv->next->type == DV_ERROR_TYPE) {
				dv->type = DV_ERROR_TYPE;
			}
		}
	}
	return dv;
}

static struct DebugVector* _DVStringParse(struct CLIDebugger* debugger, const char* string, size_t length) {
	if (!string || length < 1) {
		return 0;
	}

	struct DebugVector dvTemp = { .type = DV_CHAR_TYPE };

	size_t adjusted;
	const char* next = strchr(string, ' ');
	if (next) {
		adjusted = next - string;
	} else {
		adjusted = length;
	}
	dvTemp.charValue = malloc(adjusted);
	strncpy(dvTemp.charValue, string, adjusted);

	length -= adjusted;
	string += adjusted;

	struct DebugVector* dv = malloc(sizeof(struct DebugVector));
	*dv = dvTemp;
	if (string[0] == ' ') {
		dv->next = _DVStringParse(debugger, string + 1, length - 1);
		if (dv->next && dv->next->type == DV_ERROR_TYPE) {
			dv->type = DV_ERROR_TYPE;
		}
	}
	return dv;
}

static void _DVFree(struct DebugVector* dv) {
	struct DebugVector* next;
	while (dv) {
		next = dv->next;
		if (dv->type == DV_CHAR_TYPE) {
			free(dv->charValue);
		}
		free(dv);
		dv = next;
	}
}

static bool _parse(struct CLIDebugger* debugger, const char* line, size_t count) {
	const char* firstSpace = strchr(line, ' ');
	size_t cmdLength;
	struct DebugVector* dv = 0;
	if (firstSpace) {
		cmdLength = firstSpace - line;
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
			if (_debuggerCommands[i].parser) {
				if (firstSpace) {
					dv = _debuggerCommands[i].parser(debugger, firstSpace + 1, count - cmdLength - 1);
					if (dv && dv->type == DV_ERROR_TYPE) {
						printf("Parse error\n");
						_DVFree(dv);
						return false;
					}
				} else {
					printf("Wrong number of arguments");
				}
			} else if (firstSpace) {
				printf("Wrong number of arguments");
			}
			_debuggerCommands[i].command(debugger, dv);
			_DVFree(dv);
			return true;
		}
	}
	_DVFree(dv);
	printf("Command not found\n");
	return false;
}

static char* _prompt(EditLine* el) {
	UNUSED(el);
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
			_parse(cliDebugger, line, count - 1);
			history(cliDebugger->histate, &ev, H_ENTER, line);
		}
	}
}

static void _reportEntry(struct ARMDebugger* debugger, enum DebuggerEntryReason reason) {
	UNUSED(debugger);
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
	UNUSED(ch);
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
	cliDebugger->elstate = el_init(BINARY_NAME, stdin, stdout, stderr);
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
