/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/feature/commandline.h>

#include <mgba/core/config.h>
#include <mgba/core/version.h>
#include <mgba-util/string.h>

#include <fcntl.h>
#ifdef _MSC_VER
#include <mgba-util/platform/windows/getopt.h>
#else
#include <getopt.h>
#endif

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
	{ "frameskip", required_argument, 0, 's' },
#ifdef USE_EDITLINE
	{ "debug",     no_argument, 0, 'd' },
#endif
#ifdef USE_GDB_STUB
	{ "gdb",       no_argument, 0, 'g' },
#endif
	{ "help",      no_argument, 0, 'h' },
	{ "log-level", required_argument, 0, 'l' },
	{ "movie",     required_argument, 0, 'v' },
	{ "patch",     required_argument, 0, 'p' },
	{ "version",   no_argument, 0, '\0' },
	{ 0, 0, 0, 0 }
};

static bool _parseGraphicsArg(struct mSubParser* parser, int option, const char* arg);
static void _applyGraphicsArgs(struct mSubParser* parser, struct mCoreConfig* config);

static void _tableInsert(struct Table* table, const char* pair) {
	char* eq = strchr(pair, '=');
	if (eq) {
		char option[128] = "";
		strncpy(option, pair, eq - pair);
		option[sizeof(option) - 1] = '\0';
		HashTableInsert(table, option, strdup(&eq[1]));
	} else {
		HashTableInsert(table, pair, strdup("1"));
	}
}

static void _tableApply(const char* key, void* value, void* user) {
	struct mCoreConfig* config = user;
	mCoreConfigSetOverrideValue(config, key, value);
}

bool parseArguments(struct mArguments* args, int argc, char* const* argv, struct mSubParser* subparser) {
	int ch;
	char options[64] =
		"b:c:C:hl:p:s:v:"
#ifdef USE_EDITLINE
		"d"
#endif
#ifdef USE_GDB_STUB
		"g"
#endif
	;
	memset(args, 0, sizeof(*args));
	args->frameskip = -1;
	args->logLevel = INT_MIN;
	HashTableInit(&args->configOverrides, 0, free);
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
				args->showVersion = true;
			} else {
				return false;
			}
			break;
		case 'b':
			args->bios = strdup(optarg);
			break;
		case 'c':
			args->cheatsFile = strdup(optarg);
			break;
		case 'C':
			_tableInsert(&args->configOverrides, optarg);
			break;
#ifdef USE_EDITLINE
		case 'd':
			if (args->debuggerType != DEBUGGER_NONE) {
				return false;
			}
			args->debuggerType = DEBUGGER_CLI;
			break;
#endif
#ifdef USE_GDB_STUB
		case 'g':
			if (args->debuggerType != DEBUGGER_NONE) {
				return false;
			}
			args->debuggerType = DEBUGGER_GDB;
			break;
#endif
		case 'h':
			args->showHelp = true;
			break;
		case 'l':
			args->logLevel = atoi(optarg);
			break;
		case 'p':
			args->patch = strdup(optarg);
			break;
		case 's':
			args->frameskip = atoi(optarg);
			break;
		case 'v':
			args->movie = strdup(optarg);
			break;
		default:
			if (subparser) {
				if (!subparser->parse(subparser, ch, optarg)) {
					return false;
				}
			}
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 1) {
		return false;
	} else if (argc == 1) {
		args->fname = strdup(argv[0]);
	} else {
		args->fname = NULL;
	}
	return true;
}

void applyArguments(const struct mArguments* args, struct mSubParser* subparser, struct mCoreConfig* config) {
	if (args->frameskip >= 0) {
		mCoreConfigSetOverrideIntValue(config, "frameskip", args->frameskip);
	}
	if (args->logLevel > INT_MIN) {
		mCoreConfigSetOverrideIntValue(config, "logLevel", args->logLevel);
	}
	if (args->bios) {
		mCoreConfigSetOverrideValue(config, "bios", args->bios);
	}
	HashTableEnumerate(&args->configOverrides, _tableApply, config);
	if (subparser) {
		subparser->apply(subparser, config);
	}
}

void freeArguments(struct mArguments* args) {
	free(args->fname);
	args->fname = 0;

	free(args->patch);
	args->patch = 0;

	free(args->movie);
	args->movie = 0;

	free(args->cheatsFile);
	args->cheatsFile = 0;

	free(args->bios);
	args->bios = 0;

	HashTableDeinit(&args->configOverrides);
}

void initParserForGraphics(struct mSubParser* parser, struct mGraphicsOpts* opts) {
	parser->usage = GRAPHICS_USAGE;
	parser->opts = opts;
	parser->parse = _parseGraphicsArg;
	parser->apply = _applyGraphicsArgs;
	parser->extraOptions = GRAPHICS_OPTIONS;
	opts->multiplier = 0;
	opts->fullscreen = false;
}

bool _parseGraphicsArg(struct mSubParser* parser, int option, const char* arg) {
	UNUSED(arg);
	struct mGraphicsOpts* graphicsOpts = parser->opts;
	switch (option) {
	case 'f':
		graphicsOpts->fullscreen = true;
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
		return true;
	default:
		return false;
	}
}

void _applyGraphicsArgs(struct mSubParser* parser, struct mCoreConfig* config) {
	struct mGraphicsOpts* graphicsOpts = parser->opts;
	if (graphicsOpts->fullscreen) {
		mCoreConfigSetOverrideIntValue(config, "fullscreen", graphicsOpts->fullscreen);
	}
}

void usage(const char* arg0, const char* extraOptions) {
	printf("usage: %s [option ...] file\n", arg0);
	puts("\nGeneric options:");
	puts("  -b, --bios FILE            GBA BIOS file to use");
	puts("  -c, --cheats FILE          Apply cheat codes from a file");
	puts("  -C, --config OPTION=VALUE  Override config value");
#ifdef USE_EDITLINE
	puts("  -d, --debug                Use command-line debugger");
#endif
#ifdef USE_GDB_STUB
	puts("  -g, --gdb                  Start GDB session (default port 2345)");
#endif
	puts("  -l, --log-level N          Log level mask");
	puts("  -v, --movie FILE           Play back a movie of recorded input");
	puts("  -p, --patch FILE           Apply a specified patch file when running");
	puts("  -s, --frameskip N          Skip every N frames");
	puts("  --version                  Print version and exit");
	if (extraOptions) {
		puts(extraOptions);
	}
}

void version(const char* arg0) {
	printf("%s %s (%s)\n", arg0, projectVersion, gitCommit);
}
