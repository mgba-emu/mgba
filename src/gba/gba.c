#include "gba.h"

#include "gba-bios.h"
#include "gba-io.h"
#include "gba-thread.h"

#include "debugger.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

const uint32_t GBA_ARM7TDMI_FREQUENCY = 0x1000000;

enum {
	SP_BASE_SYSTEM = 0x03FFFF00,
	SP_BASE_IRQ = 0x03FFFFA0,
	SP_BASE_SUPERVISOR = 0x03FFFFE0
};

static const struct SavedataOverride _savedataOverrides[] = {
	{ 'EEPB', SAVEDATA_FLASH1M },
	{ 0, 0 }
};

static void GBAProcessEvents(struct ARMBoard* board);
static int32_t GBATimersProcessEvents(struct GBA* gba, int32_t cycles);
static void GBAHitStub(struct ARMBoard* board, uint32_t opcode);

static void _checkOverrides(struct GBA* gba, uint32_t code);

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

	gba->audio.p = gba;
	GBAAudioInit(&gba->audio);

	GBAIOInit(gba);

	memset(gba->timers, 0, sizeof(gba->timers));

	gba->springIRQ = 0;
	gba->keySource = 0;

	gba->logLevel = GBA_LOG_INFO | GBA_LOG_WARN | GBA_LOG_ERROR;

	ARMReset(&gba->cpu);
}

void GBADeinit(struct GBA* gba) {
	GBAMemoryDeinit(&gba->memory);
	GBAVideoDeinit(&gba->video);
	GBAAudioDeinit(&gba->audio);
}

void GBABoardInit(struct GBABoard* board) {
	board->d.reset = GBABoardReset;
	board->d.processEvents = GBAProcessEvents;
	board->d.swi16 = GBASwi16;
	board->d.swi32 = GBASwi32;
	board->d.readCPSR = GBATestIRQ;
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

	if (gbaBoard->p->springIRQ) {
		ARMRaiseIRQ(&gbaBoard->p->cpu);
		gbaBoard->p->springIRQ = 0;
	}

	testEvent = GBAVideoProcessEvents(&gbaBoard->p->video, cycles);
	if (testEvent < nextEvent) {
		nextEvent = testEvent;
	}

	testEvent = GBAAudioProcessEvents(&gbaBoard->p->audio, cycles);
	if (testEvent < nextEvent) {
		nextEvent = testEvent;
	}

	testEvent = GBAMemoryProcessEvents(&gbaBoard->p->memory, cycles);
	if (testEvent < nextEvent) {
		nextEvent = testEvent;
	}

	testEvent = GBATimersProcessEvents(gbaBoard->p, cycles);
	if (testEvent < nextEvent) {
		nextEvent = testEvent;
	}

	board->cpu->cycles = 0;
	board->cpu->nextEvent = nextEvent;
}

