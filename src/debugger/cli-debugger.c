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
static void _writeRegister(struct CLIDebugger*, struct CLIDebugVector*);
static void _writeWord(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpByte(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpHalfword(struct CLIDebugger*, struct CLIDebugVector*);
static void _dumpWord(struct CLIDebugger*, struct CLIDebugVector*);
#ifdef ENABLE_SCRIPTING
static void _source(struct CLIDebugger*, struct CLIDebugVector*);
#endif

static struct CLIDebuggerCommandSummary _debuggerCommands[] = {
	{ "b", _setBreakpoint, "Is", "Set a breakpoint" },
	{ "break", _setBreakpoint, "Is", "Set a breakpoint" },
	{ "c", _continue, "", "Continue execution" },
	{ "continue", _continue, "", "Continue execution" },
	{ "d", _clearBreakpoint, "I", "Delete a breakpoint" },
	{ "delete", _clearBreakpoint, "I", "Delete a breakpoint" },
	{ "dis", _disassemble, "Ii", "Disassemble instructions" },
	{ "disasm", _disassemble, "Ii", "Disassemble instructions" },
	{ "disassemble", _disassemble, "Ii", "Disassemble instructions" },
	{ "h", _printHelp, "S", "Print help" },
	{ "help", _printHelp, "S", "Print help" },
	{ "i", _printStatus, "", "Print the current status" },
	{ "info", _printStatus, "", "Print the current status" },
	{ "n", _next, "", "Execute next instruction" },
	{ "next", _next, "", "Execute next instruction" },
	{ "p", _print, "I", "Print a value" },
	{ "p/t", _printBin, "I", "Print a value as binary" },
	{ "p/x", _printHex, "I", "Print a value as hexadecimal" },
	{ "print", _print, "I", "Print a value" },
	{ "print/t", _printBin, "I", "Print a value as binary" },
	{ "print/x", _printHex, "I", "Print a value as hexadecimal" },
	{ "q", _quit, "", "Quit the emulator" },
	{ "quit", _quit, "", "Quit the emulator" },
	{ "reset", _reset, "", "Reset the emulation" },
	{ "r/1", _readByte, "I", "Read a byte from a specified offset" },
	{ "r/2", _readHalfword, "I", "Read a halfword from a specified offset" },
	{ "r/4", _readWord, "I", "Read a word from a specified offset" },
	{ "status", _printStatus, "", "Print the current status" },
	{ "trace", _trace, "I", "Trace a fixed number of instructions" },
	{ "w", _setWatchpoint, "Is", "Set a watchpoint" },
	{ "w/1", _writeByte, "II", "Write a byte at a specified offset" },
	{ "w/2", _writeHalfword, "II", "Write a halfword at a specified offset" },
	{ "w/r", _writeRegister, "SI", "Write a register" },
	{ "w/4", _writeWord, "II", "Write a word at a specified offset" },
	{ "watch", _setWatchpoint, "Is", "Set a watchpoint" },
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

static struct ParseTree* _parseTree(const char* string) {
	struct LexVector lv;
	bool error = false;
	LexVectorInit(&lv, 0);
	size_t length = strlen(string);
	size_t adjusted = lexExpression(&lv, string, length, NULL);
	struct ParseTree* tree = malloc(sizeof(*tree));
	if (!adjusted) {
		error = true;
	} else {
		parseLexedExpression(tree, &lv);

		if (adjusted > length) {
			error = true;
		} else {
			length -= adjusted;
			string += adjusted;
		}
	}
	lexFree(&lv);
	LexVectorClear(&lv);
	LexVectorDeinit(&lv);
	if (error) {
		parseFree(tree);
		free(tree);
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
	uint32_t address = dv->intValue;
	if (dv->next && dv->next->type == CLIDV_CHAR_TYPE) {
		struct ParseTree* tree = _parseTree(dv->next->charValue);
		if (tree) {
			debugger->d.platform->setConditionalBreakpoint(debugger->d.platform, address, dv->segmentValue, tree);
		} else {
			debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		}
	} else {
		debugger->d.platform->setBreakpoint(debugger->d.platform, address, dv->segmentValue);
	}
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
	if (dv->next && dv->next->type == CLIDV_CHAR_TYPE) {
		struct ParseTree* tree = _parseTree(dv->next->charValue);
		if (tree) {
			debugger->d.platform->setConditionalWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_RW, tree);
		} else {
			debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		}
	} else {
		debugger->d.platform->setWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_RW);
	}
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
	if (dv->next && dv->next->type == CLIDV_CHAR_TYPE) {
		struct ParseTree* tree = _parseTree(dv->next->charValue);
		if (tree) {
			debugger->d.platform->setConditionalWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_READ, tree);
		} else {
			debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		}
	} else {
		debugger->d.platform->setWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_READ);
	}
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
	if (dv->next && dv->next->type == CLIDV_CHAR_TYPE) {
		struct ParseTree* tree = _parseTree(dv->next->charValue);
		if (tree) {
			debugger->d.platform->setConditionalWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_WRITE, tree);
		} else {
			debugger->backend->printf(debugger->backend, "%s\n", ERROR_INVALID_ARGS);
		}
	} else {
		debugger->d.platform->setWatchpoint(debugger->d.platform, address, dv->segmentValue, WATCHPOINT_WRITE);
	}}

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

static int _tryCommands(struct CLIDebugger* debugger, struct CLIDebuggerCommandSummary* commands, const char* command, size_t commandLen, const char* args, size_t argsLen) {
	struct CLIDebugVector* dv = NULL;
	struct CLIDebugVector* dvLast = NULL;
	int i;
	const char* name;
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
						--args;
					} else {
						nextArgMandatory = isupper(commands[i].format[arg]) || (commands[i].format[arg] == '*');
						dvNext = _parseArg(debugger, args, adjusted, commands[i].format[arg]);
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
