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

struct GBACartridgeOverride {
	uint32_t id;
	enum SavedataType type;
	int gpio;
};

static const struct GBACartridgeOverride _overrides[] = {
	// Boktai: The Sun is in Your Hand
	{ 'EI3U', SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR },
	{ 'PI3U', SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR },

	// Boktai 2: Solar Boy Django
	{ 'E23U', SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR },
	{ 'P23U', SAVEDATA_EEPROM, GPIO_RTC | GPIO_LIGHT_SENSOR },

	// Pokemon Ruby
	{ 'JVXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'EVXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'PVXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'IVXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'SVXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'DVXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'FVXA', SAVEDATA_FLASH1M, GPIO_RTC },

	// Pokemon Sapphire
	{ 'JPXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'EPXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'PPXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'IPXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'SPXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'DPXA', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'FPXA', SAVEDATA_FLASH1M, GPIO_RTC },

	// Pokemon Emerald
	{ 'JEPB', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'EEPB', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'PEPB', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'IEPB', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'SEPB', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'DEPB', SAVEDATA_FLASH1M, GPIO_RTC },
	{ 'FEPB', SAVEDATA_FLASH1M, GPIO_RTC },

	// RockMan EXE 4.5 - Real Operation
	{ 'J4RB', SAVEDATA_FLASH512, GPIO_RTC },

	// Super Mario Advance 4
	{ 'J4XA', SAVEDATA_FLASH1M, GPIO_NONE },
	{ 'E4XA', SAVEDATA_FLASH1M, GPIO_NONE },
	{ 'P4XA', SAVEDATA_FLASH1M, GPIO_NONE },

	// Wario Ware Twisted
	{ 'JWZR', SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO },
	{ 'EWZR', SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO },
	{ 'PWZR', SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO },

	{ 0, 0, 0 }
};

static void GBAProcessEvents(struct ARMBoard* board);
static int32_t GBATimersProcessEvents(struct GBA* gba, int32_t cycles);
static void GBAHitStub(struct ARMBoard* board, uint32_t opcode);

static void _checkOverrides(struct GBA* gba, uint32_t code);

void GBAInit(struct GBA* gba) {
	gba->debugger = 0;
	gba->savefile = 0;

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

	gba->timersEnabled = 0;
	memset(gba->timers, 0, sizeof(gba->timers));

	gba->springIRQ = 0;
	gba->keySource = 0;
	gba->rotationSource = 0;

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

#ifdef USE_DEBUGGER
void GBAAttachDebugger(struct GBA* gba, struct ARMDebugger* debugger) {
	ARMDebuggerInit(debugger, &gba->cpu);
	gba->debugger = debugger;
}
#endif

void GBALoadROM(struct GBA* gba, int fd, const char* fname) {
	struct stat info;
	gba->memory.rom = mmap(0, SIZE_CART0, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	gba->activeFile = fname;
	fstat(fd, &info);
	gba->memory.romSize = info.st_size;
	if (gba->savefile) {
		GBASavedataInit(&gba->memory.savedata, gba->savefile);
	}
	GBAGPIOInit(&gba->memory.gpio, &((uint16_t*) gba->memory.rom)[GPIO_REG_DATA >> 1]);
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
#ifdef USE_DEBUGGER
	if (!gbaBoard->p->debugger) {
		abort();
	} else {
		ARMDebuggerEnter(gbaBoard->p->debugger);
	}
#else
	abort();
#endif

}

void _checkOverrides(struct GBA* gba, uint32_t id) {
	int i;
	for (i = 0; _overrides[i].id; ++i) {
		if (_overrides[i].id == id) {
			switch (_overrides[i].type) {
				case SAVEDATA_FLASH512:
				case SAVEDATA_FLASH1M:
					gba->memory.savedata.type = _overrides[i].type;
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

			if (_overrides[i].gpio & GPIO_RTC) {
				GBAGPIOInitRTC(&gba->memory.gpio);
			}

			if (_overrides[i].gpio & GPIO_GYRO) {
				GBAGPIOInitGyro(&gba->memory.gpio);
			}

			if (_overrides[i].gpio & GPIO_RUMBLE) {
				GBAGPIOInitRumble(&gba->memory.gpio);
			}
			return;
		}
	}
}
