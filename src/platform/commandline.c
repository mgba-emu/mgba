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
	{ "gdb", 1, 0, 'g' },
	{ 0, 0, 0, 0 }
};

int parseCommandArgs(struct StartupOptions* opts, int argc, char* const* argv) {
	memset(opts, 0, sizeof(*opts));
	opts->fd = -1;
	opts->biosFd = -1;
	opts->width = 240;
	opts->height = 160;
	opts->fullscreen = 0;

	int ch;
	while ((ch = getopt_long(argc, argv, "b:dfg", _options, 0)) != -1) {
		switch (ch) {
		case 'b':
			opts->biosFd = open(optarg, O_RDONLY);
			break;
#ifdef USE_CLI_DEBUGGER
		case 'd':
			opts->debuggerType = DEBUGGER_CLI;
			break;
#endif
		case 'f':
			opts->fullscreen = 1;
			break;
#ifdef USE_GDB_STUB
		case 'g':
			opts->debuggerType = DEBUGGER_GDB;
			break;
#endif
		}
	}
	argc -= optind;
	argv += optind;
	if (argc) {
		opts->fname = argv[0];
	} else {
		opts->fname = _defaultFilename;
	}
	opts->fd = open(opts->fname, O_RDONLY);
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

void usage(const char* arg0) {
	printf("%s: bad arguments\n", arg0);
}
