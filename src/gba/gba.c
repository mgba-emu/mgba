/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba.h"

#include "core/thread.h"

#include "arm/decoder.h"
#include "arm/debugger/debugger.h"
#include "arm/isa-inlines.h"

#include "gba/bios.h"
#include "gba/cheats.h"
#include "gba/io.h"
#include "gba/overrides.h"
#include "gba/rr/rr.h"
#include "gba/serialize.h"
#include "gba/sio.h"
#include "gba/vfame.h"

#include "util/crc32.h"
#include "util/memory.h"
#include "util/math.h"
#include "util/patch.h"
#include "util/vfs.h"

mLOG_DEFINE_CATEGORY(GBA, "GBA");

const uint32_t GBA_COMPONENT_MAGIC = 0x1000000;

static const size_t GBA_ROM_MAGIC_OFFSET = 3;
static const uint8_t GBA_ROM_MAGIC[] = { 0xEA };

static const size_t GBA_MB_MAGIC_OFFSET = 0xC0;

static void GBAInit(void* cpu, struct mCPUComponent* component);
static void GBAInterruptHandlerInit(struct ARMInterruptHandler* irqh);
static void GBAProcessEvents(struct ARMCore* cpu);
static int32_t GBATimersProcessEvents(struct GBA* gba, int32_t cycles);
static void GBAHitStub(struct ARMCore* cpu, uint32_t opcode);
static void GBAIllegal(struct ARMCore* cpu, uint32_t opcode);
static void GBABreakpoint(struct ARMCore* cpu, int immediate);

static bool _setSoftwareBreakpoint(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t* opcode);
static bool _clearSoftwareBreakpoint(struct ARMDebugger*, uint32_t address, enum ExecutionMode mode, uint32_t opcode);


#ifdef _3DS
extern uint32_t* romBuffer;
extern size_t romBufferSize;
#endif

void GBACreate(struct GBA* gba) {
	gba->d.id = GBA_COMPONENT_MAGIC;
	gba->d.init = GBAInit;
	gba->d.deinit = 0;
}

static void GBAInit(void* cpu, struct mCPUComponent* component) {
	struct GBA* gba = (struct GBA*) component;
	gba->cpu = cpu;
	gba->debugger = 0;
	gba->sync = 0;

	GBAInterruptHandlerInit(&gba->cpu->irqh);
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

	GBAHardwareInit(&gba->memory.hw, NULL);

	gba->springIRQ = 0;
	gba->keySource = 0;
	gba->rotationSource = 0;
	gba->luminanceSource = 0;
	gba->rtcSource = 0;
	gba->rumble = 0;
	gba->rr = 0;

	gba->romVf = 0;
	gba->biosVf = 0;

	gba->stream = 0;
	gba->keyCallback = 0;
	gba->stopCallback = 0;

	gba->biosChecksum = GBAChecksum(gba->memory.bios, SIZE_BIOS);

	gba->idleOptimization = IDLE_LOOP_REMOVE;
	gba->idleLoop = IDLE_LOOP_NONE;

	gba->realisticTiming = true;
	gba->hardCrash = true;
	gba->allowOpposingDirections = true;

	gba->performingDMA = false;

	gba->pristineRom = 0;
	gba->pristineRomSize = 0;
	gba->yankedRomSize = 0;
}

void GBAUnloadROM(struct GBA* gba) {
	if (gba->memory.rom && gba->pristineRom != gba->memory.rom) {
		if (gba->yankedRomSize) {
			gba->yankedRomSize = 0;
		}
		mappedMemoryFree(gba->memory.rom, SIZE_CART0);
	}
	gba->memory.rom = 0;

	if (gba->romVf) {
#ifndef _3DS
		gba->romVf->unmap(gba->romVf, gba->pristineRom, gba->pristineRomSize);
#endif
		gba->romVf->close(gba->romVf);
		gba->romVf = 0;
	}
	gba->pristineRom = 0;

	GBASavedataDeinit(&gba->memory.savedata);
	if (gba->memory.savedata.realVf) {
		gba->memory.savedata.realVf->close(gba->memory.savedata.realVf);
		gba->memory.savedata.realVf = 0;
	}
	gba->idleLoop = IDLE_LOOP_NONE;
}

