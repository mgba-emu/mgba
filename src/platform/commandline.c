#include "commandline.h"

#include "debugger/debugger.h"

#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"
#endif

#ifdef USE_GDB_STUB
#include "debugger/gdb-stub.h"
#endif

#include <fcntl.h>
#include <getopt.h>

#define GRAPHICS_OPTIONS "234f"
#define GRAPHICS_USAGE \
	"\nGraphics options:\n" \
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

bool _parseGraphicsArg(struct SubParser* parser, int option, const char* arg);

bool parseCommandArgs(struct StartupOptions* opts, int argc, char* const* argv, struct SubParser* subparser) {
	memset(opts, 0, sizeof(*opts));

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
	if (subparser->extraOptions) {
		// TODO: modularize options to subparsers
		strncat(options, subparser->extraOptions, sizeof(options) - strlen(options) - 1);
	}
	while ((ch = getopt_long(argc, argv, options, _options, 0)) != -1) {
		switch (ch) {
		case 'b':
			opts->bios = strdup(optarg);
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
			opts->logLevel = atoi(optarg);
			break;
		case 'p':
			opts->patch = strdup(optarg);
			break;
		case 's':
			opts->frameskip = atoi(optarg);
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
	if (argc != 1) {
		return false;
	}
	opts->fname = strdup(argv[0]);
	return true;
}

void freeOptions(struct StartupOptions* opts) {
	free(opts->fname);
	opts->fname = 0;

	free(opts->bios);
	opts->bios = 0;

	free(opts->patch);
	opts->patch = 0;
}

void initParserForGraphics(struct SubParser* parser, struct GraphicsOpts* opts) {
	parser->usage = GRAPHICS_USAGE;
	parser->opts = opts;
	parser->parse = _parseGraphicsArg;
	parser->extraOptions = GRAPHICS_OPTIONS;
	opts->multiplier = 1;
	opts->fullscreen = 0;
	opts->width = 240;
	opts->height = 160;
}

bool _parseGraphicsArg(struct SubParser* parser, int option, const char* arg) {
	UNUSED(arg);
	struct GraphicsOpts* graphicsOpts = parser->opts;
	switch (option) {
	case 'f':
		graphicsOpts->fullscreen = 1;
		return true;
	case '2':
		if (graphicsOpts->multiplier != 1) {
			return false;
		}
		graphicsOpts->multiplier = 2;
		graphicsOpts->width *= graphicsOpts->multiplier;
		graphicsOpts->height *= graphicsOpts->multiplier;
		return true;
	case '3':
		if (graphicsOpts->multiplier != 1) {
			return false;
		}
		graphicsOpts->multiplier = 3;
		graphicsOpts->width *= graphicsOpts->multiplier;
		graphicsOpts->height *= graphicsOpts->multiplier;
		return true;
	case '4':
		if (graphicsOpts->multiplier != 1) {
			return false;
		}
		graphicsOpts->multiplier = 4;
		graphicsOpts->width *= graphicsOpts->multiplier;
		graphicsOpts->height *= graphicsOpts->multiplier;
		return true;
	default:
		return false;
	}
}

struct ARMDebugger* createDebugger(struct StartupOptions* opts) {
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
	puts("  -b, --bios FILE  GBA BIOS file to use");
#ifdef USE_CLI_DEBUGGER
	puts("  -d, --debug      Use command-line debugger");
#endif
#ifdef USE_GDB_STUB
	puts("  -g, --gdb        Start GDB session (default port 2345)");
#endif
	if (extraOptions) {
		puts(extraOptions);
	}
}
