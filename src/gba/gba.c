#include "gba.h"

#include "gba-bios.h"
#include "gba-io.h"

#include "debugger.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

enum {
	SP_BASE_SYSTEM = 0x03FFFF00,
	SP_BASE_IRQ = 0x03FFFFA0,
	SP_BASE_SUPERVISOR = 0x03FFFFE0
};

static void GBAProcessEvents(struct ARMBoard* board);
static void GBAHitStub(struct ARMBoard* board, uint32_t opcode);

void GBAInit(struct GBA* gba) {
	gba->errno = GBA_NO_ERROR;
	gba->errstr = 0;

	ARMInit(&gba->cpu);

	gba->memory.p = gba;
	GBAMemoryInit(&gba->memory);
	ARMAssociateMemory(&gba->cpu, &gba->memory.d);

	gba->board.p = gba;
	GBABoardInit(&gba->board);
	ARMAssociateBoard(&gba->cpu, &gba->board.d);

	gba->video.p = gba;
	GBAVideoInit(&gba->video);

	GBAIOInit(gba);

	gba->springIRQ = 0;

	ARMReset(&gba->cpu);
}

void GBADeinit(struct GBA* gba) {
	GBAMemoryDeinit(&gba->memory);
}

void GBABoardInit(struct GBABoard* board) {
	board->d.reset = GBABoardReset;
	board->d.processEvents = GBAProcessEvents;
	board->d.swi16 = GBASwi16;
	board->d.swi32 = GBASwi32;
	board->d.hitStub = GBAHitStub;
}

void GBABoardReset(struct ARMBoard* board) {
	struct ARMCore* cpu = board->cpu;
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->gprs[ARM_SP] = SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->gprs[ARM_SP] = SP_BASE_SUPERVISOR;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_SP] = SP_BASE_SYSTEM;
}

static void GBAProcessEvents(struct ARMBoard* board) {
	struct GBABoard* gbaBoard = (struct GBABoard*) board;
	int32_t cycles = board->cpu->cycles;
	int32_t nextEvent = INT_MAX;
	int32_t testEvent;

	testEvent = GBAVideoProcessEvents(&gbaBoard->p->video, cycles);
	if (testEvent < nextEvent) {
		nextEvent = testEvent;
	}

	testEvent = GBAMemoryProcessEvents(&gbaBoard->p->memory, cycles);
	if (testEvent < nextEvent) {
		nextEvent = testEvent;
	}

	board->cpu->cycles = 0;
	board->cpu->nextEvent = nextEvent;
}

void GBAAttachDebugger(struct GBA* gba, struct ARMDebugger* debugger) {
	ARMDebuggerInit(debugger, &gba->cpu);
	gba->debugger = debugger;
}

void GBALoadROM(struct GBA* gba, int fd) {
	gba->memory.rom = mmap(0, SIZE_CART0, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FILE, fd, 0);
	// TODO: error check
}

void GBAWriteIE(struct GBA* gba, uint16_t value) {
	if (value & ((1 << IRQ_TIMER0) | (1 << IRQ_TIMER1) | (1 << IRQ_TIMER2) | (1 << IRQ_TIMER3))) {
		GBALog(GBA_LOG_STUB, "Timer interrupts not implemented");
	}

	if (value & (1 << IRQ_SIO)) {
		GBALog(GBA_LOG_STUB, "SIO interrupts not implemented");
	}

	if (value & (1 << IRQ_KEYPAD)) {
		GBALog(GBA_LOG_STUB, "Keypad interrupts not implemented");
	}

	if (value & (1 << IRQ_GAMEPAK)) {
		GBALog(GBA_LOG_STUB, "Gamepak interrupts not implemented");
	}

	if (gba->memory.io[REG_IME >> 1] && value & gba->memory.io[REG_IF >> 1]) {
		ARMRaiseIRQ(&gba->cpu);
	}
}

void GBAWriteIME(struct GBA* gba, uint16_t value) {
	if (value && gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1]) {
		ARMRaiseIRQ(&gba->cpu);
	}
}

void GBARaiseIRQ(struct GBA* gba, enum GBAIRQ irq) {
	gba->memory.io[REG_IF >> 1] |= 1 << irq;

	if (gba->memory.io[REG_IME >> 1] && (gba->memory.io[REG_IE >> 1] & 1 << irq)) {
		ARMRaiseIRQ(&gba->cpu);
	}
}

int GBATestIRQ(struct GBA* gba) {
	if (gba->memory.io[REG_IME >> 1] && gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1]) {
		gba->springIRQ = 1;
		gba->cpu.nextEvent = gba->cpu.cycles;
		return 1;
	}
	return 0;
}

int GBAWaitForIRQ(struct GBA* gba) {
	while (1) {
		if (gba->cpu.nextEvent == INT_MAX) {
			return 0;
		} else {
			gba->cpu.cycles = gba->cpu.nextEvent;
			GBAProcessEvents(&gba->board.d);
			if (gba->memory.io[REG_IF >> 1]) {
				return 1;
			}
		}
	}
}

int GBAHalt(struct GBA* gba) {
	return GBAWaitForIRQ(gba);
}

void GBALog(int level, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}

void GBAHitStub(struct ARMBoard* board, uint32_t opcode) {
	GBALog(GBA_LOG_STUB, "Stub opcode: %08x", opcode);
	struct GBABoard* gbaBoard = (struct GBABoard*) board;
	if (!gbaBoard->p->debugger) {
		abort();
	} else {
		ARMDebuggerEnter(gbaBoard->p->debugger);
	}
}
