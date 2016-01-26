/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb.h"

#include "gb/io.h"

#include "util/crc32.h"
#include "util/memory.h"
#include "util/math.h"
#include "util/patch.h"
#include "util/vfs.h"

const uint32_t DMG_LR35902_FREQUENCY = 0x400000;
const uint32_t CGB_LR35902_FREQUENCY = 0x800000;
const uint32_t SGB_LR35902_FREQUENCY = 0x418B1E;

const uint32_t GB_COMPONENT_MAGIC = 0x400000;

mLOG_DEFINE_CATEGORY(GB);

static void GBInit(struct LR35902Core* cpu, struct LR35902Component* component);
static void GBInterruptHandlerInit(struct LR35902InterruptHandler* irqh);
static void GBProcessEvents(struct LR35902Core* cpu);
static void GBSetInterrupts(struct LR35902Core* cpu, bool enable);
static void GBHitStub(struct LR35902Core* cpu);

void GBCreate(struct GB* gb) {
	gb->d.id = GB_COMPONENT_MAGIC;
	gb->d.init = GBInit;
	gb->d.deinit = 0;
}

static void GBInit(struct LR35902Core* cpu, struct LR35902Component* component) {
	struct GB* gb = (struct GB*) component;
	gb->cpu = cpu;

	GBInterruptHandlerInit(&cpu->irqh);
	GBMemoryInit(gb);

	gb->video.p = gb;
	GBVideoInit(&gb->video);

	gb->timer.p = gb;

	gb->romVf = 0;
	gb->sramVf = 0;

	gb->pristineRom = 0;
	gb->pristineRomSize = 0;
	gb->yankedRomSize = 0;
}

bool GBLoadROM(struct GB* gb, struct VFile* vf, struct VFile* sav, const char* fname) {
	GBUnloadROM(gb);
	gb->romVf = vf;
	gb->pristineRomSize = vf->size(vf);
	vf->seek(vf, 0, SEEK_SET);
#ifdef _3DS
	gb->pristineRom = 0;
	if (gb->pristineRomSize <= romBufferSize) {
		gb->pristineRom = romBuffer;
		vf->read(vf, romBuffer, gb->pristineRomSize);
	}
#else
	gb->pristineRom = vf->map(vf, gb->pristineRomSize, MAP_READ);
#endif
	if (!gb->pristineRom) {
		return false;
	}
	gb->yankedRomSize = 0;
	gb->memory.rom = gb->pristineRom;
	gb->activeFile = fname;
	gb->memory.romSize = gb->pristineRomSize;
	gb->romCrc32 = doCrc32(gb->memory.rom, gb->memory.romSize);
	gb->sramVf = sav;
	if (sav) {
		if (sav->size(sav) < 0x8000) {
			sav->truncate(sav, 0x8000);
		}
		gb->memory.sram = sav->map(sav, 0x8000, MAP_WRITE);
	} else {
		gb->memory.sram = anonymousMemoryMap(0x8000);
	}
	return true;
	// TODO: error check
}

void GBUnloadROM(struct GB* gb) {
	// TODO: Share with GBAUnloadROM
	if (gb->memory.rom && gb->pristineRom != gb->memory.rom) {
		if (gb->yankedRomSize) {
			gb->yankedRomSize = 0;
		}
		mappedMemoryFree(gb->memory.rom, 0x400000);
	}
	gb->memory.rom = 0;

	if (gb->romVf) {
#ifndef _3DS
		gb->romVf->unmap(gb->romVf, gb->pristineRom, gb->pristineRomSize);
#endif
		gb->pristineRom = 0;
		gb->romVf = 0;
	}

	if (gb->sramVf) {
		gb->sramVf->unmap(gb->sramVf, gb->memory.sram, 0x8000);
	} else if (gb->memory.sram) {
		mappedMemoryFree(gb->memory.sram, 0x8000);
	}
	gb->memory.sram = 0;
}

void GBDestroy(struct GB* gb) {
	GBUnloadROM(gb);

	GBMemoryDeinit(gb);
}

void GBInterruptHandlerInit(struct LR35902InterruptHandler* irqh) {
	irqh->reset = GBReset;
	irqh->processEvents = GBProcessEvents;
	irqh->setInterrupts = GBSetInterrupts;
	irqh->hitStub = GBHitStub;
	irqh->halt = GBHalt;
}

void GBReset(struct LR35902Core* cpu) {
	cpu->a = 1;
	cpu->f.packed = 0xB0;
	cpu->b = 0;
	cpu->c = 0x13;
	cpu->d = 0;
	cpu->e = 0xD8;
	cpu->h = 1;
	cpu->l = 0x4D;
	cpu->sp = 0xFFFE;
	cpu->pc = 0x100;

	struct GB* gb = (struct GB*) cpu->master;

	if (gb->yankedRomSize) {
		gb->memory.romSize = gb->yankedRomSize;
		gb->yankedRomSize = 0;
	}
	GBMemoryReset(gb);
	GBVideoReset(&gb->video);
	GBTimerReset(&gb->timer);
	GBIOReset(gb);
}

void GBUpdateIRQs(struct GB* gb) {
	int irqs = gb->memory.ie & gb->memory.io[REG_IF];
	if (!irqs) {
		return;
	}
	gb->cpu->halted = false;

	if (!gb->memory.ime) {
		return;
	}

	if (irqs & (1 << GB_IRQ_VBLANK)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_VBLANK);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_VBLANK);
		return;
	}
	if (irqs & (1 << GB_IRQ_LCDSTAT)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_LCDSTAT);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_LCDSTAT);
		return;
	}
	if (irqs & (1 << GB_IRQ_TIMER)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_TIMER);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_TIMER);
		return;
	}
	if (irqs & (1 << GB_IRQ_SIO)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_SIO);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_SIO);
		return;
	}
	if (irqs & (1 << GB_IRQ_KEYPAD)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_KEYPAD);
		gb->memory.io[REG_IF] &= ~(1 << GB_IRQ_KEYPAD);
	}
}

void GBProcessEvents(struct LR35902Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	do {
		int32_t cycles = cpu->nextEvent;
		int32_t nextEvent = INT_MAX;
		int32_t testEvent;

		testEvent = GBVideoProcessEvents(&gb->video, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		testEvent = GBTimerProcessEvents(&gb->timer, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		testEvent = GBMemoryProcessEvents(gb, cycles);
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

void GBSetInterrupts(struct LR35902Core* cpu, bool enable) {
	struct GB* gb = (struct GB*) cpu->master;
	gb->memory.ime = enable;
	GBUpdateIRQs(gb);
}

void GBHalt(struct LR35902Core* cpu) {
	cpu->cycles = cpu->nextEvent;
	cpu->halted = true;
}

void GBHitStub(struct LR35902Core* cpu) {
	// TODO
	mLOG(GB, STUB, "Hit stub at address %04X:%02X\n", cpu->pc, cpu->bus);
}

bool GBIsROM(struct VFile* vf) {
	vf->seek(vf, 0x104, SEEK_SET);
	uint8_t header[4];
	static const uint8_t knownHeader[4] = { 0xCE, 0xED, 0x66, 0x66};

	if (vf->read(vf, &header, sizeof(header)) < (ssize_t) sizeof(header)) {
		return false;
	}
	if (memcmp(header, knownHeader, sizeof(header))) {
		return false;
	}
	return true;
}