void GBADestroy(struct GBA* gba) {
	GBAUnloadROM(gba);

	if (gba->biosVf) {
		gba->biosVf->unmap(gba->biosVf, gba->memory.bios, SIZE_BIOS);
		gba->biosVf->close(gba->biosVf);
		gba->biosVf = 0;
	}

	GBAMemoryDeinit(gba);
	GBAVideoDeinit(&gba->video);
	GBAAudioDeinit(&gba->audio);
	GBASIODeinit(&gba->sio);
	gba->rr = 0;
}

void GBAInterruptHandlerInit(struct ARMInterruptHandler* irqh) {
	irqh->reset = GBAReset;
	irqh->processEvents = GBAProcessEvents;
	irqh->swi16 = GBASwi16;
	irqh->swi32 = GBASwi32;
	irqh->hitIllegal = GBAIllegal;
	irqh->readCPSR = GBATestIRQ;
	irqh->hitStub = GBAHitStub;
	irqh->bkpt16 = GBABreakpoint;
	irqh->bkpt32 = GBABreakpoint;
}

void GBAReset(struct ARMCore* cpu) {
	ARMSetPrivilegeMode(cpu, MODE_IRQ);
	cpu->gprs[ARM_SP] = SP_BASE_IRQ;
	ARMSetPrivilegeMode(cpu, MODE_SUPERVISOR);
	cpu->gprs[ARM_SP] = SP_BASE_SUPERVISOR;
	ARMSetPrivilegeMode(cpu, MODE_SYSTEM);
	cpu->gprs[ARM_SP] = SP_BASE_SYSTEM;

	struct GBA* gba = (struct GBA*) cpu->master;
	if (!gba->rr || (!gba->rr->isPlaying(gba->rr) && !gba->rr->isRecording(gba->rr))) {
		GBASavedataUnmask(&gba->memory.savedata);
	}

	if (gba->yankedRomSize) {
		gba->memory.romSize = gba->yankedRomSize;
		gba->memory.romMask = toPow2(gba->memory.romSize) - 1;
		gba->yankedRomSize = 0;
	}
	GBAMemoryReset(gba);
	GBAVideoReset(&gba->video);
	GBAAudioReset(&gba->audio);
	GBAIOInit(gba);

	GBASIOReset(&gba->sio);

	gba->timersEnabled = 0;
	memset(gba->timers, 0, sizeof(gba->timers));

	gba->lastJump = 0;
	gba->haltPending = false;
	gba->idleDetectionStep = 0;
	gba->idleDetectionFailures = 0;
}

void GBASkipBIOS(struct GBA* gba) {
	struct ARMCore* cpu = gba->cpu;
	if (cpu->gprs[ARM_PC] == BASE_RESET + WORD_SIZE_ARM) {
		if (gba->memory.rom) {
			cpu->gprs[ARM_PC] = BASE_CART0;
		} else {
			cpu->gprs[ARM_PC] = BASE_WORKING_RAM;
		}
		int currentCycles = 0;
		ARM_WRITE_PC;
	}
}

