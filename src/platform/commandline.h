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

#define GRAPHICS_OPTIONS "234f"
#define GRAPHICS_USAGE \
	"\nGraphics options:\n" \
	"  -2               2x viewport\n" \
	"  -3               3x viewport\n" \
	"  -4               4x viewport\n" \
	"  -f               Sart full-screen"

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

int parseCommandArgs(struct StartupOptions* opts, int argc, char* const* argv, const char* extraOptions);
void usage(const char* arg0, const char* extraOptions);

struct ARMDebugger* createDebugger(struct StartupOptions* opts);

#endif
