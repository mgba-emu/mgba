#ifndef GBA_H
#define GBA_H

#include "arm.h"

#include "gba-memory.h"
#include "gba-video.h"
#include "gba-audio.h"

enum GBAIRQ {
	IRQ_VBLANK = 0x0,
	IRQ_HBLANK = 0x1,
	IRQ_VCOUNTER = 0x2,
	IRQ_TIMER0 = 0x3,
	IRQ_TIMER1 = 0x4,
	IRQ_TIMER2 = 0x5,
	IRQ_TIMER3 = 0x6,
	IRQ_SIO = 0x7,
	IRQ_DMA0 = 0x8,
	IRQ_DMA1 = 0x9,
	IRQ_DMA2 = 0xA,
	IRQ_DMA3 = 0xB,
	IRQ_KEYPAD = 0xC,
	IRQ_GAMEPAK = 0xD
};

enum GBAError {
	GBA_NO_ERROR = 0,
	GBA_OUT_OF_MEMORY = -1
};

enum GBALogLevel {
	GBA_LOG_STUB,
	GBA_LOG_DEBUG,
	GBA_LOG_GAME_ERROR,
	GBA_LOG_INFO,
	GBA_LOG_WARN,
	GBA_LOG_ERROR,
};

enum GBAKey {
	GBA_KEY_A = 0,
	GBA_KEY_B = 1,
	GBA_KEY_SELECT = 2,
	GBA_KEY_START = 3,
	GBA_KEY_RIGHT = 4,
	GBA_KEY_LEFT = 5,
	GBA_KEY_UP = 6,
	GBA_KEY_DOWN = 7,
	GBA_KEY_R = 8,
	GBA_KEY_L = 9
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
	struct GBAAudio audio;

	struct ARMDebugger* debugger;

	int timersEnabled;
	struct GBATimer {
		uint16_t reload;
		uint16_t oldReload;
		int32_t lastEvent;
		int32_t nextEvent;
		int32_t overflowInterval;
		unsigned prescaleBits : 4;
		unsigned countUp : 1;
		unsigned doIrq : 1;
		unsigned enable : 1;
	} timers[4];

	int springIRQ;
	int* keySource;

	const char* activeFile;
	const char* savefile;
	enum GBAError errno;
	const char* errstr;
	enum GBALogLevel logLevel;
};

void GBAInit(struct GBA* gba);
void GBADeinit(struct GBA* gba);

void GBAMemoryInit(struct GBAMemory* memory);
void GBAMemoryDeinit(struct GBAMemory* memory);

void GBABoardInit(struct GBABoard* board);
void GBABoardReset(struct ARMBoard* board);

void GBATimerUpdateRegister(struct GBA* gba, int timer);
void GBATimerWriteTMCNT_LO(struct GBA* gba, int timer, uint16_t value);
void GBATimerWriteTMCNT_HI(struct GBA* gba, int timer, uint16_t value);

void GBAWriteIE(struct GBA* gba, uint16_t value);
void GBAWriteIME(struct GBA* gba, uint16_t value);
void GBARaiseIRQ(struct GBA* gba, enum GBAIRQ irq);
int GBATestIRQ(struct GBA* gba);
int GBAWaitForIRQ(struct GBA* gba);
int GBAHalt(struct GBA* gba);

void GBAAttachDebugger(struct GBA* gba, struct ARMDebugger* debugger);

void GBALoadROM(struct GBA* gba, int fd, const char* fname);

__attribute__((format (printf, 3, 4)))
void GBALog(struct GBA* gba, enum GBALogLevel level, const char* format, ...);

#endif
