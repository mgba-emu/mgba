/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/table.h>

#include <mgba/debugger/debugger.h>

struct mArguments {
	char* fname;
	char* patch;
	char* cheatsFile;
	char* savestate;
	char* bios;
	int logLevel;
	int frameskip;

	struct Table configOverrides;

	enum mDebuggerType debuggerType;
	bool debugAtStart;
	bool showHelp;
	bool showVersion;
};

struct mOption {
	const char* name;
	bool arg;
	char shortEquiv;
};

struct mCoreConfig;
struct mSubParser {
	const char* usage;
	bool (*parse)(struct mSubParser* parser, int option, const char* arg);
	bool (*parseLong)(struct mSubParser* parser, const char* option, const char* arg);
	void (*apply)(struct mSubParser* parser, struct mCoreConfig* config);
	const char* extraOptions;
	const struct mOption* longOptions;
	void* opts;
};

struct mGraphicsOpts {
	int multiplier;
	bool fullscreen;
};

void usage(const char* arg0, const char* prologue, const char* epilogue, const struct mSubParser* subparsers, int nSubparsers);
void version(const char* arg0);

bool mArgumentsParse(struct mArguments* args, int argc, char* const* argv, struct mSubParser* subparsers, int nSubparsers);
void mArgumentsApply(const struct mArguments* args, struct mSubParser* subparsers, int nSubparsers, struct mCoreConfig* config);
void mArgumentsDeinit(struct mArguments* args);

void mSubParserGraphicsInit(struct mSubParser* parser, struct mGraphicsOpts* opts);

CXX_GUARD_END

#endif
