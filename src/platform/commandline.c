/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "commandline.h"

#include "debugger/debugger.h"

#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"
#include "gba/supervisor/cli.h"
#endif

#ifdef USE_GDB_STUB
#include "debugger/gdb-stub.h"
#endif

#include "gba/video.h"
#include "util/string.h"

#include <fcntl.h>
#include <getopt.h>

#define GRAPHICS_OPTIONS "123456f"
#define GRAPHICS_USAGE \
	"\nGraphics options:\n" \
	"  -1               1x viewport\n" \
	"  -2               2x viewport\n" \
	"  -3               3x viewport\n" \
	"  -4               4x viewport\n" \
	"  -5               5x viewport\n" \
	"  -6               6x viewport\n" \
	"  -f               Start full-screen"

static const struct option _options[] = {
	{ "bios",      required_argument, 0, 'b' },
	{ "cheats",    required_argument, 0, 'c' },
	{ "dirmode",   required_argument, 0, 'D' },
	{ "frameskip", required_argument, 0, 's' },
#ifdef USE_CLI_DEBUGGER
	{ "debug",     no_argument, 0, 'd' },
#endif
#ifdef USE_GDB_STUB
	{ "gdb",       no_argument, 0, 'g' },
#endif
	{ "help",      no_argument, 0, 'h' },
	{ "movie",     required_argument, 0, 'v' },
	{ "patch",     required_argument, 0, 'p' },
	{ "version",   no_argument, 0, '\0' },
	{ 0, 0, 0, 0 }
};

static bool _parseGraphicsArg(struct SubParser* parser, struct GBAConfig* config, int option, const char* arg);

bool parseArguments(struct GBAArguments* opts, struct GBAConfig* config, int argc, char* const* argv, struct SubParser* subparser) {
	int ch;
	char options[64] =
		"b:c:Dhl:p:s:v:"
#ifdef USE_CLI_DEBUGGER
		"d"
#endif
#ifdef USE_GDB_STUB
		"g"
#endif
	;
	memset(opts, 0, sizeof(*opts));
	if (subparser && subparser->extraOptions) {
		// TODO: modularize options to subparsers
		strncat(options, subparser->extraOptions, sizeof(options) - strlen(options) - 1);
	}
	int index = 0;
	while ((ch = getopt_long(argc, argv, options, _options, &index)) != -1) {
		const struct option* opt = &_options[index];
		switch (ch) {
		case '\0':
			if (strcmp(opt->name, "version") == 0) {
				opts->showVersion = true;
			} else {
				return false;
			}
			break;
		case 'b':
			GBAConfigSetOverrideValue(config, "bios", optarg);
			break;
		case 'c':
			opts->cheatsFile = strdup(optarg);
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
		case 'h':
			opts->showHelp = true;
			break;
		case 'l':
			GBAConfigSetOverrideValue(config, "logLevel", optarg);
			break;
		case 'p':
			opts->patch = strdup(optarg);
			break;
		case 's':
			GBAConfigSetOverrideValue(config, "frameskip", optarg);
			break;
		case 'v':
			opts->movie = strdup(optarg);
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
		return opts->showHelp || opts->showVersion;
	}
	opts->fname = strdup(argv[0]);
	return true;
}

void freeArguments(struct GBAArguments* opts) {
	free(opts->fname);
	opts->fname = 0;

	free(opts->patch);
	opts->patch = 0;

	free(opts->movie);
	opts->movie = 0;
}

void initParserForGraphics(struct SubParser* parser, struct GraphicsOpts* opts) {
	parser->usage = GRAPHICS_USAGE;
	parser->opts = opts;
	parser->parse = _parseGraphicsArg;
	parser->extraOptions = GRAPHICS_OPTIONS;
	opts->multiplier = 0;
	opts->fullscreen = false;
}

bool _parseGraphicsArg(struct SubParser* parser, struct GBAConfig* config, int option, const char* arg) {
	UNUSED(arg);
	struct GraphicsOpts* graphicsOpts = parser->opts;
	switch (option) {
	case 'f':
		graphicsOpts->fullscreen = true;
		GBAConfigSetOverrideIntValue(config, "fullscreen", 1);
		return true;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
		if (graphicsOpts->multiplier) {
			return false;
		}
		graphicsOpts->multiplier = option - '0';
		GBAConfigSetOverrideIntValue(config, "width", VIDEO_HORIZONTAL_PIXELS * graphicsOpts->multiplier);
		GBAConfigSetOverrideIntValue(config, "height", VIDEO_VERTICAL_PIXELS * graphicsOpts->multiplier);
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
	puts("  -c, --cheats FILE   Apply cheat codes from a file");
#ifdef USE_CLI_DEBUGGER
	puts("  -d, --debug         Use command-line debugger");
#endif
#ifdef USE_GDB_STUB
	puts("  -g, --gdb           Start GDB session (default port 2345)");
#endif
	puts("  -v, --movie FILE    Play back a movie of recorded input");
	puts("  -p, --patch FILE    Apply a specified patch file when running");
	puts("  -s, --frameskip N   Skip every N frames");
	puts("  --version           Print version and exit");
	if (extraOptions) {
		puts(extraOptions);
	}
}

void version(const char* arg0) {
	printf("%s %s (%s)\n", arg0, projectVersion, gitCommit);
}
