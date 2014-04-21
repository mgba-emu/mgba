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
	int frameskip;
	int rewindBufferCapacity;
	int rewindBufferInterval;

	int width;
	int height;
	int fullscreen;

	enum DebuggerType debuggerType;
	int debugAtStart;
};

int parseCommandArgs(struct StartupOptions* opts, int argc, char* const* argv, int hasGraphics);
struct ARMDebugger* createDebugger(struct StartupOptions* opts);
void usage(const char* arg0);

#endif