static void GBAProcessEvents(struct ARMCore* cpu) {
	struct GBA* gba = (struct GBA*) cpu->master;

	gba->bus = cpu->prefetch[1];
	if (cpu->executionMode == MODE_THUMB) {
		gba->bus |= cpu->prefetch[1] << 16;
	}

	if (gba->springIRQ && !cpu->cpsr.i) {
		ARMRaiseIRQ(cpu);
		gba->springIRQ = 0;
	}

	do {
		int32_t cycles = cpu->nextEvent;
		int32_t nextEvent = INT_MAX;
		int32_t testEvent;
#ifndef NDEBUG
		if (cycles < 0) {
			mLOG(GBA, FATAL, "Negative cycles passed: %i", cycles);
		}
#endif

		testEvent = GBAVideoProcessEvents(&gba->video, cycles);
		if (testEvent < nextEvent) {
#ifndef NDEBUG
			if (testEvent == 0) {
				mLOG(GBA, ERROR, "Video requiring 0 cycles");
			}
#endif
			nextEvent = testEvent;
		}

		testEvent = GBAAudioProcessEvents(&gba->audio, cycles);
		if (testEvent < nextEvent) {
#ifndef NDEBUG
			if (testEvent == 0) {
				mLOG(GBA, ERROR, "Audio requiring 0 cycles");
			}
#endif
			nextEvent = testEvent;
		}

		testEvent = GBATimersProcessEvents(gba, cycles);
		if (testEvent < nextEvent) {
#ifndef NDEBUG
			if (testEvent == 0) {
				mLOG(GBA, ERROR, "Timers requiring 0 cycles");
			}
#endif
			nextEvent = testEvent;
		}

		testEvent = GBAMemoryRunDMAs(gba, cycles);
		if (testEvent < nextEvent) {
#ifndef NDEBUG
			if (testEvent == 0) {
				mLOG(GBA, ERROR, "DMAs requiring 0 cycles");
			}
#endif
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
			if (!gba->memory.io[REG_IME >> 1] || !gba->memory.io[REG_IE >> 1]) {
				break;
			}
		}
		if (nextEvent == 0) {
			break;
		}
#ifndef NDEBUG
		else if (nextEvent < 0) {
			mLOG(GBA, FATAL, "Negative cycles will pass: %i", nextEvent);
		}
#endif
	} while (cpu->cycles >= cpu->nextEvent);
}

