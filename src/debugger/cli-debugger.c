/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/cli-debugger.h>

#include <mgba/internal/debugger/symbols.h>

#include <mgba/core/core.h>
#ifdef ENABLE_SCRIPTING
#include <mgba/core/scripting.h>
#endif
#include <mgba/core/timing.h>
#include <mgba/core/version.h>
#include <mgba/internal/debugger/parser.h>
#ifdef USE_ELF
#include <mgba-util/elf-read.h>
#endif
#include <mgba-util/string.h>
#include <mgba-util/vfs.h>

#if !defined(NDEBUG) && !defined(_WIN32)
#include <signal.h>
#endif

#ifdef USE_PTHREADS
#include <pthread.h>
#endif

const char* ERROR_MISSING_ARGS = "Arguments missing"; // TODO: share
const char* ERROR_OVERFLOW = "Arguments overflow";
const char* ERROR_INVALID_ARGS = "Invalid arguments";
const char* INFO_BREAKPOINT_ADDED = "Added breakpoint #%" PRIz "i\n";
const char* INFO_WATCHPOINT_ADDED = "Added watchpoint #%" PRIz "i\n";

static struct ParseTree* _parseTree(const char** string);
static bool _doTrace(struct CLIDebugger* debugger);

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
static void _listBreakpoints(struct CLIDebugger*, struct CLIDebugVector*);
static void _setReadWriteWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setReadWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setWriteWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setWriteChangedWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setReadWriteRangeWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setReadRangeWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setWriteRangeWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _setWriteChangedRangeWatchpoint(struct CLIDebugger*, struct CLIDebugVector*);
static void _listWatchpoints(struct CLIDebugger*, struct CLIDebugVector*);
static void _trace(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeByte(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeHalfword(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeRegister(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeWord(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpByte(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpHalfword(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpWord(struct CLIDebugger*, struct CLIDebugVector*);
static void _events(struct CLIDebugger*, struct CLIDebugVector*);
#ifdef ENABLE_SCRIPTING
static void _source(struct CLIDebugger*, struct CLIDebugVector*);
#endif
static void _backtrace(struct CLIDebugger*, struct CLIDebugVector*);
static void _finish(struct CLIDebugger*, struct CLIDebugVector*);
static void _setStackTraceMode(struct CLIDebugger*, struct CLIDebugVector*);
static void _loadSymbols(struct CLIDebugger*, struct CLIDebugVector*);
static void _setSymbol(struct CLIDebugger*, struct CLIDebugVector*);
static void _findSymbol(struct CLIDebugger*, struct CLIDebugVector*);

static struct CLIDebuggerCommandSummary _debuggerCommands[] = {
	{ "backtrace", _backtrace, "i", "Print backtrace of all or specified frames" },
	{ "break", _setBreakpoint, "Is", "Set a breakpoint" },
	{ "continue", _continue, "", "Continue execution" },
	{ "delete", _clearBreakpoint, "I", "Delete a breakpoint or watchpoint" },
	{ "disassemble", _disassemble, "Ii", "Disassemble instructions" },
	{ "events", _events, "", "Print list of scheduled events" },
	{ "finish", _finish, "", "Execute until current stack frame returns" },
	{ "help", _printHelp, "S", "Print help" },
	{ "listb", _listBreakpoints, "", "List breakpoints" },
	{ "listw", _listWatchpoints, "", "List watchpoints" },
	{ "next", _next, "", "Execute next instruction" },
	{ "print", _print, "S+", "Print a value" },
	{ "print/t", _printBin, "S+", "Print a value as binary" },
	{ "print/x", _printHex, "S+", "Print a value as hexadecimal" },
	{ "quit", _quit, "", "Quit the emulator" },
	{ "reset", _reset, "", "Reset the emulation" },
	{ "r/1", _readByte, "I", "Read a byte from a specified offset" },
	{ "r/2", _readHalfword, "I", "Read a halfword from a specified offset" },
	{ "r/4", _readWord, "I", "Read a word from a specified offset" },
	{ "set", _setSymbol, "SI", "Assign a symbol to an address" },
	{ "stack", _setStackTraceMode, "S", "Change the stack tracing mode" },
	{ "status", _printStatus, "", "Print the current status" },
	{ "symbol", _findSymbol, "I", "Find the symbol name for an address" },
	{ "load-symbols", _loadSymbols, "S", "Load symbols from an external file" },
	{ "trace", _trace, "Is", "Trace a number of instructions" },
	{ "w/1", _writeByte, "II", "Write a byte at a specified offset" },
	{ "w/2", _writeHalfword, "II", "Write a halfword at a specified offset" },
	{ "w/r", _writeRegister, "SI", "Write a register" },
	{ "w/4", _writeWord, "II", "Write a word at a specified offset" },
	{ "watch", _setReadWriteWatchpoint, "Is", "Set a watchpoint" },
	{ "watch/c", _setWriteChangedWatchpoint, "Is", "Set a change watchpoint" },
	{ "watch/r", _setReadWatchpoint, "Is", "Set a read watchpoint" },
	{ "watch/w", _setWriteWatchpoint, "Is", "Set a write watchpoint" },
	{ "watch-range", _setReadWriteRangeWatchpoint, "IIs", "Set a range watchpoint" },
	{ "watch-range/c", _setWriteChangedRangeWatchpoint, "IIs", "Set a change range watchpoint" },
	{ "watch-range/r", _setReadRangeWatchpoint, "IIs", "Set a read range watchpoint" },
	{ "watch-range/w", _setWriteRangeWatchpoint, "IIs", "Set a write range watchpoint" },
	{ "x/1", _dumpByte, "Ii", "Examine bytes at a specified offset" },
	{ "x/2", _dumpHalfword, "Ii", "Examine halfwords at a specified offset" },
	{ "x/4", _dumpWord, "Ii", "Examine words at a specified offset" },
#ifdef ENABLE_SCRIPTING
	{ "source", _source, "S", "Load a script" },
#endif
#if !defined(NDEBUG) && !defined(_WIN32)
	{ "!", _breakInto, "", "Break into attached debugger (for developers)" },
#endif
	{ 0, 0, 0, 0 }
};

static struct CLIDebuggerCommandAlias _debuggerCommandAliases[] = {
	{ "b", "break" },
	{ "bt", "backtrace" },
	{ "c", "continue" },
	{ "d", "delete" },
	{ "dis", "disassemble" },
	{ "disasm", "disassemble" },
	{ "fin", "finish" },
	{ "h", "help" },
	{ "i", "status" },
	{ "info", "status" },
	{ "loadsyms", "load-symbols" },
	{ "lb", "listb" },
	{ "lw", "listw" },
	{ "n", "next" },
	{ "p", "print" },
	{ "p/t", "print/t" },
	{ "p/x", "print/x" },
	{ "q", "quit" },
	{ "w", "watch" },
	{ "watchr", "watch-range" },
	{ "wr", "watch-range" },
	{ "watchr/c", "watch-range/c" },
	{ "wr/c", "watch-range/c" },
	{ "watchr/r", "watch-range/r" },
	{ "wr/r", "watch-range/r" },
	{ "watchr/w", "watch-range/w" },
	{ "wr/w", "watch-range/w" },
	{ ".", "source" },
	{ 0, 0 }
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

static bool CLIDebuggerCheckTraceMode(struct CLIDebugger* debugger, bool requireEnabled) {
	struct mDebuggerPlatform* platform = debugger->d.platform;
	if (!platform->getStackTraceMode) {
		debugger->backend->printf(debugger->backend, "Stack tracing is not supported by this platform.\n");
		return false;
	} else if (requireEnabled && platform->getStackTraceMode(platform) == STACK_TRACE_DISABLED) {
		debugger->backend->printf(debugger->backend, "Stack tracing is not enabled.\n");
		return false;
	}
	return true;
}

static void _continue(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = debugger->traceRemaining != 0 ? DEBUGGER_CALLBACK : DEBUGGER_RUNNING;
}

static void _next(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	struct mDebuggerPlatform* platform = debugger->d.platform;
	debugger->d.core->step(debugger->d.core);
	if (platform->getStackTraceMode && platform->getStackTraceMode(platform) != STACK_TRACE_DISABLED) {
		platform->updateStackTrace(platform);
	}
	_printStatus(debugger, 0);
}

static void _disassemble(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	debugger->system->disassemble(debugger->system, dv);
}

static bool _parseExpression(struct mDebugger* debugger, struct CLIDebugVector* dv, int32_t* intValue, int* segmentValue) {
	size_t args = 0;
	struct CLIDebugVector* accum;
	for (accum = dv; accum; accum = accum->next) {
		++args;
	}
	const char** arglist = calloc(args + 1, sizeof(const char*));
	args = 0;
	for (accum = dv; accum; accum = accum->next) {
		arglist[args] = accum->charValue;
		++args;
	}
	arglist[args] = NULL;
	struct ParseTree* tree = _parseTree(arglist);
	free(arglist);

	if (!tree) {
		return false;
	}
	if (!mDebuggerEvaluateParseTree(debugger, tree, intValue, segmentValue)) {
		parseFree(tree);
		return false;
	}
	parseFree(tree);
	return true;
}

static void _print(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	int32_t intValue = 0;
	int segmentValue = -1;
	if (!_parseExpression(&debugger->d, dv, &intValue, &segmentValue)) {
		debugger->backend->printf(debugger->backend, "Parse error\n");
		return;
	}
	if (segmentValue >= 0) {
		debugger->backend->printf(debugger->backend, " $%02X:%04X\n", segmentValue, intValue);
	} else {
		debugger->backend->printf(debugger->backend, " %u\n", intValue);
	}
}

static void _printBin(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	int32_t intValue = 0;
	int segmentValue = -1;
	if (!_parseExpression(&debugger->d, dv, &intValue, &segmentValue)) {
		debugger->backend->printf(debugger->backend, "Parse error\n");
		return;
	}
	debugger->backend->printf(debugger->backend, " 0b");
	int i = 32;
	while (i--) {
		debugger->backend->printf(debugger->backend, "%u", (intValue >> i) & 1);
	}
	debugger->backend->printf(debugger->backend, "\n");
}

static void _printHex(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	int32_t intValue = 0;
	int segmentValue = -1;
	if (!_parseExpression(&debugger->d, dv, &intValue, &segmentValue)) {
		debugger->backend->printf(debugger->backend, "Parse error\n");
		return;
	}
	debugger->backend->printf(debugger->backend, " 0x%08X\n", intValue);
}

static void _printCommands(struct CLIDebugger* debugger, struct CLIDebuggerCommandSummary* commands, struct CLIDebuggerCommandAlias* aliases) {
	int i;
	for (i = 0; commands[i].name; ++i) {
		debugger->backend->printf(debugger->backend, "%-15s  %s\n", commands[i].name, commands[i].summary);
		if (aliases) {
			bool printedAlias = false;
			int j;
			for (j = 0; aliases[j].name; ++j) {
				if (strcmp(aliases[j].original, commands[i].name) == 0) {
					if (!printedAlias) {
						debugger->backend->printf(debugger->backend, "                 Aliases:");
						printedAlias = true;
					}
					debugger->backend->printf(debugger->backend, " %s", aliases[j].name);
				}
			}
			if (printedAlias) {
				debugger->backend->printf(debugger->backend, "\n");
			}
		}
	}
}

static void _printCommandSummary(struct CLIDebugger* debugger, const char* name, struct CLIDebuggerCommandSummary* commands, struct CLIDebuggerCommandAlias* aliases) {
	int i;
	for (i = 0; commands[i].name; ++i) {
		if (strcmp(commands[i].name, name) == 0) {
			debugger->backend->printf(debugger->backend, " %s\n", commands[i].summary);
			if (aliases) {
				bool printedAlias = false;
				int j;
				for (j = 0; aliases[j].name; ++j) {
					if (strcmp(aliases[j].original, commands[i].name) == 0) {
						if (!printedAlias) {
							debugger->backend->printf(debugger->backend, " Aliases:");
							printedAlias = true;
						}
						debugger->backend->printf(debugger->backend, " %s", aliases[j].name);
					}
				}
				if (printedAlias) {
					debugger->backend->printf(debugger->backend, "\n");
				}
			}
			return;
		}
	}
}

static void _printHelp(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	if (!dv) {
		debugger->backend->printf(debugger->backend, "Generic commands:\n");
		_printCommands(debugger, _debuggerCommands, _debuggerCommandAliases);
		if (debugger->system) {
			if (debugger->system->platformCommands) {
				debugger->backend->printf(debugger->backend, "\n%s commands:\n", debugger->system->platformName);
				_printCommands(debugger, debugger->system->platformCommands, debugger->system->platformCommandAliases);
			}
			if (debugger->system->commands) {
				debugger->backend->printf(debugger->backend, "\n%s commands:\n", debugger->system->name);
				_printCommands(debugger, debugger->system->commands, debugger->system->commandAliases);
			}
		}
	} else {
		_printCommandSummary(debugger, dv->charValue, _debuggerCommands, _debuggerCommandAliases);
		if (debugger->system) {
			if (debugger->system->platformCommands) {
				_printCommandSummary(debugger, dv->charValue, debugger->system->platformCommands, debugger->system->platformCommandAliases);
			}
			if (debugger->system->commands) {
				_printCommandSummary(debugger, dv->charValue, debugger->system->commands, debugger->system->commandAliases);
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
	mStackTraceClear(&debugger->d.stackTrace);
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
	if (!dv || !dv->next) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (dv->type != CLIDV_INT_TYPE || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
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
	if (!dv || !dv->next) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (dv->type != CLIDV_INT_TYPE || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
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

static void _writeRegister(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || !dv->next) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (dv->type != CLIDV_CHAR_TYPE || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		return;
	}
	if (!debugger->d.core->writeRegister(debugger->d.core, dv->charValue, &dv->next->intValue)) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
	}
}

static void _writeWord(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || !dv->next) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (dv->type != CLIDV_INT_TYPE || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
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

#ifdef ENABLE_SCRIPTING
static void _source(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv) {
		debugger->backend->printf(debugger->backend, "Needs a filename\n");
		return;
	}
	if (debugger->d.bridge && mScriptBridgeLoadScript(debugger->d.bridge, dv->charValue)) {
		mScriptBridgeRun(debugger->d.bridge);
	} else {
		debugger->backend->printf(debugger->backend, "Failed to load script\n");
	}
}
#endif

static struct ParseTree* _parseTree(const char** string) {
	struct LexVector lv;
	bool error = false;
	LexVectorInit(&lv, 0);
	size_t i;
	for (i = 0; string[i]; ++i) {
		size_t length = strlen(string[i]);
		size_t adjusted = lexExpression(&lv, string[i], length, NULL);
		if (!adjusted || adjusted > length) {
			error = true;
		}
	}
	struct ParseTree* tree = NULL;
	if (!error) {
		tree = parseTreeCreate();
		parseLexedExpression(tree, &lv);
	}
	lexFree(&lv);
	LexVectorClear(&lv);
	LexVectorDeinit(&lv);
	if (error) {
		if (tree) {
			parseFree(tree);
		}
		return NULL;
	} else {
		return tree;
	}
}

static void _setBreakpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	struct mBreakpoint breakpoint = {
		.address = dv->intValue,
		.segment = dv->segmentValue,
		.type = BREAKPOINT_HARDWARE
	};
	if (dv->next && dv->next->type == CLIDV_CHAR_TYPE) {
		struct ParseTree* tree = _parseTree((const char*[]) { dv->next->charValue, NULL });
		if (tree) {
			breakpoint.condition = tree;
		} else {
			debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
			return;
		}
	}
	ssize_t id = debugger->d.platform->setBreakpoint(debugger->d.platform, &breakpoint);
	if (id > 0) {
		debugger->backend->printf(debugger->backend, INFO_BREAKPOINT_ADDED, id);
	}
}

static void _setWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv, enum mWatchpointType type) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!debugger->d.platform->setWatchpoint) {
		debugger->backend->printf(debugger->backend, "Watchpoints are not supported by this platform.\n");
		return;
	}
	struct mWatchpoint watchpoint = {
		.segment = dv->segmentValue,
		.minAddress = dv->intValue,
		.maxAddress = dv->intValue + 1,
		.type = type
	};
	if (dv->next && dv->next->type == CLIDV_CHAR_TYPE) {
		struct ParseTree* tree = _parseTree((const char*[]) { dv->next->charValue, NULL });
		if (tree) {
			watchpoint.condition = tree;
		} else {
			debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
			return;
		}
	}
	ssize_t id = debugger->d.platform->setWatchpoint(debugger->d.platform, &watchpoint);
	if (id > 0) {
		debugger->backend->printf(debugger->backend, INFO_WATCHPOINT_ADDED, id);
	}
}

static void _setRangeWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv, enum mWatchpointType type) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!dv->next || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (!debugger->d.platform->setWatchpoint) {
		debugger->backend->printf(debugger->backend, "Watchpoints are not supported by this platform.\n");
		return;
	}
	if (dv->intValue >= dv->next->intValue) {
		debugger->backend->printf(debugger->backend, "Range watchpoint end is before start. Note that the end of the range is not included.\n");
		return;
	}
	if (dv->segmentValue != dv->next->segmentValue) {
		debugger->backend->printf(debugger->backend, "Range watchpoint does not start and end in the same segment.\n");
		return;
	}
	struct mWatchpoint watchpoint = {
		.segment = dv->segmentValue,
		.minAddress = dv->intValue,
		.maxAddress = dv->next->intValue,
		.type = type
	};
	if (dv->next->next && dv->next->next->type == CLIDV_CHAR_TYPE) {
		struct ParseTree* tree = _parseTree((const char*[]) { dv->next->next->charValue, NULL });
		if (tree) {
			watchpoint.condition = tree;
		} else {
			debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
			return;
		}
	}
	ssize_t id = debugger->d.platform->setWatchpoint(debugger->d.platform, &watchpoint);
	if (id > 0) {
		debugger->backend->printf(debugger->backend, INFO_WATCHPOINT_ADDED, id);
	}
}

static void _setReadWriteWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_setWatchpoint(debugger, dv, WATCHPOINT_RW);
}

static void _setReadWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_setWatchpoint(debugger, dv, WATCHPOINT_READ);
}

static void _setWriteWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_setWatchpoint(debugger, dv, WATCHPOINT_WRITE);
}

static void _setWriteChangedWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_setWatchpoint(debugger, dv, WATCHPOINT_WRITE_CHANGE);
}

static void _setReadWriteRangeWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_setRangeWatchpoint(debugger, dv, WATCHPOINT_RW);
}

static void _setReadRangeWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_setRangeWatchpoint(debugger, dv, WATCHPOINT_READ);
}

