/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb.h"

#include "gb/io.h"

#include "core/core.h"
#include "util/crc32.h"
#include "util/memory.h"
#include "util/math.h"
#include "util/patch.h"
#include "util/vfs.h"

const uint32_t CGB_LR35902_FREQUENCY = 0x800000;
const uint32_t SGB_LR35902_FREQUENCY = 0x418B1E;

const uint32_t GB_COMPONENT_MAGIC = 0x400000;

mLOG_DEFINE_CATEGORY(GB, "GB");

static void GBInit(void* cpu, struct mCPUComponent* component);
static void GBInterruptHandlerInit(struct LR35902InterruptHandler* irqh);
static void GBProcessEvents(struct LR35902Core* cpu);
static void GBSetInterrupts(struct LR35902Core* cpu, bool enable);
static void GBIllegal(struct LR35902Core* cpu);
static void GBHitStub(struct LR35902Core* cpu);

#ifdef _3DS
extern uint32_t* romBuffer;
extern size_t romBufferSize;
#endif

void GBCreate(struct GB* gb) {
	gb->d.id = GB_COMPONENT_MAGIC;
	gb->d.init = GBInit;
	gb->d.deinit = 0;
}

static void GBInit(void* cpu, struct mCPUComponent* component) {
	struct GB* gb = (struct GB*) component;
	gb->cpu = cpu;

	GBInterruptHandlerInit(&gb->cpu->irqh);
	GBMemoryInit(gb);

	gb->video.p = gb;
	GBVideoInit(&gb->video);

	gb->audio.p = gb;
	GBAudioInit(&gb->audio, 2048, &gb->memory.io[REG_NR52], GB_AUDIO_DMG); // TODO: Remove magic constant

	gb->timer.p = gb;

	gb->romVf = 0;
	gb->sramVf = 0;

	gb->pristineRom = 0;
	gb->pristineRomSize = 0;
	gb->yankedRomSize = 0;

	gb->eiPending = false;
}

bool GBLoadROM(struct GB* gb, struct VFile* vf) {
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
	gb->memory.romSize = gb->pristineRomSize;
	gb->romCrc32 = doCrc32(gb->memory.rom, gb->memory.romSize);

	// TODO: error check
	return true;
}

bool GBLoadSave(struct GB* gb, struct VFile* vf) {
	gb->sramVf = vf;
	if (vf) {
		// TODO: Do this in bank-switching code
		if (vf->size(vf) < 0x20000) {
			vf->truncate(vf, 0x20000);
		}
		gb->memory.sram = vf->map(vf, 0x20000, MAP_WRITE);
	} else {
		gb->memory.sram = anonymousMemoryMap(0x20000);
	}
	return gb->memory.sram;
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
		gb->sramVf = 0;
	} else if (gb->memory.sram) {
		mappedMemoryFree(gb->memory.sram, 0x8000);
	}
	gb->memory.sram = 0;
}

void GBApplyPatch(struct GB* gb, struct Patch* patch) {
	size_t patchedSize = patch->outputSize(patch, gb->memory.romSize);
	if (!patchedSize) {
		return;
	}
	if (patchedSize > 0x400000) {
		patchedSize = 0x400000;
	}
	gb->memory.rom = anonymousMemoryMap(0x400000);
	if (!patch->applyPatch(patch, gb->pristineRom, gb->pristineRomSize, gb->memory.rom, patchedSize)) {
		mappedMemoryFree(gb->memory.rom, patchedSize);
		gb->memory.rom = gb->pristineRom;
		return;
	}
	gb->memory.romSize = patchedSize;
	gb->romCrc32 = doCrc32(gb->memory.rom, gb->memory.romSize);
}

void GBDestroy(struct GB* gb) {
	GBUnloadROM(gb);

	GBMemoryDeinit(gb);
}

void GBInterruptHandlerInit(struct LR35902InterruptHandler* irqh) {
	irqh->reset = GBReset;
	irqh->processEvents = GBProcessEvents;
	irqh->setInterrupts = GBSetInterrupts;
	irqh->hitIllegal = GBIllegal;
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
	GBAudioReset(&gb->audio);
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

		if (gb->eiPending) {
			gb->eiPending -= cycles;
			if (gb->eiPending <= 0) {
				gb->memory.ime = true;
				GBUpdateIRQs(gb);
				gb->eiPending = 0;
			}
		}

		testEvent = GBVideoProcessEvents(&gb->video, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		testEvent = GBAudioProcessEvents(&gb->audio, cycles);
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
	if (!enable) {
		gb->memory.ime = enable;
		gb->eiPending = 0;
		GBUpdateIRQs(gb);
	} else {
		if (cpu->nextEvent > cpu->cycles + 4) {
			cpu->nextEvent = cpu->cycles + 4;
		}
		gb->eiPending = cpu->cycles + 4;
	}
}

void GBHalt(struct LR35902Core* cpu) {
	cpu->cycles = cpu->nextEvent;
	cpu->halted = true;
}

void GBIllegal(struct LR35902Core* cpu) {
	// TODO
	mLOG(GB, GAME_ERROR, "Hit illegal opcode at address %04X:%02X\n", cpu->pc, cpu->bus);
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

void GBGetGameTitle(struct GB* gb, char* out) {
	const struct GBCartridge* cart = NULL;
	if (gb->memory.rom) {
		cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
	}
	if (gb->pristineRom) {
		cart = (const struct GBCartridge*) &gb->pristineRom[0x100];
	}
	if (!cart) {
		return;
	}
	if (cart->oldLicensee != 0x33) {
		memcpy(out, cart->titleLong, 16);
	} else {
		memcpy(out, cart->titleShort, 11);
	}
}
