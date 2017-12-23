/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/cli-debugger.h>

#include <mgba/internal/debugger/symbols.h>

#include <mgba/core/core.h>
#include <mgba/core/version.h>
#include <mgba/internal/debugger/parser.h>
#include <mgba-util/string.h>

#if !defined(NDEBUG) && !defined(_WIN32)
#include <signal.h>
#endif

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

const char* ERROR_MISSING_ARGS = "Arguments missing"; // TODO: share
const char* ERROR_OVERFLOW = "Arguments overflow";

#if !defined(NDEBUG) && !defined(_WIN32)
static void _breakInto(struct CLIDebugger*, struct CLIDebugVector*);
#endif
static void _continue(struct CLIDebugger*, struct CLIDebugVector*);
static void _disassemble(struct CLIDebugger*, struct CLIDebugVector*);
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
static void _setReadWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setWriteWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _trace(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeByte(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeHalfword(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeWord(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpByte(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpHalfword(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpWord(struct CLIDebugger*, struct CLIDebugVector*);

static struct CLIDebuggerCommandSummary _debuggerCommands[] = {
	{ "b", _setBreakpoint, CLIDVParse, "Set a breakpoint" },
	{ "break", _setBreakpoint, CLIDVParse, "Set a breakpoint" },
	{ "c", _continue, 0, "Continue execution" },
	{ "continue", _continue, 0, "Continue execution" },
	{ "d", _clearBreakpoint, CLIDVParse, "Delete a breakpoint" },
	{ "delete", _clearBreakpoint, CLIDVParse, "Delete a breakpoint" },
	{ "dis", _disassemble, CLIDVParse, "Disassemble instructions" },
	{ "disasm", _disassemble, CLIDVParse, "Disassemble instructions" },
	{ "disassemble", _disassemble, CLIDVParse, "Disassemble instructions" },
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
	{ "quit", _quit, 0, "Quit the emulator" },
	{ "reset", _reset, 0, "Reset the emulation" },
	{ "r/1", _readByte, CLIDVParse, "Read a byte from a specified offset" },
	{ "r/2", _readHalfword, CLIDVParse, "Read a halfword from a specified offset" },
	{ "r/4", _readWord, CLIDVParse, "Read a word from a specified offset" },
	{ "status", _printStatus, 0, "Print the current status" },
	{ "trace", _trace, CLIDVParse, "Trace a fixed number of instructions" },
	{ "w", _setWatchpoint, CLIDVParse, "Set a watchpoint" },
	{ "w/1", _writeByte, CLIDVParse, "Write a byte at a specified offset" },
	{ "w/2", _writeHalfword, CLIDVParse, "Write a halfword at a specified offset" },
	{ "w/4", _writeWord, CLIDVParse, "Write a word at a specified offset" },
	{ "watch", _setWatchpoint, CLIDVParse, "Set a watchpoint" },
	{ "watch/r", _setReadWatchpoint, CLIDVParse, "Set a read watchpoint" },
	{ "watch/w", _setWriteWatchpoint, CLIDVParse, "Set a write watchpoint" },
	{ "x/1", _dumpByte, CLIDVParse, "Examine bytes at a specified offset" },
	{ "x/2", _dumpHalfword, CLIDVParse, "Examine halfwords at a specified offset" },
	{ "x/4", _dumpWord, CLIDVParse, "Examine words at a specified offset" },
#if !defined(NDEBUG) && !defined(_WIN32)
	{ "!", _breakInto, 0, "Break into attached debugger (for developers)" },
#endif
	{ 0, 0, 0, 0 }
};

#if !defined(NDEBUG) && !defined(_WIN32)
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
#endif

static void _continue(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = DEBUGGER_RUNNING;
}

static void _next(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.core->step(debugger->d.core);
	_printStatus(debugger, 0);
}

static void _disassemble(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	debugger->system->disassemble(debugger->system, dv);
}

static void _print(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	for (; dv; dv = dv->next) {
		if (dv->segmentValue >= 0) {
			debugger->backend->printf(debugger->backend, " $%02X:%04X", dv->segmentValue, dv->intValue);
			continue;
		}
		debugger->backend->printf(debugger->backend, " %u", dv->intValue);
	}
	debugger->backend->printf(debugger->backend, "\n");
}

static void _printBin(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	for (; dv; dv = dv->next) {
		debugger->backend->printf(debugger->backend, " 0b");
		int i = 32;
		while (i--) {
			debugger->backend->printf(debugger->backend, "%u", (dv->intValue >> i) & 1);
		}
	}
	debugger->backend->printf(debugger->backend, "\n");
}

static void _printHex(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	for (; dv; dv = dv->next) {
		debugger->backend->printf(debugger->backend, " 0x%08X", dv->intValue);
	}
	debugger->backend->printf(debugger->backend, "\n");
}

static void _printHelp(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	if (!dv) {
		debugger->backend->printf(debugger->backend, "Generic commands:\n");
		int i;
		for (i = 0; _debuggerCommands[i].name; ++i) {
			debugger->backend->printf(debugger->backend, "%-10s %s\n", _debuggerCommands[i].name, _debuggerCommands[i].summary);
		}
		if (debugger->system) {
			debugger->backend->printf(debugger->backend, "%s commands:\n", debugger->system->platformName);
			for (i = 0; debugger->system->platformCommands[i].name; ++i) {
				debugger->backend->printf(debugger->backend, "%-10s %s\n", debugger->system->platformCommands[i].name, debugger->system->platformCommands[i].summary);
			}
			debugger->backend->printf(debugger->backend, "%s commands:\n", debugger->system->name);
			for (i = 0; debugger->system->commands[i].name; ++i) {
				debugger->backend->printf(debugger->backend, "%-10s %s\n", debugger->system->commands[i].name, debugger->system->commands[i].summary);
			}
		}
	} else {
		int i;
		for (i = 0; _debuggerCommands[i].name; ++i) {
			if (strcmp(_debuggerCommands[i].name, dv->charValue) == 0) {
				debugger->backend->printf(debugger->backend, " %s\n", _debuggerCommands[i].summary);
			}
		}
		if (debugger->system) {
			for (i = 0; debugger->system->platformCommands[i].name; ++i) {
				if (strcmp(debugger->system->platformCommands[i].name, dv->charValue) == 0) {
					debugger->backend->printf(debugger->backend, " %s\n", debugger->system->platformCommands[i].summary);
				}
			}
			for (i = 0; debugger->system->commands[i].name; ++i) {
				if (strcmp(debugger->system->commands[i].name, dv->charValue) == 0) {
					debugger->backend->printf(debugger->backend, " %s\n", debugger->system->commands[i].summary);
				}
			}
		}
	}
}

static void _quit(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = DEBUGGER_SHUTDOWN;
}

static void _readByte(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint8_t value;
	if (dv->segmentValue >= 0) {
		value = debugger->d.core->rawRead8(debugger->d.core, address, dv->segmentValue);
	} else {
		value = debugger->d.core->busRead8(debugger->d.core, address);
	}
	debugger->backend->printf(debugger->backend, " 0x%02X\n", value);
}

static void _reset(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.core->reset(debugger->d.core);
	_printStatus(debugger, 0);
}

static void _readHalfword(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint16_t value;
	if (dv->segmentValue >= 0) {
		value = debugger->d.core->rawRead16(debugger->d.core, address & -1, dv->segmentValue);
	} else {
		value = debugger->d.core->busRead16(debugger->d.core, address & ~1);
	}
	debugger->backend->printf(debugger->backend, " 0x%04X\n", value);
}

static void _readWord(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t value;
	if (dv->segmentValue >= 0) {
		value = debugger->d.core->rawRead32(debugger->d.core, address & -3, dv->segmentValue);
	} else {
		value = debugger->d.core->busRead32(debugger->d.core, address & ~3);
	}
	debugger->backend->printf(debugger->backend, " 0x%08X\n", value);
}

static void _writeByte(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!dv->next || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t value = dv->next->intValue;
	if (value > 0xFF) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_OVERFLOW);
		return;
	}
	if (dv->segmentValue >= 0) {
		debugger->d.core->rawWrite8(debugger->d.core, address, value, dv->segmentValue);
	} else {
		debugger->d.core->busWrite8(debugger->d.core, address, value);
	}
}

static void _writeHalfword(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!dv->next || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t value = dv->next->intValue;
	if (value > 0xFFFF) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_OVERFLOW);
		return;
	}
	if (dv->segmentValue >= 0) {
		debugger->d.core->rawWrite16(debugger->d.core, address, value, dv->segmentValue);
	} else {
		debugger->d.core->busWrite16(debugger->d.core, address, value);
	}
}

static void _writeWord(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!dv->next || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t value = dv->next->intValue;
	if (dv->segmentValue >= 0) {
		debugger->d.core->rawWrite32(debugger->d.core, address, value, dv->segmentValue);
	} else {
		debugger->d.core->busWrite32(debugger->d.core, address, value);
	}
}

static void _dumpByte(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t words = 16;
	if (dv->next && dv->next->type == CLIDV_INT_TYPE) {
		words = dv->next->intValue;
	}
	while (words) {
		uint32_t line = 16;
		if (line > words) {
			line = words;
		}
		debugger->backend->printf(debugger->backend, "0x%08X:", address);
		for (; line > 0; --line, ++address, --words) {
			uint32_t value;
			if (dv->segmentValue >= 0) {
				value = debugger->d.core->rawRead8(debugger->d.core, address, dv->segmentValue);
			} else {
				value = debugger->d.core->busRead8(debugger->d.core, address);
			}
			debugger->backend->printf(debugger->backend, " %02X", value);
		}
		debugger->backend->printf(debugger->backend, "\n");
	}
}

static void _dumpHalfword(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t words = 8;
	if (dv->next && dv->next->type == CLIDV_INT_TYPE) {
		words = dv->next->intValue;
	}
	while (words) {
		uint32_t line = 8;
		if (line > words) {
			line = words;
		}
		debugger->backend->printf(debugger->backend, "0x%08X:", address);
		for (; line > 0; --line, address += 2, --words) {
			uint32_t value;
			if (dv->segmentValue >= 0) {
				value = debugger->d.core->rawRead16(debugger->d.core, address, dv->segmentValue);
			} else {
				value = debugger->d.core->busRead16(debugger->d.core, address);
			}
			debugger->backend->printf(debugger->backend, " %04X", value);
		}
		debugger->backend->printf(debugger->backend, "\n");
	}
}

static void _dumpWord(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	uint32_t words = 4;
	if (dv->next && dv->next->type == CLIDV_INT_TYPE) {
		words = dv->next->intValue;
	}
	while (words) {
		uint32_t line = 4;
		if (line > words) {
			line = words;
		}
		debugger->backend->printf(debugger->backend, "0x%08X:", address);
		for (; line > 0; --line, address += 4, --words) {
			uint32_t value;
			if (dv->segmentValue >= 0) {
				value = debugger->d.core->rawRead32(debugger->d.core, address, dv->segmentValue);
			} else {
				value = debugger->d.core->busRead32(debugger->d.core, address);
			}
			debugger->backend->printf(debugger->backend, " %08X", value);
		}
		debugger->backend->printf(debugger->backend, "\n");
	}
}

static void _setBreakpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	debugger->d.platform->setBreakpoint(debugger->d.platform, address, dv->segmentValue);
}

static void _setWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!debugger->d.platform->setWatchpoint) {
		debugger->backend->printf(debugger->backend, "Watchpoints are not supported by this platform.\n");
		return;
	}
	uint32_t address = dv->intValue;
	debugger->d.platform->setWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_RW);
}