static void _setWriteRangeWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_setRangeWatchpoint(debugger, dv, WATCHPOINT_WRITE);
}

static void _setWriteChangedRangeWatchpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	_setRangeWatchpoint(debugger, dv, WATCHPOINT_WRITE_CHANGE);
}

static void _clearBreakpoint(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	uint64_t id = dv->intValue;
	debugger->d.platform->clearBreakpoint(debugger->d.platform, id);
}

static void _listBreakpoints(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	struct mBreakpointList breakpoints;
	mBreakpointListInit(&breakpoints, 0);
	debugger->d.platform->listBreakpoints(debugger->d.platform, &breakpoints);
	size_t i;
	for (i = 0; i < mBreakpointListSize(&breakpoints); ++i) {
		struct mBreakpoint* breakpoint = mBreakpointListGetPointer(&breakpoints, i);
		if (breakpoint->segment >= 0) {
			debugger->backend->printf(debugger->backend, "%" PRIz "i: %02X:%X\n", breakpoint->id, breakpoint->segment, breakpoint->address);
		} else {
			debugger->backend->printf(debugger->backend, "%" PRIz "i: 0x%X\n", breakpoint->id, breakpoint->address);
		}
	}
	mBreakpointListDeinit(&breakpoints);
}

static void _listWatchpoints(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	struct mWatchpointList watchpoints;
	mWatchpointListInit(&watchpoints, 0);
	debugger->d.platform->listWatchpoints(debugger->d.platform, &watchpoints);
	size_t i;
	for (i = 0; i < mWatchpointListSize(&watchpoints); ++i) {
		struct mWatchpoint* watchpoint = mWatchpointListGetPointer(&watchpoints, i);
		if (watchpoint->segment >= 0) {
			if (watchpoint->maxAddress == watchpoint->minAddress + 1) {
				debugger->backend->printf(debugger->backend, "%" PRIz "i: %02X:%X\n", watchpoint->id, watchpoint->segment, watchpoint->minAddress);
			} else {
				debugger->backend->printf(debugger->backend, "%" PRIz "i: %02X:%X-%X\n", watchpoint->id, watchpoint->segment, watchpoint->minAddress, watchpoint->maxAddress);
			}
		} else {
			if (watchpoint->maxAddress == watchpoint->minAddress + 1) {
				debugger->backend->printf(debugger->backend, "%" PRIz "i: 0x%X\n", watchpoint->id, watchpoint->minAddress);
			} else {
				debugger->backend->printf(debugger->backend, "%" PRIz "i: 0x%X-0x%X\n", watchpoint->id, watchpoint->minAddress, watchpoint->maxAddress);
			}
		}
	}
	mWatchpointListDeinit(&watchpoints);
}

