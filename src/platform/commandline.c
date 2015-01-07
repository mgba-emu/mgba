/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "commandline.h"

#include "debugger/debugger.h"

#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"
#include "gba/gba-cli.h"
#endif

#ifdef USE_GDB_STUB
#include "debugger/gdb-stub.h"
#endif

#include "gba/gba-video.h"

#include <fcntl.h>
#include <getopt.h>

#define GRAPHICS_OPTIONS "1234f"
#define GRAPHICS_USAGE \
	"\nGraphics options:\n" \
	"  -1               1x viewport\n" \
	"  -2               2x viewport\n" \
	"  -3               3x viewport\n" \
	"  -4               4x viewport\n" \
	"  -f               Start full-screen"

static const struct option _options[] = {
	{ "bios",      required_argument, 0, 'b' },
	{ "dirmode",      required_argument, 0, 'D' },
	{ "frameskip", required_argument, 0, 's' },
#ifdef USE_CLI_DEBUGGER
	{ "debug",     no_argument, 0, 'd' },
#endif
#ifdef USE_GDB_STUB
	{ "gdb",       no_argument, 0, 'g' },
#endif
	{ "patch",     required_argument, 0, 'p' },
	{ 0, 0, 0, 0 }
};

bool _parseGraphicsArg(struct SubParser* parser, struct GBAConfig* config, int option, const char* arg);

bool parseArguments(struct GBAArguments* opts, struct GBAConfig* config, int argc, char* const* argv, struct SubParser* subparser) {
	int ch;
	char options[64] =
		"b:Dl:p:s:"
#ifdef USE_CLI_DEBUGGER
		"d"
#endif
#ifdef USE_GDB_STUB
		"g"
#endif
	;
	if (subparser && subparser->extraOptions) {
		// TODO: modularize options to subparsers
		strncat(options, subparser->extraOptions, sizeof(options) - strlen(options) - 1);
	}
	while ((ch = getopt_long(argc, argv, options, _options, 0)) != -1) {
		switch (ch) {
		case 'b':
			GBAConfigSetDefaultValue(config, "bios", optarg);
			break;
		case 'D':
			opts->dirmode = true;
			break;
#ifdef USE_CLI_DEBUGGER
		case 'd':
			if (opts->debuggerType != DEBUGGER_NONE) {
				return false;
			}
			opts->debuggerType = DEBUGGER_CLI;
			break;
#endif
#ifdef USE_GDB_STUB
		case 'g':
			if (opts->debuggerType != DEBUGGER_NONE) {
				return false;
			}
			opts->debuggerType = DEBUGGER_GDB;
			break;
#endif
		case 'l':
			GBAConfigSetDefaultValue(config, "logLevel", optarg);
			break;
		case 'p':
			opts->patch = strdup(optarg);
			break;
		case 's':
			GBAConfigSetDefaultValue(config, "frameskip", optarg);
			break;
		default:
			if (subparser) {
				if (!subparser->parse(subparser, config, ch, optarg)) {
					return false;
				}
			}
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1) {
		return false;
	}
	opts->fname = strdup(argv[0]);
	return true;
}

void freeArguments(struct GBAArguments* opts) {
	free(opts->fname);
	opts->fname = 0;

	free(opts->patch);
	opts->patch = 0;
}

void initParserForGraphics(struct SubParser* parser, struct GraphicsOpts* opts) {
	parser->usage = GRAPHICS_USAGE;
	parser->opts = opts;
	parser->parse = _parseGraphicsArg;
	parser->extraOptions = GRAPHICS_OPTIONS;
	opts->multiplier = 0;
}

bool _parseGraphicsArg(struct SubParser* parser, struct GBAConfig* config, int option, const char* arg) {
	UNUSED(arg);
	struct GraphicsOpts* graphicsOpts = parser->opts;
	switch (option) {
	case 'f':
		GBAConfigSetDefaultIntValue(config, "fullscreen", 1);
		return true;
	case '1':
	case '2':
	case '3':
	case '4':
		if (graphicsOpts->multiplier) {
			return false;
		}
		graphicsOpts->multiplier = option - '0';
		GBAConfigSetDefaultIntValue(config, "width", VIDEO_HORIZONTAL_PIXELS * graphicsOpts->multiplier);
		GBAConfigSetDefaultIntValue(config, "height", VIDEO_VERTICAL_PIXELS * graphicsOpts->multiplier);
		return true;
	default:
		return false;
	}
}

struct ARMDebugger* createDebugger(struct GBAArguments* opts, struct GBAThread* context) {
#ifndef USE_CLI_DEBUGGER
	UNUSED(context);
#endif
	union DebugUnion {
		struct ARMDebugger d;
#ifdef USE_CLI_DEBUGGER
		struct CLIDebugger cli;
#endif
#ifdef USE_GDB_STUB
		struct GDBStub gdb;
#endif
	};

	union DebugUnion* debugger = malloc(sizeof(union DebugUnion));

	switch (opts->debuggerType) {
#ifdef USE_CLI_DEBUGGER
	case DEBUGGER_CLI:
		CLIDebuggerCreate(&debugger->cli);
		struct GBACLIDebugger* gbaDebugger = GBACLIDebuggerCreate(context);
		CLIDebuggerAttachSystem(&debugger->cli, &gbaDebugger->d);
		break;
#endif
#ifdef USE_GDB_STUB
	case DEBUGGER_GDB:
		GDBStubCreate(&debugger->gdb);
		GDBStubListen(&debugger->gdb, 2345, 0);
		break;
#endif
	case DEBUGGER_NONE:
	case DEBUGGER_MAX:
		free(debugger);
		return 0;
		break;
	}

	return &debugger->d;
}

void usage(const char* arg0, const char* extraOptions) {
	printf("usage: %s [option ...] file\n", arg0);
	puts("\nGeneric options:");
	puts("  -b, --bios FILE     GBA BIOS file to use");
#ifdef USE_CLI_DEBUGGER
	puts("  -d, --debug         Use command-line debugger");
#endif
#ifdef USE_GDB_STUB
	puts("  -g, --gdb           Start GDB session (default port 2345)");
#endif
	puts("  -p, --patch FILE    Apply a specified patch file when running");
	puts("  -s, --frameskip N   Skip every N frames");
	if (extraOptions) {
		puts(extraOptions);
	}
}