static void _setReadWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!debugger->d.platform->setWatchpoint) {
		debugger->backend->printf(debugger->backend, "Watchpoints are not supported by this platform.\n");
		return;
	}
	uint32_t address = dv->intValue;
	debugger->d.platform->setWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_READ);
}

static void _setWriteWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!debugger->d.platform->setWatchpoint) {
		debugger->backend->printf(debugger->backend, "Watchpoints are not supported by this platform.\n");
		return;
	}
	uint32_t address = dv->intValue;
	debugger->d.platform->setWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_WRITE);
}

static void _clearBreakpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint32_t address = dv->intValue;
	debugger->d.platform->clearBreakpoint(debugger->d.platform, address, dv->segmentValue);
	if (debugger->d.platform->clearWatchpoint) {
		debugger->d.platform->clearWatchpoint(debugger->d.platform, address, dv->segmentValue);
	}
}

static void _trace(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}

	char trace[1024];
	trace[sizeof(trace) - 1] = '\0';

	int i;
	for (i = 0; i < dv->intValue; ++i) {
		debugger->d.core->step(debugger->d.core);
		size_t traceSize = sizeof(trace) - 1;
		debugger->d.platform->trace(debugger->d.platform, trace, &traceSize);
		if (traceSize + 1 < sizeof(trace)) {
			trace[traceSize + 1] = '\0';
		}
		debugger->backend->printf(debugger->backend, "%s\n", trace);
	}
}