static void _trace(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!dv || dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}

	debugger->traceRemaining = dv->intValue;
	if (debugger->traceVf) {
		debugger->traceVf->close(debugger->traceVf);
		debugger->traceVf = NULL;
	}
	if (debugger->traceRemaining == 0) {
		return;
	}
	if (dv->next && dv->next->charValue) {
		debugger->traceVf = VFileOpen(dv->next->charValue, O_CREAT | O_WRONLY | O_APPEND);
	}
	if (_doTrace(debugger)) {
		debugger->d.state = DEBUGGER_CALLBACK;
	} else {
		debugger->system->printStatus(debugger->system);
	}
}

static bool _doTrace(struct CLIDebugger* debugger) {
	char trace[1024];
	trace[sizeof(trace) - 1] = '\0';
	size_t traceSize = sizeof(trace) - 2;
	debugger->d.platform->trace(debugger->d.platform, trace, &traceSize);
	if (traceSize + 2 <= sizeof(trace)) {
		trace[traceSize] = '\n';
		trace[traceSize + 1] = '\0';
	}
	if (debugger->traceVf) {
		debugger->traceVf->write(debugger->traceVf, trace, traceSize + 1);
	} else {
		debugger->backend->printf(debugger->backend, "%s", trace);
	}
	if (debugger->traceRemaining > 0) {
		--debugger->traceRemaining;
	}
	if (!debugger->traceRemaining) {
		if (debugger->traceVf) {
			debugger->traceVf->close(debugger->traceVf);
			debugger->traceVf = NULL;
		}
		return false;
	}
	return true;
}

