/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cli-debugger.h"
#include "decoder.h"
#include "parser.h"

#include <signal.h>

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

static const char* ERROR_MISSING_ARGS = "Arguments missing"; // TODO: share

static struct CLIDebugger* _activeDebugger;

static void _breakInto(struct CLIDebugger*, struct CLIDebugVector*);
static void _continue(struct CLIDebugger*, struct CLIDebugVector*);
static void _disassemble(struct CLIDebugger*, struct CLIDebugVector*);
static void _disassembleArm(struct CLIDebugger*, struct CLIDebugVector*);
static void _disassembleThumb(struct CLIDebugger*, struct CLIDebugVector*);
static void _next(struct CLIDebugger*, struct CLIDebugVector*);
static void _print(struct CLIDebugger*, struct CLIDebugVector*);
static void _printBin(struct CLIDebugger*, struct CLIDebugVector*);
static void _printHex(struct CLIDebugger*, struct CLIDebugVector*);
static void _printStatus(struct CLIDebugger*, struct CLIDebugVector*);
static void _printHelp(struct CLIDebugger*, struct CLIDebugVector*);
static void _quit(struct CLIDebugger*, struct CLIDebugVector*);
static void _readByte(struct CLIDebugger*, struct CLIDebugVector*);
static void _reset(struct CLIDebugger*, struct CLIDebugVector*);
static void _readHalfword(struct CLIDebugger*, struct CLIDebugVector*);
static void _readWord(struct CLIDebugger*, struct CLIDebugVector*);
static void _setBreakpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _clearBreakpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);

static void _breakIntoDefault(int signal);
static void _disassembleMode(struct CLIDebugger*, struct CLIDebugVector*, enum ExecutionMode mode);
static void _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode);