static void _printStatus(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->system->printStatus(debugger->system);
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

static void _lookupIdentifier(struct mDebugger* debugger, const char* name, struct CLIDebugVector* dv) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	if (cliDebugger->system) {
		uint32_t value;
		if (debugger->core->symbolTable && mDebuggerSymbolLookup(debugger->core->symbolTable, name, &dv->intValue, &dv->segmentValue)) {
			return;
		}
		value = cliDebugger->system->lookupPlatformIdentifier(cliDebugger->system, name, dv);
		if (dv->type != CLIDV_ERROR_TYPE) {
			dv->intValue = value;
			return;
		}
		dv->type = CLIDV_INT_TYPE;
		value = cliDebugger->system->lookupIdentifier(cliDebugger->system, name, dv);
		if (dv->type != CLIDV_ERROR_TYPE) {
			dv->intValue = value;
			return;
		}
	}
	dv->type = CLIDV_ERROR_TYPE;
}

static void _evaluateParseTree(struct mDebugger* debugger, struct ParseTree* tree, struct CLIDebugVector* dv) {
	int32_t lhs, rhs;
	switch (tree->token.type) {
	case TOKEN_UINT_TYPE:
		dv->intValue = tree->token.uintValue;
		break;
	case TOKEN_SEGMENT_TYPE:
		_evaluateParseTree(debugger, tree->lhs, dv);
		dv->segmentValue = dv->intValue;
		_evaluateParseTree(debugger, tree->rhs, dv);
		break;
	case TOKEN_OPERATOR_TYPE:
		_evaluateParseTree(debugger, tree->lhs, dv);
		lhs = dv->intValue;
		_evaluateParseTree(debugger, tree->rhs, dv);
		rhs = dv->intValue;
		dv->intValue = _performOperation(tree->token.operatorValue, lhs, rhs, dv);
		break;
	case TOKEN_IDENTIFIER_TYPE:
		_lookupIdentifier(debugger, tree->token.identifierValue, dv);
		break;
	case TOKEN_ERROR_TYPE:
	default:
		dv->type = CLIDV_ERROR_TYPE;
	}
}