static int32_t GBATimersProcessEvents(struct GBA* gba, int32_t cycles) {
	int32_t nextEvent = INT_MAX;
	if (gba->timersEnabled) {
		struct GBATimer* timer;
		struct GBATimer* nextTimer;

		timer = &gba->timers[0];
		if (timer->enable) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			if (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				gba->memory.io[REG_TM0CNT_LO >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (timer->doIrq) {
					GBARaiseIRQ(gba, IRQ_TIMER0);
				}

				if (gba->audio.enable) {
					if ((gba->audio.chALeft || gba->audio.chARight) && gba->audio.chATimer == 0) {
						GBAAudioSampleFIFO(&gba->audio, 0);
					}

					if ((gba->audio.chBLeft || gba->audio.chBRight) && gba->audio.chBTimer == 0) {
						GBAAudioSampleFIFO(&gba->audio, 1);
					}
				}

				nextTimer = &gba->timers[1];
				if (nextTimer->countUp) {
					++gba->memory.io[REG_TM1CNT_LO >> 1];
					if (!gba->memory.io[REG_TM1CNT_LO >> 1]) {
						nextTimer->nextEvent = 0;
					}
				}
			}
			nextEvent = timer->nextEvent;
		}

		timer = &gba->timers[1];
		if (timer->enable) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			if (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				gba->memory.io[REG_TM1CNT_LO >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (timer->doIrq) {
					GBARaiseIRQ(gba, IRQ_TIMER1);
				}

				if (gba->audio.enable) {
					if ((gba->audio.chALeft || gba->audio.chARight) && gba->audio.chATimer == 1) {
						GBAAudioSampleFIFO(&gba->audio, 0);
					}

					if ((gba->audio.chBLeft || gba->audio.chBRight) && gba->audio.chBTimer == 1) {
						GBAAudioSampleFIFO(&gba->audio, 1);
					}
				}

				if (timer->countUp) {
					timer->nextEvent = INT_MAX;
				}

				nextTimer = &gba->timers[2];
				if (nextTimer->countUp) {
					++gba->memory.io[REG_TM2CNT_LO >> 1];
					if (!gba->memory.io[REG_TM2CNT_LO >> 1]) {
						nextTimer->nextEvent = 0;
					}
				}
			}
			if (timer->nextEvent < nextEvent) {
				nextEvent = timer->nextEvent;
			}
		}

		timer = &gba->timers[2];
		if (timer->enable) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			nextEvent = timer->nextEvent;
			if (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				gba->memory.io[REG_TM2CNT_LO >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (timer->doIrq) {
					GBARaiseIRQ(gba, IRQ_TIMER2);
				}

				if (timer->countUp) {
					timer->nextEvent = INT_MAX;
				}

				nextTimer = &gba->timers[3];
				if (nextTimer->countUp) {
					++gba->memory.io[REG_TM3CNT_LO >> 1];
					if (!gba->memory.io[REG_TM3CNT_LO >> 1]) {
						nextTimer->nextEvent = 0;
					}
				}
			}
			if (timer->nextEvent < nextEvent) {
				nextEvent = timer->nextEvent;
			}
		}

		timer = &gba->timers[3];
		if (timer->enable) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			nextEvent = timer->nextEvent;
			if (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				gba->memory.io[REG_TM3CNT_LO >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (timer->doIrq) {
					GBARaiseIRQ(gba, IRQ_TIMER3);
				}

				if (timer->countUp) {
					timer->nextEvent = INT_MAX;
				}
			}
			if (timer->nextEvent < nextEvent) {
				nextEvent = timer->nextEvent;
			}
		}
	}
	return nextEvent;
}

void GBAAttachDebugger(struct GBA* gba, struct ARMDebugger* debugger) {
	ARMDebuggerInit(debugger, &gba->cpu);
	gba->debugger = debugger;
}

void GBALoadROM(struct GBA* gba, int fd, const char* fname) {
	struct stat info;
	gba->memory.rom = mmap(0, SIZE_CART0, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	gba->activeFile = fname;
	fstat(fd, &info);
	gba->memory.romSize = info.st_size;
	_checkOverrides(gba, ((struct GBACartridge*) gba->memory.rom)->id);
	// TODO: error check
}

void GBALoadBIOS(struct GBA* gba, int fd) {
	gba->memory.bios = mmap(0, SIZE_BIOS, PROT_READ, MAP_SHARED, fd, 0);
	gba->memory.fullBios = 1;
	if ((gba->cpu.gprs[ARM_PC] >> BASE_OFFSET) == BASE_BIOS) {
		gba->memory.d.setActiveRegion(&gba->memory.d, gba->cpu.gprs[ARM_PC]);
	}
	// TODO: error check
}

void GBATimerUpdateRegister(struct GBA* gba, int timer) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	if (currentTimer->enable && !currentTimer->countUp) {
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->oldReload + ((gba->cpu.cycles - currentTimer->lastEvent) >> currentTimer->prescaleBits);
	}
}

void GBATimerWriteTMCNT_LO(struct GBA* gba, int timer, uint16_t reload) {
	gba->timers[timer].reload = reload;
}