static struct CLIDebuggerCommandSummary _debuggerCommands[] = {
	{ "b", _setBreakpoint, CLIDVParse, "Set a breakpoint" },
	{ "break", _setBreakpoint, CLIDVParse, "Set a breakpoint" },
	{ "c", _continue, 0, "Continue execution" },
	{ "continue", _continue, 0, "Continue execution" },
	{ "d", _clearBreakpoint, CLIDVParse, "Delete a breakpoint" },
	{ "delete", _clearBreakpoint, CLIDVParse, "Delete a breakpoint" },
	{ "dis", _disassemble, CLIDVParse, "Disassemble instructions" },
	{ "dis/a", _disassembleArm, CLIDVParse, "Disassemble instructions as ARM" },
	{ "dis/t", _disassembleThumb, CLIDVParse, "Disassemble instructions as Thumb" },
	{ "disasm", _disassemble, CLIDVParse, "Disassemble instructions" },
	{ "disasm/a", _disassembleArm, CLIDVParse, "Disassemble instructions as ARM" },
	{ "disasm/t", _disassembleThumb, CLIDVParse, "Disassemble instructions as Thumb" },
	{ "disassemble", _disassemble, CLIDVParse, "Disassemble instructions" },
	{ "disassemble/a", _disassembleArm, CLIDVParse, "Disassemble instructions as ARM" },
	{ "disassemble/t", _disassembleThumb, CLIDVParse, "Disassemble instructions as Thumb" },
	{ "h", _printHelp, CLIDVStringParse, "Print help" },
	{ "help", _printHelp, CLIDVStringParse, "Print help" },
	{ "i", _printStatus, 0, "Print the current status" },
	{ "info", _printStatus, 0, "Print the current status" },
	{ "n", _next, 0, "Execute next instruction" },
	{ "next", _next, 0, "Execute next instruction" },
	{ "p", _print, CLIDVParse, "Print a value" },
	{ "p/t", _printBin, CLIDVParse, "Print a value as binary" },
	{ "p/x", _printHex, CLIDVParse, "Print a value as hexadecimal" },
	{ "print", _print, CLIDVParse, "Print a value" },
	{ "print/t", _printBin, CLIDVParse, "Print a value as binary" },
	{ "print/x", _printHex, CLIDVParse, "Print a value as hexadecimal" },
	{ "q", _quit, 0, "Quit the emulator" },
	{ "quit", _quit, 0, "Quit the emulator"  },
	{ "rb", _readByte, CLIDVParse, "Read a byte from a specified offset" },
	{ "reset", _reset, 0, "Reset the emulation" },
	{ "rh", _readHalfword, CLIDVParse, "Read a halfword from a specified offset" },
	{ "rw", _readWord, CLIDVParse, "Read a word from a specified offset" },
	{ "status", _printStatus, 0, "Print the current status" },
	{ "w", _setWatchpoint, CLIDVParse, "Set a watchpoint" },
	{ "watch", _setWatchpoint, CLIDVParse, "Set a watchpoint" },
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

static void _breakInto(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
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

static void _continue(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = DEBUGGER_RUNNING;
}

static void _next(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	ARMRun(debugger->d.cpu);
	_printStatus(debugger, 0);
}

static void _disassemble(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_disassembleMode(debugger, dv, debugger->d.cpu->executionMode);
}

static void _disassembleArm(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_disassembleMode(debugger, dv, MODE_ARM);
}

static void _disassembleThumb(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_disassembleMode(debugger, dv, MODE_THUMB);
}

static void _disassembleMode(struct CLIDebugger* debugger, struct CLIDebugVector* dv, enum ExecutionMode mode) {
	uint32_t address;
	int size;
	int wordSize;

	if (mode == MODE_ARM) {
		wordSize = WORD_SIZE_ARM;
	} else {
		wordSize = WORD_SIZE_THUMB;
	}

	if (!dv || dv->type != CLIDV_INT_TYPE) {
		address = debugger->d.cpu->gprs[ARM_PC] - wordSize;
	} else {
		address = dv->intValue;
		dv = dv->next;
	}

	if (!dv || dv->type != CLIDV_INT_TYPE) {
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

static void _print(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" %u", dv->intValue);
	}
	printf("\n");
}

static void _printBin(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
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

static void _printHex(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(debugger);
	for ( ; dv; dv = dv->next) {
		printf(" 0x%08X", dv->intValue);
	}
	printf("\n");
}

static void _printHelp(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(debugger);
	UNUSED(dv);
	if (!dv) {
		puts("ARM commands:");
		int i;
		for (i = 0; _debuggerCommands[i].name; ++i) {
			printf("%-10s %s\n", _debuggerCommands[i].name, _debuggerCommands[i].summary);
		}
		if (debugger->system) {
			printf("%s commands:\n", debugger->system->name);
			for (i = 0; debugger->system->commands[i].name; ++i) {
				printf("%-10s %s\n", debugger->system->commands[i].name, debugger->system->commands[i].summary);
			}
		}
	} else {
		int i;
		for (i = 0; _debuggerCommands[i].name; ++i) {
			if (strcmp(_debuggerCommands[i].name, dv->charValue) == 0) {
				printf(" %s\n", _debuggerCommands[i].summary);
			}
		}
		if (debugger->system) {
			printf("\n%s commands:\n", debugger->system->name);
			for (i = 0; debugger->system->commands[i].name; ++i) {
				if (strcmp(debugger->system->commands[i].name, dv->charValue) == 0) {
					printf(" %s\n", debugger->system->commands[i].summary);
				}
			}
		}
	}
}

static inline void _printLine(struct CLIDebugger* debugger, uint32_t address, enum ExecutionMode mode) {
	char disassembly[48];
	struct ARMInstructionInfo info;
	printf("%08X:  ", address);
	if (mode == MODE_ARM) {
		uint32_t instruction = debugger->d.cpu->memory.load32(debugger->d.cpu, address, 0);
		ARMDecodeARM(instruction, &info);
		ARMDisassemble(&info, address + WORD_SIZE_ARM * 2, disassembly, sizeof(disassembly));
		printf("%08X\t%s\n", instruction, disassembly);
	} else {
		uint16_t instruction = debugger->d.cpu->memory.loadU16(debugger->d.cpu, address, 0);
		ARMDecodeThumb(instruction, &info);
		ARMDisassemble(&info, address + WORD_SIZE_THUMB * 2, disassembly, sizeof(disassembly));
		printf("%04X\t%s\n", instruction, disassembly);
	}
}

static void _printStatus(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
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

static void _quit(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = DEBUGGER_SHUTDOWN;
}

static void _readByte(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint8_t value = debugger->d.cpu->memory.loadU8(debugger->d.cpu, address, 0);
	printf(" 0x%02X\n", value);
}

static void _reset(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	ARMReset(debugger->d.cpu);
	_printStatus(debugger, 0);
}

static void _readHalfword(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint16_t value = debugger->d.cpu->memory.loadU16(debugger->d.cpu, address, 0);
	printf(" 0x%04X\n", value);
}

static void _readWord(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t value = debugger->d.cpu->memory.load32(debugger->d.cpu, address, 0);
	printf(" 0x%08X\n", value);
}

static void _setBreakpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerSetBreakpoint(&debugger->d, address);
}

static void _clearBreakpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		printf("%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	ARMDebuggerClearBreakpoint(&debugger->d, address);
}

static void _setWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
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

static uint32_t _performOperation(enum Operation operation, uint32_t current, uint32_t next, struct CLIDebugVector* dv) {
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
			dv->type = CLIDV_ERROR_TYPE;
			return 0;
		}
		break;
	}
	return current;
}

static uint32_t _lookupIdentifier(struct ARMDebugger* debugger, const char* name, struct CLIDebugVector* dv) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
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
	if (cliDebugger->system) {
		uint32_t value = cliDebugger->system->lookupIdentifier(cliDebugger->system, name, dv);
		if (dv->type != CLIDV_ERROR_TYPE) {
			return value;
		}
	} else {
		dv->type = CLIDV_ERROR_TYPE;
	}
	return 0;
}

