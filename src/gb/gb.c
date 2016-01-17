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

	gb->romVf = 0;

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
	GBIOReset(gb);
}

void GBUpdateIRQs(struct GB* gb) {
	if (!gb->memory.ime) {
		return;
	}
	int irqs = gb->memory.ie & gb->memory.io[REG_IF];
	if (!irqs) {
		return;
	}
	if (irqs & (1 << GB_IRQ_VBLANK)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_VBLANK);
		return;
	}
	if (irqs & (1 << GB_IRQ_LCDSTAT)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_LCDSTAT);
		return;
	}
	if (irqs & (1 << GB_IRQ_TIMER)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_TIMER);
		return;
	}
	if (irqs & (1 << GB_IRQ_SIO)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_SIO);
		return;
	}
	if (irqs & (1 << GB_IRQ_KEYPAD)) {
		LR35902RaiseIRQ(gb->cpu, GB_VECTOR_KEYPAD);
	}
}

void GBProcessEvents(struct LR35902Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	int32_t cycles = cpu->nextEvent;
	int32_t nextEvent = INT_MAX;
	int32_t testEvent;

	testEvent = GBVideoProcessEvents(&gb->video, cycles);
	if (testEvent < nextEvent) {
		nextEvent = testEvent;
	}

	cpu->cycles -= cycles;
	cpu->nextEvent = nextEvent;
}

void GBSetInterrupts(struct LR35902Core* cpu, bool enable) {
	struct GB* gb = (struct GB*) cpu->master;
	gb->memory.ime = enable;
	GBUpdateIRQs(gb);
}

void GBHitStub(struct LR35902Core* cpu) {
	// TODO
	//printf("Hit stub at address %04X\n", cpu->pc);
}
