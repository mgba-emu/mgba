/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba.h"

#include "gba-bios.h"
#include "gba-io.h"
#include "gba-rr.h"
#include "gba-sio.h"
#include "gba-thread.h"

#include "util/crc32.h"
#include "util/memory.h"
#include "util/patch.h"
#include "util/vfs.h"

const uint32_t GBA_ARM7TDMI_FREQUENCY = 0x1000000;
const uint32_t GBA_COMPONENT_MAGIC = 0x1000000;

static const size_t GBA_ROM_MAGIC_OFFSET = 2;
static const uint8_t GBA_ROM_MAGIC[] = { 0x00, 0xEA };

enum {
	SP_BASE_SYSTEM = 0x03FFFF00,
	SP_BASE_IRQ = 0x03FFFFA0,
	SP_BASE_SUPERVISOR = 0x03FFFFE0
};

struct GBACartridgeOverride {
	const char id[4];
	enum SavedataType type;
	int gpio;
	uint32_t busyLoop;
};

static const struct GBACartridgeOverride _overrides[] = {
	// Drill Dozer
	{ "V49J", SAVEDATA_SRAM, GPIO_RUMBLE, -1 },
	{ "V49E", SAVEDATA_SRAM, GPIO_RUMBLE, -1 },

	// Final Fantasy Tactics Advance
	{ "AFXE", SAVEDATA_FLASH512, GPIO_NONE, 0x8000418 },

	// Mega Man Battle Network
	{ "AREE", SAVEDATA_SRAM, GPIO_NONE, 0x800032E },

	// Pokemon Ruby
	{ "AXVJ", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVE", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVP", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVI", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVS", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVD", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXVF", SAVEDATA_FLASH1M, GPIO_RTC, -1 },

	// Pokemon Sapphire
	{ "AXPJ", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPE", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPP", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPI", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPS", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPD", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "AXPF", SAVEDATA_FLASH1M, GPIO_RTC, -1 },

	// Pokemon Emerald
	{ "BPEJ", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPEE", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPEP", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPEI", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPES", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPED", SAVEDATA_FLASH1M, GPIO_RTC, -1 },
	{ "BPEF", SAVEDATA_FLASH1M, GPIO_RTC, -1 },

	// Pokemon Mystery Dungeon
	{ "B24J", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "B24E", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "B24P", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "B24U", SAVEDATA_FLASH1M, GPIO_NONE, -1 },

	// Pokemon FireRed
	{ "BPRJ", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "BPRE", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "BPRP", SAVEDATA_FLASH1M, GPIO_NONE, -1 },

	// Pokemon LeafGreen
	{ "BPGJ", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "BPGE", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "BPGP", SAVEDATA_FLASH1M, GPIO_NONE, -1 },

	// RockMan EXE 4.5 - Real Operation
	{ "BR4J", SAVEDATA_FLASH512, GPIO_RTC, -1 },

	// Super Mario Advance 4
	{ "AX4J", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "AX4E", SAVEDATA_FLASH1M, GPIO_NONE, -1 },
	{ "AX4P", SAVEDATA_FLASH1M, GPIO_NONE, -1 },

	// Wario Ware Twisted
	{ "RZWJ", SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO, -1 },
	{ "RZWE", SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO, -1 },
	{ "RZWP", SAVEDATA_SRAM, GPIO_RUMBLE | GPIO_GYRO, -1 },

	{ { 0, 0, 0, 0 }, 0, 0, -1 }
};

static void GBAInit(struct ARMCore* cpu, struct ARMComponent* component);
static void GBAInterruptHandlerInit(struct ARMInterruptHandler* irqh);
static void GBAProcessEvents(struct ARMCore* cpu);
static int32_t GBATimersProcessEvents(struct GBA* gba, int32_t cycles);
static void GBAHitStub(struct ARMCore* cpu, uint32_t opcode);
static void GBAIllegal(struct ARMCore* cpu, uint32_t opcode);

static void _checkOverrides(struct GBA* gba, uint32_t code);

void GBACreate(struct GBA* gba) {
	gba->d.id = GBA_COMPONENT_MAGIC;
	gba->d.init = GBAInit;
	gba->d.deinit = 0;
}

static void GBAInit(struct ARMCore* cpu, struct ARMComponent* component) {
	struct GBA* gba = (struct GBA*) component;
	gba->cpu = cpu;
	gba->debugger = 0;

	GBAInterruptHandlerInit(&cpu->irqh);
	GBAMemoryInit(gba);
	GBASavedataInit(&gba->memory.savedata, 0);

	gba->video.p = gba;
	GBAVideoInit(&gba->video);

	gba->audio.p = gba;
	GBAAudioInit(&gba->audio, GBA_AUDIO_SAMPLES);

	GBAIOInit(gba);

	gba->sio.p = gba;
	GBASIOInit(&gba->sio);

	gba->timersEnabled = 0;
	memset(gba->timers, 0, sizeof(gba->timers));

	gba->springIRQ = 0;
	gba->keySource = 0;
	gba->rotationSource = 0;
	gba->rumble = 0;
	gba->rr = 0;

	gba->romVf = 0;
	gba->biosVf = 0;

	gba->logLevel = GBA_LOG_INFO | GBA_LOG_WARN | GBA_LOG_ERROR | GBA_LOG_FATAL;

	gba->biosChecksum = GBAChecksum(gba->memory.bios, SIZE_BIOS);

	gba->busyLoop = -1;
}

void GBADestroy(struct GBA* gba) {
	if (gba->pristineRom == gba->memory.rom) {
		gba->memory.rom = 0;
	}

	if (gba->romVf) {
		gba->romVf->unmap(gba->romVf, gba->pristineRom, gba->pristineRomSize);
	}

	if (gba->biosVf) {
		gba->biosVf->unmap(gba->biosVf, gba->memory.bios, SIZE_BIOS);
	}

	GBAMemoryDeinit(gba);
	GBAVideoDeinit(&gba->video);
	GBAAudioDeinit(&gba->audio);
	GBARRContextDestroy(gba);
}

void GBAInterruptHandlerInit(struct ARMInterruptHandler* irqh) {
	irqh->reset = GBAReset;
	irqh->processEvents = GBAProcessEvents;
	irqh->swi16 = GBASwi16;
	irqh->swi32 = GBASwi32;
	irqh->hitIllegal = GBAIllegal;
	irqh->readCPSR = GBATestIRQ;
	irqh->hitStub = GBAHitStub;
}

void GBAReset(struct ARMCore* cpu) {
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->gprs[ARM_SP] = SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->gprs[ARM_SP] = SP_BASE_SUPERVISOR;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_SP] = SP_BASE_SYSTEM;

	struct GBA* gba = (struct GBA*) cpu->master;
	if (!GBARRIsPlaying(gba->rr) && !GBARRIsRecording(gba->rr)) {
		GBASavedataUnmask(&gba->memory.savedata);
	}
	GBAMemoryReset(gba);
	GBAVideoReset(&gba->video);
	GBAAudioReset(&gba->audio);
	GBAIOInit(gba);

	GBASIODeinit(&gba->sio);
	GBASIOInit(&gba->sio);

	gba->timersEnabled = 0;
	memset(gba->timers, 0, sizeof(gba->timers));
}

static void GBAProcessEvents(struct ARMCore* cpu) {
	do {
		struct GBA* gba = (struct GBA*) cpu->master;
		int32_t cycles = cpu->cycles;
		int32_t nextEvent = INT_MAX;
		int32_t testEvent;

		gba->bus = cpu->prefetch;
		if (cpu->executionMode == MODE_THUMB) {
			gba->bus |= cpu->prefetch << 16;
		}

		if (gba->springIRQ) {
			ARMRaiseIRQ(cpu);
			gba->springIRQ = 0;
		}

		testEvent = GBAVideoProcessEvents(&gba->video, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		testEvent = GBAAudioProcessEvents(&gba->audio, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		testEvent = GBATimersProcessEvents(gba, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		testEvent = GBAMemoryRunDMAs(gba, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		testEvent = GBASIOProcessEvents(&gba->sio, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		cpu->cycles -= cycles;
		cpu->nextEvent = nextEvent;

		if (cpu->halted) {
			cpu->cycles = cpu->nextEvent;
		}
	} while (cpu->cycles >= cpu->nextEvent);
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
						GBAAudioSampleFIFO(&gba->audio, 0, timer->lastEvent);
					}

					if ((gba->audio.chBLeft || gba->audio.chBRight) && gba->audio.chBTimer == 0) {
						GBAAudioSampleFIFO(&gba->audio, 1, timer->lastEvent);
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
						GBAAudioSampleFIFO(&gba->audio, 0, timer->lastEvent);
					}

					if ((gba->audio.chBLeft || gba->audio.chBRight) && gba->audio.chBTimer == 1) {
						GBAAudioSampleFIFO(&gba->audio, 1, timer->lastEvent);
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
	gba->debugger = debugger;
}

void GBADetachDebugger(struct GBA* gba) {
	gba->debugger = 0;
}

void GBALoadROM(struct GBA* gba, struct VFile* vf, struct VFile* sav, const char* fname) {
	gba->romVf = vf;
	gba->pristineRomSize = vf->seek(vf, 0, SEEK_END);
	vf->seek(vf, 0, SEEK_SET);
	gba->pristineRom = vf->map(vf, SIZE_CART0, MAP_READ);
	if (!gba->pristineRom) {
		GBALog(gba, GBA_LOG_WARN, "Couldn't map ROM");
		return;
	}
	gba->memory.rom = gba->pristineRom;
	gba->activeFile = fname;
	gba->memory.romSize = gba->pristineRomSize;
	gba->romCrc32 = doCrc32(gba->memory.rom, gba->memory.romSize);
	GBASavedataInit(&gba->memory.savedata, sav);
	GBAGPIOInit(&gba->memory.gpio, &((uint16_t*) gba->memory.rom)[GPIO_REG_DATA >> 1]);
	_checkOverrides(gba, ((struct GBACartridge*) gba->memory.rom)->id);
	// TODO: error check
}

void GBALoadBIOS(struct GBA* gba, struct VFile* vf) {
	gba->biosVf = vf;
	uint32_t* bios = vf->map(vf, SIZE_BIOS, MAP_READ);
	if (!bios) {
		GBALog(gba, GBA_LOG_WARN, "Couldn't map BIOS");
		return;
	}
	gba->memory.bios = bios;
	gba->memory.fullBios = 1;
	uint32_t checksum = GBAChecksum(gba->memory.bios, SIZE_BIOS);
	GBALog(gba, GBA_LOG_DEBUG, "BIOS Checksum: 0x%X", checksum);
	if (checksum == GBA_BIOS_CHECKSUM) {
		GBALog(gba, GBA_LOG_INFO, "Official GBA BIOS detected");
	} else if (checksum == GBA_DS_BIOS_CHECKSUM) {
		GBALog(gba, GBA_LOG_INFO, "Official GBA (DS) BIOS detected");
	} else {
		GBALog(gba, GBA_LOG_WARN, "BIOS checksum incorrect");
	}
	gba->biosChecksum = checksum;
	if (gba->memory.activeRegion == REGION_BIOS) {
		gba->cpu->memory.activeRegion = gba->memory.bios;
	}
	// TODO: error check
}

void GBAApplyPatch(struct GBA* gba, struct Patch* patch) {
	size_t patchedSize = patch->outputSize(patch, gba->memory.romSize);
	if (!patchedSize) {
		return;
	}
	gba->memory.rom = anonymousMemoryMap(patchedSize);
	memcpy(gba->memory.rom, gba->pristineRom, gba->memory.romSize > patchedSize ? patchedSize : gba->memory.romSize);
	if (!patch->applyPatch(patch, gba->memory.rom, patchedSize)) {
		mappedMemoryFree(gba->memory.rom, patchedSize);
		gba->memory.rom = gba->pristineRom;
		return;
	}
	gba->memory.romSize = patchedSize;
	gba->romCrc32 = doCrc32(gba->memory.rom, gba->memory.romSize);
}

void GBATimerUpdateRegister(struct GBA* gba, int timer) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	if (currentTimer->enable && !currentTimer->countUp) {
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->oldReload + ((gba->cpu->cycles - currentTimer->lastEvent) >> currentTimer->prescaleBits);
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
			currentTimer->nextEvent = gba->cpu->cycles + currentTimer->overflowInterval;
		} else {
			currentTimer->nextEvent = INT_MAX;
		}
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->reload;
		currentTimer->oldReload = currentTimer->reload;
		currentTimer->lastEvent = 0;
		gba->timersEnabled |= 1 << timer;
	} else if (wasEnabled && !currentTimer->enable) {
		if (!currentTimer->countUp) {
			gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->oldReload + ((gba->cpu->cycles - currentTimer->lastEvent) >> oldPrescale);
		}
		gba->timersEnabled &= ~(1 << timer);
	} else if (currentTimer->prescaleBits != oldPrescale && !currentTimer->countUp) {
		// FIXME: this might be before present
		currentTimer->nextEvent = currentTimer->lastEvent + currentTimer->overflowInterval;
	}

	if (currentTimer->nextEvent < gba->cpu->nextEvent) {
		gba->cpu->nextEvent = currentTimer->nextEvent;
	}
};

void GBAWriteIE(struct GBA* gba, uint16_t value) {
	if (value & (1 << IRQ_KEYPAD)) {
		GBALog(gba, GBA_LOG_STUB, "Keypad interrupts not implemented");
	}

	if (value & (1 << IRQ_GAMEPAK)) {
		GBALog(gba, GBA_LOG_STUB, "Gamepak interrupts not implemented");
	}

	if (gba->memory.io[REG_IME >> 1] && value & gba->memory.io[REG_IF >> 1]) {
		ARMRaiseIRQ(gba->cpu);
	}
}

void GBAWriteIME(struct GBA* gba, uint16_t value) {
	if (value && gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1]) {
		ARMRaiseIRQ(gba->cpu);
	}
}

void GBARaiseIRQ(struct GBA* gba, enum GBAIRQ irq) {
	gba->memory.io[REG_IF >> 1] |= 1 << irq;
	gba->cpu->halted = 0;

	if (gba->memory.io[REG_IME >> 1] && (gba->memory.io[REG_IE >> 1] & 1 << irq)) {
		ARMRaiseIRQ(gba->cpu);
	}
}

void GBATestIRQ(struct ARMCore* cpu) {
	struct GBA* gba = (struct GBA*) cpu->master;
	if (gba->memory.io[REG_IME >> 1] && gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1]) {
		gba->springIRQ = 1;
		gba->cpu->nextEvent = 0;
	}
}

void GBAHalt(struct GBA* gba) {
	gba->cpu->nextEvent = 0;
	gba->cpu->halted = 1;
}

static void _GBAVLog(struct GBA* gba, enum GBALogLevel level, const char* format, va_list args) {
	struct GBAThread* threadContext = GBAThreadGetContext();
	if (threadContext) {
		if (!gba) {
			gba = threadContext->gba;
		}
	}

	if (gba && !(level & gba->logLevel) && level != GBA_LOG_FATAL) {
		return;
	}

	if (threadContext && threadContext->logHandler) {
		threadContext->logHandler(threadContext, level, format, args);
		return;
	}

	vprintf(format, args);
	printf("\n");

	if (level == GBA_LOG_FATAL) {
		abort();
	}
}

void GBALog(struct GBA* gba, enum GBALogLevel level, const char* format, ...) {
	va_list args;
	va_start(args, format);
	_GBAVLog(gba, level, format, args);
	va_end(args);
}

void GBADebuggerLogShim(struct ARMDebugger* debugger, enum DebuggerLogLevel level, const char* format, ...) {
	struct GBA* gba = 0;
	if (debugger->cpu) {
		gba = (struct GBA*) debugger->cpu->master;
	}

	enum GBALogLevel gbaLevel;
	switch (level) {
	default: // Avoids compiler warning
	case DEBUGGER_LOG_DEBUG:
		gbaLevel = GBA_LOG_DEBUG;
		break;
	case DEBUGGER_LOG_INFO:
		gbaLevel = GBA_LOG_INFO;
		break;
	case DEBUGGER_LOG_WARN:
		gbaLevel = GBA_LOG_WARN;
		break;
	case DEBUGGER_LOG_ERROR:
		gbaLevel = GBA_LOG_ERROR;
		break;
	}
	va_list args;
	va_start(args, format);
	_GBAVLog(gba, gbaLevel, format, args);
	va_end(args);
}

bool GBAIsROM(struct VFile* vf) {
	if (vf->seek(vf, GBA_ROM_MAGIC_OFFSET, SEEK_SET) < 0) {
		return false;
	}
	uint8_t signature[sizeof(GBA_ROM_MAGIC)];
	if (vf->read(vf, &signature, sizeof(signature)) != sizeof(signature)) {
		return false;
	}
	return memcmp(signature, GBA_ROM_MAGIC, sizeof(signature)) == 0;
}

void GBAGetGameCode(struct GBA* gba, char* out) {
	memcpy(out, &((struct GBACartridge*) gba->memory.rom)->id, 4);
}

void GBAGetGameTitle(struct GBA* gba, char* out) {
	memcpy(out, &((struct GBACartridge*) gba->memory.rom)->title, 12);
}

void GBAHitStub(struct ARMCore* cpu, uint32_t opcode) {
	struct GBA* gba = (struct GBA*) cpu->master;
	enum GBALogLevel level = GBA_LOG_FATAL;
	if (gba->debugger) {
		level = GBA_LOG_STUB;
		ARMDebuggerEnter(gba->debugger, DEBUGGER_ENTER_ILLEGAL_OP);
	}
	GBALog(gba, level, "Stub opcode: %08x", opcode);
}

void GBAIllegal(struct ARMCore* cpu, uint32_t opcode) {
	struct GBA* gba = (struct GBA*) cpu->master;
	GBALog(gba, GBA_LOG_WARN, "Illegal opcode: %08x", opcode);
	if (gba->debugger) {
		ARMDebuggerEnter(gba->debugger, DEBUGGER_ENTER_ILLEGAL_OP);
	}
}

void _checkOverrides(struct GBA* gba, uint32_t id) {
	int i;
	gba->busyLoop = -1;
	if ((id & 0xFF) == 'F') {
		GBALog(gba, GBA_LOG_DEBUG, "Found Classic NES Series game, using EEPROM saves");
		GBASavedataInitEEPROM(&gba->memory.savedata);
		return;
	}
	for (i = 0; _overrides[i].id[0]; ++i) {
		const uint32_t* overrideId = (const uint32_t*) _overrides[i].id;
		if (*overrideId == id) {
			GBALog(gba, GBA_LOG_DEBUG, "Found override for game %s!", _overrides[i].id);
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

			gba->busyLoop = _overrides[i].busyLoop;
			return;
		}
	}
}