static void _printStatus(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->system->printStatus(debugger->system);
}

static void _events(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	struct mTiming* timing = debugger->d.core->timing;
	struct mTimingEvent* next = timing->root;
	for (; next; next = next->next) {
		debugger->backend->printf(debugger->backend, "%s in %i cycles\n", next->name, mTimingUntil(timing, next));
	}
}

struct CLIDebugVector* CLIDVParse(struct CLIDebugger* debugger, const char* string, size_t length) {
	if (!string || length < 1) {
		return 0;
	}

	struct CLIDebugVector dvTemp = { .type = CLIDV_INT_TYPE, .segmentValue = -1 };

	struct LexVector lv;
	LexVectorInit(&lv, 0);
	size_t adjusted = lexExpression(&lv, string, length, " ");
	if (adjusted > length) {
		dvTemp.type = CLIDV_ERROR_TYPE;
	}

	struct ParseTree* tree = parseTreeCreate();
	if (!parseLexedExpression(tree, &lv)) {
		dvTemp.type = CLIDV_ERROR_TYPE;
	} else {
		if (!mDebuggerEvaluateParseTree(&debugger->d, tree, &dvTemp.intValue, &dvTemp.segmentValue)) {
			dvTemp.type = CLIDV_ERROR_TYPE;
		}
	}

