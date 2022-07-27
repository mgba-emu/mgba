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

#define GRAPHICS_OPTIONS "12345678f"
#define GRAPHICS_USAGE \
	"Graphics options:\n" \
	"  -1, -2, -3, -4, -5, -6, -7, -8  Scale viewport by 1-8 times\n" \
	"  -f, --fullscreen                Start full-screen\n" \
	"  --scale X                       Scale viewport by X times"

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
	{ "savestate", required_argument, 0, 't' },
	{ "patch",     required_argument, 0, 'p' },
	{ "version",   no_argument, 0, '\0' },
	{ 0, 0, 0, 0 }
};

static const struct mOption _graphicsLongOpts[] = {
	{ "fullscreen", false, 'f' },
	{ "scale", true, '\0' },
	{ 0, 0, 0 }
};

static bool _parseGraphicsArg(struct mSubParser* parser, int option, const char* arg);
static bool _parseLongGraphicsArg(struct mSubParser* parser, const char* option, const char* arg);
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

bool mArgumentsParse(struct mArguments* args, int argc, char* const* argv, struct mSubParser* subparsers, int nSubparsers) {
	int ch;
	char options[128] =
		"b:c:C:hl:p:s:t:"
#ifdef USE_EDITLINE
		"d"
#endif
#ifdef USE_GDB_STUB
		"g"
#endif
	;

	struct option longOptions[128] = {0};
	memcpy(longOptions, _options, sizeof(_options));

	memset(args, 0, sizeof(*args));
	args->frameskip = -1;
	args->logLevel = INT_MIN;
	HashTableInit(&args->configOverrides, 0, free);
	int lastLongOpt;

	int i, j;
	for (i = 0; _options[i].name; ++i); // Seek to end
	lastLongOpt = i;

	for (i = 0; i < nSubparsers; ++i) {
		if (subparsers[i].extraOptions) {
			strncat(options, subparsers[i].extraOptions, sizeof(options) - strlen(options) - 1);
		}
		if (subparsers[i].longOptions) {
			for (j = 0; subparsers[i].longOptions[j].name; ++j) {
				longOptions[lastLongOpt].name = subparsers[i].longOptions[j].name;
				longOptions[lastLongOpt].has_arg = subparsers[i].longOptions[j].arg ? required_argument : no_argument;
				longOptions[lastLongOpt].flag = NULL;
				longOptions[lastLongOpt].val = subparsers[i].longOptions[j].shortEquiv;
				++lastLongOpt;
			}
		}
	}
	bool ok = false;
	int index = 0;
	while ((ch = getopt_long(argc, argv, options, longOptions, &index)) != -1) {
		const struct option* opt = &longOptions[index];
		switch (ch) {
		case '\0':
			if (strcmp(opt->name, "version") == 0) {
				args->showVersion = true;
			} else {
				for (i = 0; i < nSubparsers; ++i) {
					if (subparsers[i].parseLong) {
						ok = subparsers[i].parseLong(&subparsers[i], opt->name, optarg) || ok;
					}
				}
				if (!ok) {
					return false;
				}
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
		case 't':
			args->savestate = strdup(optarg);
			break;
		default:
			for (i = 0; i < nSubparsers; ++i) {
				if (subparsers[i].parse) {
					ok = subparsers[i].parse(&subparsers[i], ch, optarg) || ok;
				}
			}
			if (!ok) {
				return false;
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

void mArgumentsApply(const struct mArguments* args, struct mSubParser* subparsers, int nSubparsers, struct mCoreConfig* config) {
	if (args->frameskip >= 0) {
		mCoreConfigSetOverrideIntValue(config, "frameskip", args->frameskip);
	}
	if (args->logLevel > INT_MIN) {
		mCoreConfigSetOverrideIntValue(config, "logLevel", args->logLevel);
	}
	if (args->bios) {
		mCoreConfigSetOverrideValue(config, "bios", args->bios);
		mCoreConfigSetOverrideIntValue(config, "useBios", true);
	}
	HashTableEnumerate(&args->configOverrides, _tableApply, config);
	int i;
	for (i = 0; i < nSubparsers; ++i) {
		if (subparsers[i].apply) {
			subparsers[i].apply(&subparsers[i], config);
		}
	}
}

void mArgumentsDeinit(struct mArguments* args) {
	free(args->fname);
	args->fname = 0;

	free(args->patch);
	args->patch = 0;

	free(args->savestate);
	args->savestate = 0;

	free(args->cheatsFile);
	args->cheatsFile = 0;

	free(args->bios);
	args->bios = 0;

	HashTableDeinit(&args->configOverrides);
}

void mSubParserGraphicsInit(struct mSubParser* parser, struct mGraphicsOpts* opts) {
	parser->usage = GRAPHICS_USAGE;
	parser->opts = opts;
	parser->parse = _parseGraphicsArg;
	parser->parseLong = _parseLongGraphicsArg;
	parser->apply = _applyGraphicsArgs;
	parser->extraOptions = GRAPHICS_OPTIONS;
	parser->longOptions = _graphicsLongOpts;
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
	case '7':
	case '8':
		if (graphicsOpts->multiplier) {
			return false;
		}
		graphicsOpts->multiplier = option - '0';
		return true;
	default:
		return false;
	}
}

bool _parseLongGraphicsArg(struct mSubParser* parser, const char* option, const char* arg) {
	struct mGraphicsOpts* graphicsOpts = parser->opts;
	if (strcmp(option, "scale") == 0) {
		if (graphicsOpts->multiplier) {
			return false;
		}
		graphicsOpts->multiplier = atoi(arg);
		return graphicsOpts->multiplier != 0;
	}
	return false;
}

void _applyGraphicsArgs(struct mSubParser* parser, struct mCoreConfig* config) {
	struct mGraphicsOpts* graphicsOpts = parser->opts;
	if (graphicsOpts->fullscreen) {
		mCoreConfigSetOverrideIntValue(config, "fullscreen", graphicsOpts->fullscreen);
	}
}

void usage(const char* arg0, const char* prologue, const char* epilogue, const struct mSubParser* subparsers, int nSubparsers) {
	printf("usage: %s [option ...] file\n", arg0);
	if (prologue) {
		puts(prologue);
	}
	puts("\nGeneric options:\n"
	     "  -b, --bios FILE            GBA BIOS file to use\n"
	     "  -c, --cheats FILE          Apply cheat codes from a file\n"
	     "  -C, --config OPTION=VALUE  Override config value\n"
#ifdef USE_EDITLINE
	     "  -d, --debug                Use command-line debugger\n"
#endif
#ifdef USE_GDB_STUB
	     "  -g, --gdb                  Start GDB session (default port 2345)\n"
#endif
	     "  -l, --log-level N          Log level mask\n"
	     "  -t, --savestate FILE       Load savestate when starting\n"
	     "  -p, --patch FILE           Apply a specified patch file when running\n"
	     "  -s, --frameskip N          Skip every N frames\n"
	     "  --version                  Print version and exit"
	);
	int i;
	for (i = 0; i < nSubparsers; ++i) {
		if (subparsers[i].usage) {
			puts("");
			puts(subparsers[i].usage);
		}
	}
	if (epilogue) {
		puts(epilogue);
	}
}

void version(const char* arg0) {
	printf("%s %s (%s)\n", arg0, projectVersion, gitCommit);
}