static int32_t GBATimersProcessEvents(struct GBA* gba, int32_t cycles) {
	int32_t nextEvent = INT_MAX;
	if (gba->timersEnabled) {
		struct GBATimer* timer;
		struct GBATimer* nextTimer;

		timer = &gba->timers[0];
		if (GBATimerFlagsIsEnable(timer->flags) && timer->nextEvent != INT_MAX) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			while (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				gba->memory.io[REG_TM0CNT_LO >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (GBATimerFlagsIsDoIrq(timer->flags)) {
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
				if (GBATimerFlagsIsCountUp(nextTimer->flags)) {
					++gba->memory.io[REG_TM1CNT_LO >> 1];
					if (!gba->memory.io[REG_TM1CNT_LO >> 1]) {
						nextTimer->nextEvent = cycles;
					}
				}
			}
			nextEvent = timer->nextEvent;
		}

		timer = &gba->timers[1];
		if (GBATimerFlagsIsEnable(timer->flags) && timer->nextEvent != INT_MAX) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			if (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				gba->memory.io[REG_TM1CNT_LO >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (GBATimerFlagsIsDoIrq(timer->flags)) {
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

				if (GBATimerFlagsIsCountUp(timer->flags)) {
					timer->nextEvent = INT_MAX;
				}

				nextTimer = &gba->timers[2];
				if (GBATimerFlagsIsCountUp(nextTimer->flags)) {
					++gba->memory.io[REG_TM2CNT_LO >> 1];
					if (!gba->memory.io[REG_TM2CNT_LO >> 1]) {
						nextTimer->nextEvent = cycles;
					}
				}
			}
			if (timer->nextEvent < nextEvent) {
				nextEvent = timer->nextEvent;
			}
		}

		timer = &gba->timers[2];
		if (GBATimerFlagsIsEnable(timer->flags) && timer->nextEvent != INT_MAX) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			if (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				gba->memory.io[REG_TM2CNT_LO >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (GBATimerFlagsIsDoIrq(timer->flags)) {
					GBARaiseIRQ(gba, IRQ_TIMER2);
				}

				if (GBATimerFlagsIsCountUp(timer->flags)) {
					timer->nextEvent = INT_MAX;
				}

				nextTimer = &gba->timers[3];
				if (GBATimerFlagsIsCountUp(nextTimer->flags)) {
					++gba->memory.io[REG_TM3CNT_LO >> 1];
					if (!gba->memory.io[REG_TM3CNT_LO >> 1]) {
						nextTimer->nextEvent = cycles;
					}
				}
			}
			if (timer->nextEvent < nextEvent) {
				nextEvent = timer->nextEvent;
			}
		}

		timer = &gba->timers[3];
		if (GBATimerFlagsIsEnable(timer->flags) && timer->nextEvent != INT_MAX) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			if (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				gba->memory.io[REG_TM3CNT_LO >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (GBATimerFlagsIsDoIrq(timer->flags)) {
					GBARaiseIRQ(gba, IRQ_TIMER3);
				}

				if (GBATimerFlagsIsCountUp(timer->flags)) {
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

void GBAAttachDebugger(struct GBA* gba, struct mDebugger* debugger) {
	gba->debugger = (struct ARMDebugger*) debugger->platform;
	gba->debugger->setSoftwareBreakpoint = _setSoftwareBreakpoint;
	gba->debugger->clearSoftwareBreakpoint = _clearSoftwareBreakpoint;
	gba->cpu->components[CPU_COMPONENT_DEBUGGER] = &debugger->d;
	ARMHotplugAttach(gba->cpu, CPU_COMPONENT_DEBUGGER);
}

void GBADetachDebugger(struct GBA* gba) {
	gba->debugger = 0;
	ARMHotplugDetach(gba->cpu, CPU_COMPONENT_DEBUGGER);
	gba->cpu->components[CPU_COMPONENT_DEBUGGER] = 0;
}

bool GBALoadMB(struct GBA* gba, struct VFile* vf) {
	GBAUnloadROM(gba);
	gba->romVf = vf;
	gba->pristineRomSize = vf->size(vf);
	vf->seek(vf, 0, SEEK_SET);
	if (gba->pristineRomSize > SIZE_WORKING_RAM) {
		gba->pristineRomSize = SIZE_WORKING_RAM;
	}
#ifdef _3DS
	gba->pristineRom = 0;
	if (gba->pristineRomSize <= romBufferSize) {
		gba->pristineRom = romBuffer;
		vf->read(vf, romBuffer, gba->pristineRomSize);
	}
#else
	gba->pristineRom = vf->map(vf, gba->pristineRomSize, MAP_READ);
#endif
	if (!gba->pristineRom) {
		mLOG(GBA, WARN, "Couldn't map ROM");
		return false;
	}
	gba->yankedRomSize = 0;
	gba->memory.romSize = 0;
	gba->memory.romMask = 0;
	gba->romCrc32 = doCrc32(gba->pristineRom, gba->pristineRomSize);
	return true;
}

bool GBALoadROM(struct GBA* gba, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	GBAUnloadROM(gba);
	gba->romVf = vf;
	gba->pristineRomSize = vf->size(vf);
	vf->seek(vf, 0, SEEK_SET);
	if (gba->pristineRomSize > SIZE_CART0) {
		gba->pristineRomSize = SIZE_CART0;
	}
#ifdef _3DS
	gba->pristineRom = 0;
	if (gba->pristineRomSize <= romBufferSize) {
		gba->pristineRom = romBuffer;
		vf->read(vf, romBuffer, gba->pristineRomSize);
	}
#else
	gba->pristineRom = vf->map(vf, gba->pristineRomSize, MAP_READ);
#endif
	if (!gba->pristineRom) {
		mLOG(GBA, WARN, "Couldn't map ROM");
		return false;
	}
	gba->yankedRomSize = 0;
	gba->memory.rom = gba->pristineRom;
	gba->memory.romSize = gba->pristineRomSize;
	gba->memory.romMask = toPow2(gba->memory.romSize) - 1;
	gba->memory.mirroring = false;
	gba->romCrc32 = doCrc32(gba->memory.rom, gba->memory.romSize);
	GBAHardwareInit(&gba->memory.hw, &((uint16_t*) gba->memory.rom)[GPIO_REG_DATA >> 1]);
	GBAVFameDetect(&gba->memory.vfame, gba->memory.rom, gba->memory.romSize);
	// TODO: error check
	return true;
}

bool GBALoadSave(struct GBA* gba, struct VFile* sav) {
	GBASavedataInit(&gba->memory.savedata, sav);
	return true;
}

void GBAYankROM(struct GBA* gba) {
	gba->yankedRomSize = gba->memory.romSize;
	gba->memory.romSize = 0;
	gba->memory.romMask = 0;
	GBARaiseIRQ(gba, IRQ_GAMEPAK);
}

void GBALoadBIOS(struct GBA* gba, struct VFile* vf) {
	gba->biosVf = vf;
	uint32_t* bios = vf->map(vf, SIZE_BIOS, MAP_READ);
	if (!bios) {
		mLOG(GBA, WARN, "Couldn't map BIOS");
		return;
	}
	gba->memory.bios = bios;
	gba->memory.fullBios = 1;
	uint32_t checksum = GBAChecksum(gba->memory.bios, SIZE_BIOS);
	mLOG(GBA, DEBUG, "BIOS Checksum: 0x%X", checksum);
	if (checksum == GBA_BIOS_CHECKSUM) {
		mLOG(GBA, INFO, "Official GBA BIOS detected");
	} else if (checksum == GBA_DS_BIOS_CHECKSUM) {
		mLOG(GBA, INFO, "Official GBA (DS) BIOS detected");
	} else {
		mLOG(GBA, WARN, "BIOS checksum incorrect");
	}
	gba->biosChecksum = checksum;
	if (gba->memory.activeRegion == REGION_BIOS) {
		gba->cpu->memory.activeRegion = gba->memory.bios;
	}
	// TODO: error check
}

void GBAApplyPatch(struct GBA* gba, struct Patch* patch) {
	size_t patchedSize = patch->outputSize(patch, gba->memory.romSize);
	if (!patchedSize || patchedSize > SIZE_CART0) {
		return;
	}
	gba->memory.rom = anonymousMemoryMap(SIZE_CART0);
	if (!patch->applyPatch(patch, gba->pristineRom, gba->pristineRomSize, gba->memory.rom, patchedSize)) {
		mappedMemoryFree(gba->memory.rom, patchedSize);
		gba->memory.rom = gba->pristineRom;
		return;
	}
	gba->memory.romSize = patchedSize;
	gba->memory.romMask = SIZE_CART0 - 1;
	gba->romCrc32 = doCrc32(gba->memory.rom, gba->memory.romSize);
}

void GBATimerUpdateRegister(struct GBA* gba, int timer) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	if (GBATimerFlagsIsEnable(currentTimer->flags) && !GBATimerFlagsIsCountUp(currentTimer->flags)) {
		int32_t prefetchSkew = 0;
		if (gba->memory.lastPrefetchedPc >= (uint32_t) gba->cpu->gprs[ARM_PC]) {
			prefetchSkew = (gba->memory.lastPrefetchedPc - gba->cpu->gprs[ARM_PC]) * (gba->cpu->memory.activeSeqCycles16 + 1) / WORD_SIZE_THUMB;
		}
		// Reading this takes two cycles (1N+1I), so let's remove them preemptively
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->oldReload + ((gba->cpu->cycles - currentTimer->lastEvent - 2 + prefetchSkew) >> GBATimerFlagsGetPrescaleBits(currentTimer->flags));
	}
}

void GBATimerWriteTMCNT_LO(struct GBA* gba, int timer, uint16_t reload) {
	gba->timers[timer].reload = reload;
	gba->timers[timer].overflowInterval = (0x10000 - gba->timers[timer].reload) << GBATimerFlagsGetPrescaleBits(gba->timers[timer].flags);
}

void GBATimerWriteTMCNT_HI(struct GBA* gba, int timer, uint16_t control) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	GBATimerUpdateRegister(gba, timer);

	unsigned oldPrescale = GBATimerFlagsGetPrescaleBits(currentTimer->flags);
	switch (control & 0x0003) {
	case 0x0000:
		currentTimer->flags = GBATimerFlagsSetPrescaleBits(currentTimer->flags, 0);
		break;
	case 0x0001:
		currentTimer->flags = GBATimerFlagsSetPrescaleBits(currentTimer->flags, 6);
		break;
	case 0x0002:
		currentTimer->flags = GBATimerFlagsSetPrescaleBits(currentTimer->flags, 8);
		break;
	case 0x0003:
		currentTimer->flags = GBATimerFlagsSetPrescaleBits(currentTimer->flags, 10);
		break;
	}
	currentTimer->flags = GBATimerFlagsTestFillCountUp(currentTimer->flags, timer > 0 && (control & 0x0004));
	currentTimer->flags = GBATimerFlagsTestFillDoIrq(currentTimer->flags, control & 0x0040);
	currentTimer->overflowInterval = (0x10000 - currentTimer->reload) << GBATimerFlagsGetPrescaleBits(currentTimer->flags);
	bool wasEnabled = GBATimerFlagsIsEnable(currentTimer->flags);
	currentTimer->flags = GBATimerFlagsTestFillEnable(currentTimer->flags, control & 0x0080);
	if (!wasEnabled && GBATimerFlagsIsEnable(currentTimer->flags)) {
		if (!GBATimerFlagsIsCountUp(currentTimer->flags)) {
			currentTimer->nextEvent = gba->cpu->cycles + currentTimer->overflowInterval;
		} else {
			currentTimer->nextEvent = INT_MAX;
		}
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->reload;
		currentTimer->oldReload = currentTimer->reload;
		currentTimer->lastEvent = gba->cpu->cycles;
		gba->timersEnabled |= 1 << timer;
	} else if (wasEnabled && !GBATimerFlagsIsEnable(currentTimer->flags)) {
		if (!GBATimerFlagsIsCountUp(currentTimer->flags)) {
			gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->oldReload + ((gba->cpu->cycles - currentTimer->lastEvent) >> oldPrescale);
		}
		gba->timersEnabled &= ~(1 << timer);
	} else if (GBATimerFlagsGetPrescaleBits(currentTimer->flags) != oldPrescale && !GBATimerFlagsIsCountUp(currentTimer->flags)) {
		// FIXME: this might be before present
		currentTimer->nextEvent = currentTimer->lastEvent + currentTimer->overflowInterval;
	}

	if (currentTimer->nextEvent < gba->cpu->nextEvent) {
		gba->cpu->nextEvent = currentTimer->nextEvent;
	}
};

void GBAWriteIE(struct GBA* gba, uint16_t value) {
	if (value & (1 << IRQ_KEYPAD)) {
		mLOG(GBA, STUB, "Keypad interrupts not implemented");
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

	if (gba->memory.io[REG_IE >> 1] & 1 << irq) {
		gba->cpu->halted = 0;
		if (gba->memory.io[REG_IME >> 1]) {
			ARMRaiseIRQ(gba->cpu);
		}
	}
}

void GBATestIRQ(struct ARMCore* cpu) {
	struct GBA* gba = (struct GBA*) cpu->master;
	if (gba->memory.io[REG_IME >> 1] && gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1]) {
		gba->springIRQ = gba->memory.io[REG_IE >> 1] & gba->memory.io[REG_IF >> 1];
		gba->cpu->nextEvent = gba->cpu->cycles;
	}
}

void GBAHalt(struct GBA* gba) {
	gba->cpu->nextEvent = gba->cpu->cycles;
	gba->cpu->halted = 1;
}

void GBAStop(struct GBA* gba) {
	if (!gba->stopCallback) {
		return;
	}
	gba->cpu->nextEvent = gba->cpu->cycles;
	gba->stopCallback->stop(gba->stopCallback);
}

bool GBAIsROM(struct VFile* vf) {
	if (vf->seek(vf, GBA_ROM_MAGIC_OFFSET, SEEK_SET) < 0) {
		return false;
	}
	uint8_t signature[sizeof(GBA_ROM_MAGIC)];
	if (vf->read(vf, &signature, sizeof(signature)) != sizeof(signature)) {
		return false;
	}
	if (GBAIsBIOS(vf)) {
		return false;
	}
	return memcmp(signature, GBA_ROM_MAGIC, sizeof(signature)) == 0;
}

bool GBAIsMB(struct VFile* vf) {
	if (!GBAIsROM(vf)) {
		return false;
	}
	if (vf->size(vf) > SIZE_WORKING_RAM) {
		return false;
	}
	if (vf->seek(vf, GBA_MB_MAGIC_OFFSET, SEEK_SET) < 0) {
		return false;
	}
	uint32_t signature;
	if (vf->read(vf, &signature, sizeof(signature)) != sizeof(signature)) {
		return false;
	}
	uint32_t opcode;
	LOAD_32(opcode, 0, &signature);
	struct ARMInstructionInfo info;
	ARMDecodeARM(opcode, &info);
	if (info.branchType != ARM_BRANCH) {
		return false;
	}
	if (info.op1.immediate <= 0) {
		return false;
	} else if (info.op1.immediate == 28) {
		// Ancient toolchain that is known to throw MB detection for a loop
		return false;
	} else if (info.op1.immediate != 24) {
		return true;
	}
	// Found a libgba-linked cart...these are a bit harder to detect.
	return false;
}

bool GBAIsBIOS(struct VFile* vf) {
	if (vf->seek(vf, 0, SEEK_SET) < 0) {
		return false;
	}
	uint8_t interruptTable[7 * 4];
	if (vf->read(vf, &interruptTable, sizeof(interruptTable)) != sizeof(interruptTable)) {
		return false;
	}
	int i;
	for (i = 0; i < 7; ++i) {
		if (interruptTable[4 * i + 3] != 0xEA || interruptTable[4 * i + 2]) {
			return false;
		}
	}
	return true;
}

void GBAGetGameCode(struct GBA* gba, char* out) {
	memset(out, 0, 8);
	if (!gba->memory.rom) {
		return;
	}

	memcpy(out, "AGB-", 4);
	memcpy(&out[4], &((struct GBACartridge*) gba->memory.rom)->id, 4);
}

void GBAGetGameTitle(struct GBA* gba, char* out) {
	if (gba->memory.rom) {
		memcpy(out, &((struct GBACartridge*) gba->memory.rom)->title, 12);
		return;
	}
	if (gba->pristineRom) {
		memcpy(out, &((struct GBACartridge*) gba->pristineRom)->title, 12);
		return;
	}
	strncpy(out, "(BIOS)", 12);
}

void GBAHitStub(struct ARMCore* cpu, uint32_t opcode) {
	struct GBA* gba = (struct GBA*) cpu->master;
	if (gba->debugger) {
		struct mDebuggerEntryInfo info = {
			.address = _ARMPCAddress(cpu),
			.opcode = opcode
		};
		mDebuggerEnter(gba->debugger->d.p, DEBUGGER_ENTER_ILLEGAL_OP, &info);
	}
	// TODO: More sensible category?
	mLOG(GBA, ERROR, "Stub opcode: %08x", opcode);
}

void GBAIllegal(struct ARMCore* cpu, uint32_t opcode) {
	struct GBA* gba = (struct GBA*) cpu->master;
	if (!gba->yankedRomSize) {
		// TODO: More sensible category?
		mLOG(GBA, WARN, "Illegal opcode: %08x", opcode);
	}
	if (gba->debugger) {
		struct mDebuggerEntryInfo info = {
			.address = _ARMPCAddress(cpu),
			.opcode = opcode
		};
		mDebuggerEnter(gba->debugger->d.p, DEBUGGER_ENTER_ILLEGAL_OP, &info);
	} else {
		ARMRaiseUndefined(cpu);
	}
}

void GBABreakpoint(struct ARMCore* cpu, int immediate) {
	struct GBA* gba = (struct GBA*) cpu->master;
	if (immediate >= CPU_COMPONENT_MAX) {
		return;
	}
	switch (immediate) {
	case CPU_COMPONENT_DEBUGGER:
		if (gba->debugger) {
			struct mDebuggerEntryInfo info = {
				.address = _ARMPCAddress(cpu),
				.breakType = BREAKPOINT_SOFTWARE
			};
			mDebuggerEnter(gba->debugger->d.p, DEBUGGER_ENTER_BREAKPOINT, &info);
		}
		break;
	case CPU_COMPONENT_CHEAT_DEVICE:
		if (gba->cpu->components[CPU_COMPONENT_CHEAT_DEVICE]) {
			struct mCheatDevice* device = (struct mCheatDevice*) gba->cpu->components[CPU_COMPONENT_CHEAT_DEVICE];
			struct GBACheatHook* hook = 0;
			size_t i;
			for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
				struct GBACheatSet* cheats = (struct GBACheatSet*) *mCheatSetsGetPointer(&device->cheats, i);
				if (cheats->hook && cheats->hook->address == _ARMPCAddress(cpu)) {
					mCheatRefresh(device, &cheats->d);
					hook = cheats->hook;
				}
			}
			if (hook) {
				ARMRunFake(cpu, hook->patchedOpcode);
			}
		}
		break;
	default:
		break;
	}
}

void GBAFrameStarted(struct GBA* gba) {
	UNUSED(gba);

	struct mCoreThread* thread = mCoreThreadGet();
	mCoreThreadFrameStarted(thread);
}

void GBAFrameEnded(struct GBA* gba) {
	GBASavedataClean(&gba->memory.savedata, gba->video.frameCounter);

	if (gba->rr) {
		gba->rr->nextFrame(gba->rr);
	}

	if (gba->cpu->components && gba->cpu->components[CPU_COMPONENT_CHEAT_DEVICE]) {
		struct mCheatDevice* device = (struct mCheatDevice*) gba->cpu->components[CPU_COMPONENT_CHEAT_DEVICE];
		size_t i;
		for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
			struct GBACheatSet* cheats = (struct GBACheatSet*) *mCheatSetsGetPointer(&device->cheats, i);
			mCheatRefresh(device, &cheats->d);
		}
	}

	if (gba->stream && gba->stream->postVideoFrame) {
		const color_t* pixels;
		size_t stride;
		gba->video.renderer->getPixels(gba->video.renderer, &stride, (const void**) &pixels);
		gba->stream->postVideoFrame(gba->stream, pixels, stride);
	}

	if (gba->memory.hw.devices & (HW_GB_PLAYER | HW_GB_PLAYER_DETECTION)) {
		GBAHardwarePlayerUpdate(gba);
	}

	struct mCoreThread* thread = mCoreThreadGet();
	mCoreThreadFrameEnded(thread);

	// TODO: Put back RR
}

void GBASetBreakpoint(struct GBA* gba, struct mCPUComponent* component, uint32_t address, enum ExecutionMode mode, uint32_t* opcode) {
	size_t immediate;
	for (immediate = 0; immediate < gba->cpu->numComponents; ++immediate) {
		if (gba->cpu->components[immediate] == component) {
			break;
		}
	}
	if (immediate == gba->cpu->numComponents) {
		return;
	}
	if (mode == MODE_ARM) {
		int32_t value;
		int32_t old;
		value = 0xE1200070;
		value |= immediate & 0xF;
		value |= (immediate & 0xFFF0) << 4;
		GBAPatch32(gba->cpu, address, value, &old);
		*opcode = old;
	} else {
		int16_t value;
		int16_t old;
		value = 0xBE00;
		value |= immediate & 0xFF;
		GBAPatch16(gba->cpu, address, value, &old);
		*opcode = (uint16_t) old;
	}
}

void GBAClearBreakpoint(struct GBA* gba, uint32_t address, enum ExecutionMode mode, uint32_t opcode) {
	if (mode == MODE_ARM) {
		GBAPatch32(gba->cpu, address, opcode, 0);
	} else {
		GBAPatch16(gba->cpu, address, opcode, 0);
	}
}

static bool _setSoftwareBreakpoint(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode, uint32_t* opcode) {
	GBASetBreakpoint((struct GBA*) debugger->cpu->master, &debugger->d.p->d, address, mode, opcode);
	return true;
}

static bool _clearSoftwareBreakpoint(struct ARMDebugger* debugger, uint32_t address, enum ExecutionMode mode, uint32_t opcode) {
	GBAClearBreakpoint((struct GBA*) debugger->cpu->master, address, mode, opcode);
	return true;
}