struct CLIDebugVector* CLIDVParse(struct CLIDebugger* debugger, const char* string, size_t length) {
	if (!string || length < 1) {
		return 0;
	}

	struct CLIDebugVector dvTemp = { .type = CLIDV_INT_TYPE, .segmentValue = -1 };

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
		_evaluateParseTree(&debugger->d, &tree, &dvTemp);
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
	dvTemp.charValue = strndup(string, adjusted);

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
						debugger->backend->printf(debugger->backend, "Parse error\n");
						_DVFree(dv);
						return false;
					}
				}
			} else if (args) {
				debugger->backend->printf(debugger->backend, "Wrong number of arguments\n");
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
		if (result < 0) {
			result = _tryCommands(debugger, debugger->system->platformCommands, line, cmdLength, args, count - cmdLength - 1);
		}
	}
	if (result < 0) {
		debugger->backend->printf(debugger->backend, "Command not found\n");
	}
	return false;
}

static void _commandLine(struct mDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	const char* line;
		size_t len;
	_printStatus(cliDebugger, 0);
	while (debugger->state == DEBUGGER_PAUSED) {
		line = cliDebugger->backend->readline(cliDebugger->backend, &len);
		if (!line || len == 0) {
			debugger->state = DEBUGGER_SHUTDOWN;
			return;
		}
		if (line[0] == '\n') {
			line = cliDebugger->backend->historyLast(cliDebugger->backend, &len);
			if (line && len) {
				_parse(cliDebugger, line, len);
			}
		} else {
			_parse(cliDebugger, line, len);
			cliDebugger->backend->historyAppend(cliDebugger->backend, line);
		}
	}
}