static uint32_t _evaluateParseTree(struct ARMDebugger* debugger, struct ParseTree* tree, struct CLIDebugVector* dv) {
	switch (tree->token.type) {
	case TOKEN_UINT_TYPE:
		return tree->token.uintValue;
	case TOKEN_OPERATOR_TYPE:
		return _performOperation(tree->token.operatorValue, _evaluateParseTree(debugger, tree->lhs, dv), _evaluateParseTree(debugger, tree->rhs, dv), dv);
	case TOKEN_IDENTIFIER_TYPE:
		return _lookupIdentifier(debugger, tree->token.identifierValue, dv);
	case TOKEN_ERROR_TYPE:
	default:
		dv->type = CLIDV_ERROR_TYPE;
	}
	return 0;
}

struct CLIDebugVector* CLIDVParse(struct CLIDebugger* debugger, const char* string, size_t length) {
	if (!string || length < 1) {
		return 0;
	}

	struct CLIDebugVector dvTemp = { .type = CLIDV_INT_TYPE };

	struct LexVector lv = { .next = 0 };
	size_t adjusted = lexExpression(&lv, string, length);
	if (adjusted > length) {
		dvTemp.type = CLIDV_ERROR_TYPE;
		lexFree(lv.next);
	}

	struct ParseTree tree;
	parseLexedExpression(&tree, &lv);
	if (tree.token.type == TOKEN_ERROR_TYPE) {
		dvTemp.type = CLIDV_ERROR_TYPE;
	} else {
		dvTemp.intValue = _evaluateParseTree(&debugger->d, &tree, &dvTemp);
	}

	parseFree(tree.lhs);
	parseFree(tree.rhs);

	length -= adjusted;
	string += adjusted;

	struct CLIDebugVector* dv = malloc(sizeof(struct CLIDebugVector));
	if (dvTemp.type == CLIDV_ERROR_TYPE) {
		dv->type = CLIDV_ERROR_TYPE;
		dv->next = 0;
	} else {
		*dv = dvTemp;
		if (string[0] == ' ') {
			dv->next = CLIDVParse(debugger, string + 1, length - 1);
			if (dv->next && dv->next->type == CLIDV_ERROR_TYPE) {
				dv->type = CLIDV_ERROR_TYPE;
			}
		}
	}
	return dv;
}

struct CLIDebugVector* CLIDVStringParse(struct CLIDebugger* debugger, const char* string, size_t length) {
	if (!string || length < 1) {
		return 0;
	}

