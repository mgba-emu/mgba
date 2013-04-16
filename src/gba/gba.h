#ifndef GBA_H
#define GBA_H

#include "arm.h"

#include "gba-memory.h"
#include "gba-video.h"

enum GBAError {
	GBA_NO_ERROR = 0,
	GBA_OUT_OF_MEMORY = -1
};

enum GBALogLevel {
	GBA_LOG_STUB
};

struct GBABoard {
	struct ARMBoard d;
	struct GBA* p;
};

struct GBA {
	struct ARMCore cpu;
	struct GBABoard board;
	struct GBAMemory memory;
	struct GBAVideo video;

	struct ARMDebugger* debugger;

	enum GBAError errno;
	const char* errstr;
};

void GBAInit(struct GBA* gba);
void GBADeinit(struct GBA* gba);

void GBAMemoryInit(struct GBAMemory* memory);
void GBAMemoryDeinit(struct GBAMemory* memory);

void GBABoardInit(struct GBABoard* board);
void GBABoardReset(struct ARMBoard* board);

void GBAAttachDebugger(struct GBA* gba, struct ARMDebugger* debugger);

void GBALoadROM(struct GBA* gba, int fd);

void GBALog(int level, const char* format, ...);

#endif