static void _reportEntry(struct mDebugger* debugger, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	switch (reason) {
	case DEBUGGER_ENTER_MANUAL:
	case DEBUGGER_ENTER_ATTACHED:
		break;
	case DEBUGGER_ENTER_BREAKPOINT:
		if (info) {
			cliDebugger->backend->printf(cliDebugger->backend, "Hit breakpoint at 0x%08X\n", info->address);
		} else {
			cliDebugger->backend->printf(cliDebugger->backend, "Hit breakpoint\n");
		}
		break;
	case DEBUGGER_ENTER_WATCHPOINT:
		if (info) {
			if (info->type.wp.accessType & WATCHPOINT_WRITE) {
				cliDebugger->backend->printf(cliDebugger->backend, "Hit watchpoint at 0x%08X: (new value = 0x%08x, old value = 0x%08X)\n", info->address, info->type.wp.newValue, info->type.wp.oldValue);
			} else {
				cliDebugger->backend->printf(cliDebugger->backend, "Hit watchpoint at 0x%08X: (value = 0x%08x)\n", info->address, info->type.wp.oldValue);
			}
		} else {
			cliDebugger->backend->printf(cliDebugger->backend, "Hit watchpoint\n");
		}
		break;
	case DEBUGGER_ENTER_ILLEGAL_OP:
		if (info) {
			cliDebugger->backend->printf(cliDebugger->backend, "Hit illegal opcode at 0x%08X: 0x%08X\n", info->address, info->type.bp.opcode);
		} else {
			cliDebugger->backend->printf(cliDebugger->backend, "Hit illegal opcode\n");
		}
		break;
	}
}

static void _cliDebuggerInit(struct mDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	cliDebugger->backend->init(cliDebugger->backend);
}

static void _cliDebuggerDeinit(struct mDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	if (cliDebugger->system) {
		if (cliDebugger->system->deinit) {
			cliDebugger->system->deinit(cliDebugger->system);
		}
		free(cliDebugger->system);
		cliDebugger->system = NULL;
	}
	if (cliDebugger->backend && cliDebugger->backend->deinit) {
		cliDebugger->backend->deinit(cliDebugger->backend);
		cliDebugger->backend = NULL;
	}
}

static void _cliDebuggerCustom(struct mDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	bool retain = false;
	if (cliDebugger->system) {
		retain = cliDebugger->system->custom(cliDebugger->system);
	}
	if (!retain && debugger->state == DEBUGGER_CUSTOM) {
		debugger->state = DEBUGGER_RUNNING;
	}
}

void CLIDebuggerCreate(struct CLIDebugger* debugger) {
	debugger->d.init = _cliDebuggerInit;
	debugger->d.deinit = _cliDebuggerDeinit;
	debugger->d.custom = _cliDebuggerCustom;
	debugger->d.paused = _commandLine;
	debugger->d.entered = _reportEntry;

	debugger->system = NULL;
	debugger->backend = NULL;
}

void CLIDebuggerAttachSystem(struct CLIDebugger* debugger, struct CLIDebuggerSystem* system) {
	if (debugger->system) {
		if (debugger->system->deinit) {
			debugger->system->deinit(debugger->system);
		}
		free(debugger->system);
	}

	debugger->system = system;
	system->p = debugger;
}

void CLIDebuggerAttachBackend(struct CLIDebugger* debugger, struct CLIDebuggerBackend* backend) {
	if (debugger->backend == backend) {
		return;
	}
	if (debugger->backend && debugger->backend->deinit) {
		debugger->backend->deinit(debugger->backend);
	}

	debugger->backend = backend;
	backend->p = debugger;
}

bool CLIDebuggerTabComplete(struct CLIDebugger* debugger, const char* token, bool initial, size_t tokenLen) {
	size_t cmd = 0;
	size_t len;
	const char* name = 0;
	for (len = 1; len <= tokenLen; ++len) {
		for (; (name = _debuggerCommands[cmd].name); ++cmd) {
			int cmp = strncasecmp(name, token, len);
			if (cmp > 0) {
				return false;
			}
			if (cmp == 0) {
				break;
			}
		}
	}
	if (!name) {
		return false;
	}
	if (_debuggerCommands[cmd + 1].name && strlen(_debuggerCommands[cmd + 1].name) >= len && name[len - 1] == _debuggerCommands[cmd + 1].name[len - 1]) {
		--len;
		const char* next = 0;
		int i;
		for (i = cmd + 1; _debuggerCommands[i].name; ++i) {
			if (strncasecmp(name, _debuggerCommands[i].name, len)) {
				break;
			}
			next = _debuggerCommands[i].name;
		}
		if (!next) {
			return false;
		}

		for (; name[len]; ++len) {
			if (name[len] != next[len]) {
				break;
			}
			char out[2] = { name[len], '\0' };
			debugger->backend->lineAppend(debugger->backend, out);
		}
		return true;
	}
	name += len - 1;
	debugger->backend->lineAppend(debugger->backend, name);
	debugger->backend->lineAppend(debugger->backend, " ");
	return true;
}
