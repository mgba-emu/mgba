#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include "common.h"

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

struct StartupOptions {
	int fd;
	const char* fname;
	int biosFd;
	int patchFd;
	int logLevel;
	int frameskip;
	int rewindBufferCapacity;
	int rewindBufferInterval;

	enum DebuggerType debuggerType;
	int debugAtStart;
};

struct SubParser {
	const char* usage;
	bool (*parse)(struct SubParser* parser, int option, const char* arg);
	const char* extraOptions;
	void* opts;
};

struct GraphicsOpts {
	int multiplier;
	int fullscreen;
	int width;
	int height;
};

bool parseCommandArgs(struct StartupOptions* opts, int argc, char* const* argv, struct SubParser* subparser);
void usage(const char* arg0, const char* extraOptions);

void initParserForGraphics(struct SubParser* parser, struct GraphicsOpts* opts);
struct ARMDebugger* createDebugger(struct StartupOptions* opts);

#endif
