#include "commandline.h"

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

void usage(const char* arg0) {
	printf("%s: bad arguments\n", arg0);
}
