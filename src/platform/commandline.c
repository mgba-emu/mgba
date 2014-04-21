#include "commandline.h"

#ifdef USE_CLI_DEBUGGER
#include "debugger/cli-debugger.h"
#endif

#ifdef USE_GDB_STUB
#include "debugger/gdb-stub.h"
#endif

#include <fcntl.h>
#include <getopt.h>

static const char* _defaultFilename = "test.rom";

static const struct option _options[] = {
	{ "bios", 1, 0, 'b' },
	{ "frameskip", 1, 0, 's' },
#ifdef USE_CLI_DEBUGGER
	{ "debug", 1, 0, 'd' },
#endif
#ifdef USE_GDB_STUB
	{ "gdb", 1, 0, 'g' },
#endif
	{ 0, 0, 0, 0 }
};

int parseCommandArgs(struct StartupOptions* opts, int argc, char* const* argv, const char* extraOptions) {
	memset(opts, 0, sizeof(*opts));
	opts->fd = -1;
	opts->biosFd = -1;
	opts->width = 240;
	opts->height = 160;

	int multiplier = 1;
	int ch;
	char options[64] =
		"b:s:"
#ifdef USE_CLI_DEBUGGER
		"d"
#endif
#ifdef USE_GDB_STUB
		"g"
#endif
	;
	if (extraOptions) {
		// TODO: modularize options to subparsers
		strncat(options, extraOptions, sizeof(options) - strlen(options) - 1);
	}
	while ((ch = getopt_long(argc, argv, options, _options, 0)) != -1) {
		switch (ch) {
		case 'b':
			opts->biosFd = open(optarg, O_RDONLY);
			break;
#ifdef USE_CLI_DEBUGGER
		case 'd':
			if (opts->debuggerType != DEBUGGER_NONE) {
				return 0;
			}
			opts->debuggerType = DEBUGGER_CLI;
			break;
#endif
		case 'f':
			opts->fullscreen = 1;
			break;
#ifdef USE_GDB_STUB
		case 'g':
			if (opts->debuggerType != DEBUGGER_NONE) {
				return 0;
			}
			opts->debuggerType = DEBUGGER_GDB;
			break;
#endif
		case 's':
			opts->frameskip = atoi(optarg);
			break;
		case 'S':
			opts->perfDuration = atoi(optarg);
			break;
		case '2':
			if (multiplier != 1) {
				return 0;
			}
			multiplier = 2;
			break;
		case '3':
			if (multiplier != 1) {
				return 0;
			}
			multiplier = 3;
			break;
		case '4':
			if (multiplier != 1) {
				return 0;
			}
			multiplier = 4;
			break;
		default:
			return 0;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 1) {
		opts->fname = argv[0];
	} else if (argc == 0) {
		opts->fname = _defaultFilename;
	} else {
		return 0;
	}
	opts->fd = open(opts->fname, O_RDONLY);
	opts->width *= multiplier;
	opts->height *= multiplier;
	return 1;
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
