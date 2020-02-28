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
#include <mgba-util/vfs.h>

#if ENABLE_SCRIPTING
#include <mgba/core/scripting.h>
#endif

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
static void _listWatchpoints(struct CLIDebugger*, struct CLIDebugVector*);
static void _trace(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeByte(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeHalfword(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeRegister(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeWord(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpByte(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpHalfword(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpWord(struct CLIDebugger*, struct CLIDebugVector*);
#ifdef ENABLE_SCRIPTING
static void _source(struct CLIDebugger*, struct CLIDebugVector*);
#endif

static struct CLIDebuggerCommandSummary _debuggerCommands[] = {
	{ "break", _setBreakpoint, "Is", "Set a breakpoint" },
	{ "continue", _continue, "", "Continue execution" },
	{ "delete", _clearBreakpoint, "I", "Delete a breakpoint or watchpoint" },
	{ "disassemble", _disassemble, "Ii", "Disassemble instructions" },
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
	{ "status", _printStatus, "", "Print the current status" },
	{ "trace", _trace, "Is", "Trace a number of instructions" },
	{ "w/1", _writeByte, "II", "Write a byte at a specified offset" },
	{ "w/2", _writeHalfword, "II", "Write a halfword at a specified offset" },
	{ "w/r", _writeRegister, "SI", "Write a register" },
	{ "w/4", _writeWord, "II", "Write a word at a specified offset" },
	{ "watch", _setReadWriteWatchpoint, "Is", "Set a watchpoint" },
	{ "watch/c", _setWriteChangedWatchpoint, "Is", "Set a change watchpoint" },
	{ "watch/r", _setReadWatchpoint, "Is", "Set a read watchpoint" },
	{ "watch/w", _setWriteWatchpoint, "Is", "Set a write watchpoint" },
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
	{ "c", "continue" },
	{ "d", "delete" },
	{ "dis", "disassemble" },
	{ "disasm", "disassemble" },
	{ "h", "help" },
	{ "i", "status" },
	{ "info", "status" },
	{ "lb", "listb" },
	{ "lw", "listw" },
	{ "n", "next" },
	{ "p", "print" },
	{ "p/t", "print/t" },
	{ "p/x", "print/x" },
	{ "q", "quit" },
	{ "w", "watch" },
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

static void _continue(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.state = debugger->traceRemaining != 0 ? DEBUGGER_CALLBACK : DEBUGGER_RUNNING;
}

static void _next(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->d.core->step(debugger->d.core);
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
	const char** arglist = malloc(sizeof(const char*) * (args + 1));
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
		free(tree);
		return false;
	}
	parseFree(tree);
	free(tree);
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
			debugger->backend->printf(debugger->backend, "\n%s commands:\n", debugger->system->platformName);
			_printCommands(debugger, debugger->system->platformCommands, debugger->system->platformCommandAliases);
			debugger->backend->printf(debugger->backend, "\n%s commands:\n", debugger->system->name);
			_printCommands(debugger, debugger->system->commands, debugger->system->commandAliases);
		}
	} else {
		_printCommandSummary(debugger, dv->charValue, _debuggerCommands, _debuggerCommandAliases);
		if (debugger->system) {
			_printCommandSummary(debugger, dv->charValue, debugger->system->platformCommands, debugger->system->platformCommandAliases);
			_printCommandSummary(debugger, dv->charValue, debugger->system->commands, debugger->system->commandAliases);
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
	if (!debugger->d.platform->setRegister(debugger->d.platform, dv->charValue, dv->next->intValue)) {
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
		tree = malloc(sizeof(*tree));
		parseLexedExpression(tree, &lv);
	}
	lexFree(&lv);
	LexVectorClear(&lv);
	LexVectorDeinit(&lv);
	if (error) {
		if (tree) {
			parseFree(tree);
			free(tree);
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
		.address = dv->intValue,
		.segment = dv->segmentValue,
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
			debugger->backend->printf(debugger->backend, "%" PRIz "i: %02X:%X\n", watchpoint->id, watchpoint->segment, watchpoint->address);
		} else {
			debugger->backend->printf(debugger->backend, "%" PRIz "i: 0x%X\n", watchpoint->id, watchpoint->address);
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
	if (traceSize + 1 <= sizeof(trace)) {
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
	return debugger->traceRemaining != 0;
}

static void _printStatus(struct CLIDebugger* debugger, struct CLIDebugVector* dv) {
	UNUSED(dv);
	debugger->system->printStatus(debugger->system);
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

	struct ParseTree tree;
	parseLexedExpression(&tree, &lv);
	if (tree.token.type == TOKEN_ERROR_TYPE) {
		dvTemp.type = CLIDV_ERROR_TYPE;
	} else {
		if (!mDebuggerEvaluateParseTree(&debugger->d, &tree, &dvTemp.intValue, &dvTemp.segmentValue)) {
			dvTemp.type = CLIDV_ERROR_TYPE;
		}
	}

	parseFree(&tree);

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
	int result = _tryCommands(debugger, _debuggerCommands, _debuggerCommandAliases, line, cmdLength, args, count - cmdLength - 1);
	if (result < 0 && debugger->system) {
		result = _tryCommands(debugger, debugger->system->commands, debugger->system->commandAliases, line, cmdLength, args, count - cmdLength - 1);
		if (result < 0) {
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
	if (cliDebugger->traceRemaining > 0) {
		cliDebugger->traceRemaining = 0;
	}
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
	}
}

static void _cliDebuggerInit(struct mDebugger* debugger) {
	struct CLIDebugger* cliDebugger = (struct CLIDebugger*) debugger;
	cliDebugger->traceRemaining = 0;
	cliDebugger->traceVf = NULL;
	cliDebugger->backend->init(cliDebugger->backend);
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

void CLIDebuggerCreate(struct CLIDebugger* debugger) {
	debugger->d.init = _cliDebuggerInit;
	debugger->d.deinit = _cliDebuggerDeinit;
	debugger->d.custom = _cliDebuggerCustom;
	debugger->d.paused = _commandLine;
	debugger->d.entered = _reportEntry;
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
