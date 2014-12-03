/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include "util/common.h"

#include "gba-config.h"

enum DebuggerType {
	DEBUGGER_NONE = 0,
#ifdef USE_CLI_DEBUGGER
	DEBUGGER_CLI,
#endif
#ifdef USE_GDB_STUB
	DEBUGGER_GDB,
#endif
	DEBUGGER_MAX
};

struct GBAArguments {
	char* fname;
	char* patch;
	bool dirmode;

	enum DebuggerType debuggerType;
	bool debugAtStart;
};

struct SubParser {
	const char* usage;
	bool (*parse)(struct SubParser* parser, struct GBAConfig* config, int option, const char* arg);
	const char* extraOptions;
	void* opts;
};

struct GraphicsOpts {
	int multiplier;
};

struct GBAThread;

bool parseArguments(struct GBAArguments* opts, struct GBAConfig* config, int argc, char* const* argv, struct SubParser* subparser);
void freeArguments(struct GBAArguments* opts);

void usage(const char* arg0, const char* extraOptions);

void initParserForGraphics(struct SubParser* parser, struct GraphicsOpts* opts);
struct ARMDebugger* createDebugger(struct GBAArguments* opts, struct GBAThread* context);

#endif