	parseFree(tree);

	lexFree(&lv);
	LexVectorDeinit(&lv);

	struct CLIDebugVector* dv = malloc(sizeof(struct CLIDebugVector));
	if (dvTemp.type == CLIDV_ERROR_TYPE) {
		dv->type = CLIDV_ERROR_TYPE;
		dv->next = 0;
	} else {
		*dv = dvTemp;
	}
	return dv;
}

struct CLIDebugVector* CLIDVStringParse(struct CLIDebugger* debugger, const char* string, size_t length) {
	UNUSED(debugger);
	if (!string || length < 1) {
		return 0;
	}

	struct CLIDebugVector dvTemp = { .type = CLIDV_CHAR_TYPE };

	dvTemp.charValue = strndup(string, length);

	struct CLIDebugVector* dv = malloc(sizeof(struct CLIDebugVector));
	*dv = dvTemp;
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

static struct CLIDebugVector* _parseArg(struct CLIDebugger* debugger, const char* args, size_t argsLen, char type) {
	struct CLIDebugVector* dv = NULL;
	switch (type) {
	case 'I':
	case 'i':
		return CLIDVParse(debugger, args, argsLen);
	case 'S':
	case 's':
		return CLIDVStringParse(debugger, args, argsLen);
	case '*':
		dv = _parseArg(debugger, args, argsLen, 'I');
		if (!dv) {
			dv = _parseArg(debugger, args, argsLen, 'S');
		}
		break;
	}
	return dv;
}

static int _tryCommands(struct CLIDebugger* debugger, struct CLIDebuggerCommandSummary* commands, struct CLIDebuggerCommandAlias* aliases, const char* command, size_t commandLen, const char* args, size_t argsLen) {
	struct CLIDebugVector* dv = NULL;
	struct CLIDebugVector* dvLast = NULL;
	int i;
	const char* name;
	if (aliases) {
		for (i = 0; (name = aliases[i].name); ++i) {
			if (strlen(name) != commandLen) {
				continue;
			}
			if (strncasecmp(name, command, commandLen) == 0) {
				command = aliases[i].original;
				commandLen = strlen(aliases[i].original);
			}
		}
	}
	for (i = 0; (name = commands[i].name); ++i) {
		if (strlen(name) != commandLen) {
			continue;
		}
		if (strncasecmp(name, command, commandLen) == 0) {
			if (commands[i].format && args) {
				char lastArg = '\0';
				int arg;
				for (arg = 0; commands[i].format[arg] && argsLen; ++arg) {
					while (isspace(args[0]) && argsLen) {
						++args;
						--argsLen;
					}
					if (!args[0] || !argsLen) {
						debugger->backend->printf(debugger->backend, "Wrong number of arguments\n");
						_DVFree(dv);
						return 0;
					}

					size_t adjusted;
					const char* next = strchr(args, ' ');
					if (next) {
						adjusted = next - args;
					} else {
						adjusted = argsLen;
					}

					struct CLIDebugVector* dvNext = NULL;
					bool nextArgMandatory = false;

					if (commands[i].format[arg] == '+') {
						dvNext = _parseArg(debugger, args, adjusted, lastArg);
						--arg;
					} else {
						nextArgMandatory = isupper(commands[i].format[arg]) || (commands[i].format[arg] == '*');
						dvNext = _parseArg(debugger, args, adjusted, commands[i].format[arg]);
						lastArg = commands[i].format[arg];
					}

					args += adjusted;
					argsLen -= adjusted;

					if (!dvNext) {
						if (!nextArgMandatory) {
							args = NULL;
						}
						break;
					}
					if (dvNext->type == CLIDV_ERROR_TYPE) {
						debugger->backend->printf(debugger->backend, "Parse error\n");
						_DVFree(dv);
						_DVFree(dvNext);
						return 0;
					}

					if (dvLast) {
						dvLast->next = dvNext;
						dvLast = dvNext;
					} else {
						dv = dvNext;
						dvLast = dv;
					}
				}
			}

			if (args) {
				while (isspace(args[0]) && argsLen) {
					++args;
					--argsLen;
				}
			}
			if (args && argsLen) {
				debugger->backend->printf(debugger->backend, "Wrong number of arguments\n");
				_DVFree(dv);
				return 0;
			}
			commands[i].command(debugger, dv);
			_DVFree(dv);
			return 1;
		}
	}
	return -1;
}

bool CLIDebuggerRunCommand(struct CLIDebugger* debugger, const char* line, size_t count) {
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
	int result = _tryCommands(debugger, _debuggerCommands, _debuggerCommandAliases, line, cmdLength, args, count - cmdLength - 1);
	if (result < 0 && debugger->system) {
		if (debugger->system->commands) {
			result = _tryCommands(debugger, debugger->system->commands, debugger->system->commandAliases, line, cmdLength, args, count - cmdLength - 1);
		}
		if (result < 0 && debugger->system->platformCommands) {
			result = _tryCommands(debugger, debugger->system->platformCommands, debugger->system->platformCommandAliases, line, cmdLength, args, count - cmdLength - 1);
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
	if (cliDebugger->skipStatus) {
		cliDebugger->skipStatus = false;
	} else {
		_printStatus(cliDebugger, 0);
	}
	while (debugger->state == DEBUGGER_PAUSED) {
		line = cliDebugger->backend->readline(cliDebugger->backend, &len);
		if (!line || len == 0) {
			debugger->state = DEBUGGER_SHUTDOWN;
			return;
		}
		if (line[0] == '\033') {
			cliDebugger->skipStatus = true;
			return;
		}
		if (line[0] == '\n') {
			line = cliDebugger->backend->historyLast(cliDebugger->backend, &len);
			if (line && len) {
				CLIDebuggerRunCommand(cliDebugger, line, len);
			}
		} else {
			if (line[0] == '#') {
				cliDebugger->skipStatus = true;
			} else {
				CLIDebuggerRunCommand(cliDebugger, line, len);
			}
			cliDebugger->backend->historyAppend(cliDebugger->backend, line);
		}
	}
}

static void _reportEntry(struct mDebugger* debugger, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	if (cliDebugger->traceRemaining > 0) {
		cliDebugger->traceRemaining = 0;
	}
	cliDebugger->skipStatus = false;
	switch (reason) {
	case DEBUGGER_ENTER_MANUAL:
	case DEBUGGER_ENTER_ATTACHED:
		break;
	case DEBUGGER_ENTER_BREAKPOINT:
		if (info) {
			if (info->pointId > 0) {
				cliDebugger->backend->printf(cliDebugger->backend, "Hit breakpoint %" PRIz "i at 0x%08X\n", info->pointId, info->address);
			} else {
				cliDebugger->backend->printf(cliDebugger->backend, "Hit unknown breakpoint at 0x%08X\n", info->address);
			}
		} else {
			cliDebugger->backend->printf(cliDebugger->backend, "Hit breakpoint\n");
		}
		break;
	case DEBUGGER_ENTER_WATCHPOINT:
		if (info) {
			if (info->type.wp.accessType & WATCHPOINT_WRITE) {
				cliDebugger->backend->printf(cliDebugger->backend, "Hit watchpoint %" PRIz "i at 0x%08X: (new value = 0x%08X, old value = 0x%08X)\n", info->pointId, info->address, info->type.wp.newValue, info->type.wp.oldValue);
			} else {
				cliDebugger->backend->printf(cliDebugger->backend, "Hit watchpoint %" PRIz "i at 0x%08X: (value = 0x%08X)\n", info->pointId, info->address, info->type.wp.oldValue);
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
	case DEBUGGER_ENTER_STACK:
		if (info) {
			if (info->type.st.traceType == STACK_TRACE_BREAK_ON_CALL) {
				struct mStackTrace* stack = &cliDebugger->d.stackTrace;
				struct mStackFrame* frame = mStackTraceGetFrame(stack, 0);
				if (frame->interrupt) {
					cliDebugger->backend->printf(cliDebugger->backend, "Hit interrupt at at 0x%08X\n", info->address);
				} else {
					cliDebugger->backend->printf(cliDebugger->backend, "Hit function call at at 0x%08X\n", info->address);
				}
			} else {
				cliDebugger->backend->printf(cliDebugger->backend, "Hit function return at at 0x%08X\n", info->address);
			}
		} else {
			cliDebugger->backend->printf(cliDebugger->backend, "Hit function call or return\n");
		}
		_backtrace(cliDebugger, NULL);
		break;
	}
}

static void _cliDebuggerInit(struct mDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	cliDebugger->traceRemaining = 0;
	cliDebugger->traceVf = NULL;
	cliDebugger->skipStatus = false;
	cliDebugger->backend->init(cliDebugger->backend);
	if (cliDebugger->system && cliDebugger->system->init) {
		cliDebugger->system->init(cliDebugger->system);
	}
}

static void _cliDebuggerDeinit(struct mDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	if (cliDebugger->traceVf) {
		cliDebugger->traceVf->close(cliDebugger->traceVf);
		cliDebugger->traceVf = NULL;
	}

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
	bool retain = true;
	enum mDebuggerState next = DEBUGGER_RUNNING;
	if (cliDebugger->traceRemaining) {
		retain = _doTrace(cliDebugger) && retain;
		next = DEBUGGER_PAUSED;
	}
	if (cliDebugger->system) {
		retain = cliDebugger->system->custom(cliDebugger->system) && retain;
	}
	if (!retain && debugger->state == DEBUGGER_CALLBACK) {
		debugger->state = next;
	}
}

static void _cliDebuggerInterrupt(struct mDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	if (cliDebugger->backend->interrupt) {
		cliDebugger->backend->interrupt(cliDebugger->backend);
	}
}

void CLIDebuggerCreate(struct CLIDebugger* debugger) {
	debugger->d.init = _cliDebuggerInit;
	debugger->d.deinit = _cliDebuggerDeinit;
	debugger->d.custom = _cliDebuggerCustom;
	debugger->d.paused = _commandLine;
	debugger->d.update = NULL;
	debugger->d.entered = _reportEntry;
	debugger->d.interrupt = _cliDebuggerInterrupt;
	debugger->d.type = DEBUGGER_CLI;

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

static void _backtrace(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!CLIDebuggerCheckTraceMode(debugger, true)) {
		return;
	}
	struct mStackTrace* stack = &debugger->d.stackTrace;
	ssize_t frames = mStackTraceGetDepth(stack);
	if (dv && dv->type == CLIDV_INT_TYPE && dv->intValue < frames) {
		frames = dv->intValue;
	}
	ssize_t i;
	struct mDebuggerSymbols* symbolTable = debugger->d.core->symbolTable;
	for (i = 0; i < frames; ++i) {
		char trace[1024];
		size_t traceSize = sizeof(trace) - 2;
		mStackTraceFormatFrame(stack, symbolTable, i, trace, &traceSize);
		debugger->backend->printf(debugger->backend, "%s", trace);
	}
}

static void _finish(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	if (!CLIDebuggerCheckTraceMode(debugger, true)) {
		return;
	}
	struct mStackTrace* stack = &debugger->d.stackTrace;
	struct mStackFrame* frame = mStackTraceGetFrame(stack, 0);
	if (!frame) {
		debugger->backend->printf(debugger->backend, "No current stack frame.\n");
		return;
	}
	frame->breakWhenFinished = true;
	_continue(debugger, dv);
}

static void _setStackTraceMode(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	if (!CLIDebuggerCheckTraceMode(debugger, false)) {
		return;
	}
	if (!dv) {
		debugger->backend->printf(debugger->backend, "off           disable stack tracing (default)\n");
		debugger->backend->printf(debugger->backend, "trace-only    enable stack tracing\n");
		debugger->backend->printf(debugger->backend, "break-call    break on function calls\n");
		debugger->backend->printf(debugger->backend, "break-return  break on function returns\n");
		debugger->backend->printf(debugger->backend, "break-all     break on function calls and returns\n");
		return;
	}
	if (dv->type != CLIDV_CHAR_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		return;
	}
	struct mDebuggerPlatform* platform = debugger->d.platform;
	if (strcmp(dv->charValue, "off") == 0) {
		platform->setStackTraceMode(platform, STACK_TRACE_DISABLED);
	} else if (strcmp(dv->charValue, "trace-only") == 0) {
		platform->setStackTraceMode(platform, STACK_TRACE_ENABLED);
	} else if (strcmp(dv->charValue, "break-call") == 0) {
		platform->setStackTraceMode(platform, STACK_TRACE_BREAK_ON_CALL);
	} else if (strcmp(dv->charValue, "break-return") == 0) {
		platform->setStackTraceMode(platform, STACK_TRACE_BREAK_ON_RETURN);
	} else if (strcmp(dv->charValue, "break-all") == 0) {
		platform->setStackTraceMode(platform, STACK_TRACE_BREAK_ON_BOTH);
	} else {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
	}
}

static void _loadSymbols(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct mDebuggerSymbols* symbolTable = debugger->d.core->symbolTable;
	if (!symbolTable) {
		debugger->backend->printf(debugger->backend, "No symbol table available.\n");
		return;
	}
	if (!dv || dv->next) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (dv->type != CLIDV_CHAR_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		return;
	}
	struct VFile* vf = VFileOpen(dv->charValue, O_RDONLY);
	if (!vf) {
		debugger->backend->printf(debugger->backend, "%s\n", "Could not open symbol file");
		return;
	}
#ifdef USE_ELF
	struct ELF* elf = ELFOpen(vf);
	if (elf) {
#ifdef USE_DEBUGGERS
		mCoreLoadELFSymbols(symbolTable, elf);
#endif
		ELFClose(elf);
	} else
#endif
	{
		mDebuggerLoadARMIPSSymbols(symbolTable, vf);
	}
	vf->close(vf);
}

static void _setSymbol(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct mDebuggerSymbols* symbolTable = debugger->d.core->symbolTable;
	if (!symbolTable) {
		debugger->backend->printf(debugger->backend, "No symbol table available.\n");
		return;
	}
	if (!dv || !dv->next) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (dv->type != CLIDV_CHAR_TYPE || dv->next->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		return;
	}
	mDebuggerSymbolAdd(symbolTable, dv->charValue, dv->next->intValue, dv->next->segmentValue);
}

static void _findSymbol(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	struct mDebuggerSymbols* symbolTable = debugger->d.core->symbolTable;
	if (!symbolTable) {
		debugger->backend->printf(debugger->backend, "No symbol table available.\n");
		return;
	}
	if (!dv) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_MISSING_ARGS);
		return;
	}
	if (dv->type != CLIDV_INT_TYPE) {
		debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		return;
	}
	const char* name = mDebuggerSymbolReverseLookup(symbolTable, dv->intValue, dv->segmentValue);
	if (name) {
		if (dv->segmentValue >= 0) {
			debugger->backend->printf(debugger->backend, " 0x%02X:%08X = %s\n", dv->segmentValue, dv->intValue, name);
		} else {
			debugger->backend->printf(debugger->backend, " 0x%08X = %s\n", dv->intValue, name);
		}
	} else {
		debugger->backend->printf(debugger->backend, "Not found.\n");
	}
}