	struct CLIDebugVector dvTemp = { .type = CLIDV_CHAR_TYPE };

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

	struct CLIDebugVector* dv = malloc(sizeof(struct CLIDebugVector));
	*dv = dvTemp;
	if (string[0] == ' ') {
		dv->next = CLIDVStringParse(debugger, string + 1, length - 1);
		if (dv->next && dv->next->type == CLIDV_ERROR_TYPE) {
			dv->type = CLIDV_ERROR_TYPE;
		}
	}
	return dv;
}

static void _DVFree(struct CLIDebugVector* dv) {
	struct CLIDebugVector* next;
	while (dv) {
		next = dv->next;
		if (dv->type == CLIDV_CHAR_TYPE) {
			free(dv->charValue);
		}
		free(dv);
		dv = next;
	}
}

static int _tryCommands(struct CLIDebugger* debugger, struct CLIDebuggerCommandSummary* commands, const char* command, size_t commandLen, const char* args, size_t argsLen) {
	struct CLIDebugVector* dv = 0;
	int i;
	const char* name;
	for (i = 0; (name = commands[i].name); ++i) {
		if (strlen(name) != commandLen) {
			continue;
		}
		if (strncasecmp(name, command, commandLen) == 0) {
			if (commands[i].parser) {
				if (args) {
					dv = commands[i].parser(debugger, args, argsLen);
					if (dv && dv->type == CLIDV_ERROR_TYPE) {
						printf("Parse error\n");
						_DVFree(dv);
						return false;
					}
				}
			} else if (args) {
				printf("Wrong number of arguments\n");
				return false;
			}
			commands[i].command(debugger, dv);
			_DVFree(dv);
			return true;
		}
	}
	return -1;
}

static bool _parse(struct CLIDebugger* debugger, const char* line, size_t count) {
	const char* firstSpace = strchr(line, ' ');
	size_t cmdLength;
	if (firstSpace) {
		cmdLength = firstSpace - line;
	} else {
		cmdLength = count;
	}

	const char* args = 0;
	if (firstSpace) {
		args = firstSpace + 1;
	}
	int result = _tryCommands(debugger, _debuggerCommands, line, cmdLength, args, count - cmdLength - 1);
	if (result < 0 && debugger->system) {
		result = _tryCommands(debugger, debugger->system->commands, line, cmdLength, args, count - cmdLength - 1);
	}
	if (result < 0) {
		printf("Command not found\n");
	}
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
	if (!li->buffer[0]) {
		return CC_ERROR;
	}

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
	if (!name) {
		return CC_ERROR;
	}
	if (_debuggerCommands[cmd + 1].name && name[len - 2] == _debuggerCommands[cmd + 1].name[len - 2]) {
		--len;
		const char* next = 0;
		int i;
		for (i = cmd + 1; _debuggerCommands[i].name; ++i) {
			if (strncasecmp(name, _debuggerCommands[i].name, len)) {
				break;
			}
			next = _debuggerCommands[i].name;
		}

		for (; name[len]; ++len) {
			if (name[len] != next[len]) {
				break;
			}
			char out[2] = { name[len], '\0' };
			el_insertstr(elstate, out);
		}
		return CC_REDISPLAY;
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

	if (cliDebugger->system) {
		cliDebugger->system->deinit(cliDebugger->system);
		free(cliDebugger->system);
		cliDebugger->system = 0;
	}
}

void CLIDebuggerCreate(struct CLIDebugger* debugger) {
	ARMDebuggerCreate(&debugger->d);
	debugger->d.init = _cliDebuggerInit;
	debugger->d.deinit = _cliDebuggerDeinit;
	debugger->d.paused = _commandLine;
	debugger->d.entered = _reportEntry;

	debugger->system = 0;
}

void CLIDebuggerAttachSystem(struct CLIDebugger* debugger, struct CLIDebuggerSystem* system) {
	if (debugger->system) {
		debugger->system->deinit(debugger->system);
		free(debugger->system);
	}

	debugger->system = system;
	system->p = debugger;
}