void GBATimerWriteTMCNT_HI(struct GBA* gba, int timer, uint16_t control) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	GBATimerUpdateRegister(gba, timer);

	int oldPrescale = currentTimer->prescaleBits;
	switch (control & 0x0003) {
	case 0x0000:
		currentTimer->prescaleBits = 0;
		break;
	case 0x0001:
		currentTimer->prescaleBits = 6;
		break;
	case 0x0002:
		currentTimer->prescaleBits = 8;
		break;
	case 0x0003:
		currentTimer->prescaleBits = 10;
		break;
	}
	currentTimer->countUp = !!(control & 0x0004);
	currentTimer->doIrq = !!(control & 0x0040);
	currentTimer->overflowInterval = (0x10000 - currentTimer->reload) << currentTimer->prescaleBits;
	int wasEnabled = currentTimer->enable;
	currentTimer->enable = !!(control & 0x0080);
	if (!wasEnabled && currentTimer->enable) {
		if (!currentTimer->countUp) {
			currentTimer->nextEvent = gba->cpu.cycles + currentTimer->overflowInterval;
		} else {
			currentTimer->nextEvent = INT_MAX;
		}
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->reload;
		currentTimer->oldReload = currentTimer->reload;
		gba->timersEnabled |= 1 << timer;
	} else if (wasEnabled && !currentTimer->enable) {
		if (!currentTimer->countUp) {
			gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->oldReload + ((gba->cpu.cycles - currentTimer->lastEvent) >> oldPrescale);
		}
		gba->timersEnabled &= ~(1 << timer);
	} else if (currentTimer->prescaleBits != oldPrescale && !currentTimer->countUp) {
		// FIXME: this might be before present
		currentTimer->nextEvent = currentTimer->lastEvent + currentTimer->overflowInterval;
	}

	if (currentTimer->nextEvent < gba->cpu.nextEvent) {
		gba->cpu.nextEvent = currentTimer->nextEvent;
	}
};

void GBAWriteIE(struct GBA* gba, uint16_t value) {
	if (value & (1 << IRQ_SIO)) {
		GBALog(gba, GBA_LOG_STUB, "SIO interrupts not implemented");
	}

	if (value & (1 << IRQ_KEYPAD)) {
		GBALog(gba, GBA_LOG_STUB, "Keypad interrupts not implemented");
	}

	if (value & (1 << IRQ_GAMEPAK)) {
		GBALog(gba, GBA_LOG_STUB, "Gamepak interrupts not implemented");
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

void GBATestIRQ(struct ARMBoard* board) {
	struct GBABoard* gbaBoard = (struct GBABoard*) board;
	struct GBA* gba = gbaBoard->p;
	if (gba->memory.io[REG_IME >> 1] && gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1]) {
		gba->springIRQ = 1;
		gba->cpu.nextEvent = 0;
	}
}

int GBAWaitForIRQ(struct GBA* gba) {
	int irqs = gba->memory.io[REG_IF >> 1];
	int newIRQs = 0;
	gba->memory.io[REG_IF >> 1] = 0;
	while (1) {
		if (gba->cpu.nextEvent == INT_MAX) {
			break;
		} else {
			gba->cpu.cycles = gba->cpu.nextEvent;
			GBAProcessEvents(&gba->board.d);
			if (gba->memory.io[REG_IF >> 1]) {
				newIRQs = gba->memory.io[REG_IF >> 1];
				break;
			}
		}
	}
	gba->memory.io[REG_IF >> 1] = newIRQs | irqs;
	return newIRQs;
}

int GBAHalt(struct GBA* gba) {
	return GBAWaitForIRQ(gba);
}

void GBALog(struct GBA* gba, enum GBALogLevel level, const char* format, ...) {
	if (!gba) {
		struct GBAThread* threadContext = GBAThreadGetContext();
		if (threadContext) {
			gba = threadContext->gba;
		}
	}
	if (gba && !(level & gba->logLevel)) {
		return;
	}
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}

void GBAHitStub(struct ARMBoard* board, uint32_t opcode) {
	struct GBABoard* gbaBoard = (struct GBABoard*) board;
	GBALog(gbaBoard->p, GBA_LOG_STUB, "Stub opcode: %08x", opcode);
	if (!gbaBoard->p->debugger) {
		abort();
	} else {
		ARMDebuggerEnter(gbaBoard->p->debugger);
	}
}

void _checkOverrides(struct GBA* gba, uint32_t id) {
	int i;
	for (i = 0; _savedataOverrides[i].id; ++i) {
		if (_savedataOverrides[i].id == id) {
			gba->memory.savedata.type = _savedataOverrides[i].type;
			switch (_savedataOverrides[i].type) {
				case SAVEDATA_FLASH512:
				case SAVEDATA_FLASH1M:
					GBASavedataInitFlash(&gba->memory.savedata);
					break;
				case SAVEDATA_EEPROM:
					GBASavedataInitEEPROM(&gba->memory.savedata);
					break;
				case SAVEDATA_SRAM:
					GBASavedataInitSRAM(&gba->memory.savedata);
					break;
				case SAVEDATA_NONE:
					break;
			}
			return;
		}
	}
}
