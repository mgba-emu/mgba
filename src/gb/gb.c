/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb.h"

#include "gb/io.h"

#include "core/core.h"
#include "core/cheats.h"
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
static void GBStop(struct LR35902Core* cpu);

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
	gb->sync = NULL;

	GBInterruptHandlerInit(&gb->cpu->irqh);
	GBMemoryInit(gb);

	gb->video.p = gb;
	GBVideoInit(&gb->video);

	gb->audio.p = gb;
	GBAudioInit(&gb->audio, 2048, &gb->memory.io[REG_NR52], GB_AUDIO_DMG); // TODO: Remove magic constant

	gb->sio.p = gb;
	GBSIOInit(&gb->sio);

	gb->timer.p = gb;

	gb->biosVf = 0;
	gb->romVf = 0;
	gb->sramVf = 0;

	gb->pristineRom = 0;
	gb->pristineRomSize = 0;
	gb->yankedRomSize = 0;

	gb->stream = NULL;
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
	gb->memory.romBase = gb->memory.rom;
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
	}
	return gb->memory.sram;
}

void GBUnloadROM(struct GB* gb) {
	// TODO: Share with GBAUnloadROM
	if (gb->memory.rom && gb->memory.romBase != gb->memory.rom) {
		free(gb->memory.romBase);
	}
	if (gb->memory.rom && gb->pristineRom != gb->memory.rom) {
		if (gb->yankedRomSize) {
			gb->yankedRomSize = 0;
		}
		mappedMemoryFree(gb->memory.rom, GB_SIZE_CART_MAX);
	}
	gb->memory.rom = 0;

	if (gb->romVf) {
#ifndef _3DS
		gb->romVf->unmap(gb->romVf, gb->pristineRom, gb->pristineRomSize);
#endif
		gb->romVf->close(gb->romVf);
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

void GBLoadBIOS(struct GB* gb, struct VFile* vf) {
	gb->biosVf = vf;
}

void GBApplyPatch(struct GB* gb, struct Patch* patch) {
	size_t patchedSize = patch->outputSize(patch, gb->memory.romSize);
	if (!patchedSize) {
		return;
	}
	if (patchedSize > GB_SIZE_CART_MAX) {
		patchedSize = GB_SIZE_CART_MAX;
	}
	gb->memory.rom = anonymousMemoryMap(GB_SIZE_CART_MAX);
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
	GBVideoDeinit(&gb->video);
	GBSIODeinit(&gb->sio);
}

void GBInterruptHandlerInit(struct LR35902InterruptHandler* irqh) {
	irqh->reset = GBReset;
	irqh->processEvents = GBProcessEvents;
	irqh->setInterrupts = GBSetInterrupts;
	irqh->hitIllegal = GBIllegal;
	irqh->stop = GBStop;
	irqh->halt = GBHalt;
}

void GBReset(struct LR35902Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;

	if (gb->biosVf) {
		gb->biosVf->seek(gb->biosVf, 0, SEEK_SET);
		gb->memory.romBase = malloc(GB_SIZE_CART_BANK0);
		ssize_t size = gb->biosVf->read(gb->biosVf, gb->memory.romBase, GB_SIZE_CART_BANK0);
		uint32_t biosCrc = doCrc32(gb->memory.romBase, size);
		switch (biosCrc) {
		case 0x59C8598E:
			gb->model = GB_MODEL_DMG;
			gb->audio.style = GB_AUDIO_DMG;
			break;
		case 0x41884E46:
			gb->model = GB_MODEL_CGB;
			gb->audio.style = GB_AUDIO_CGB;
			break;
		default:
			free(gb->memory.romBase);
			gb->memory.romBase = gb->memory.rom;
			gb->biosVf = NULL;
			break;
		}

		memcpy(&gb->memory.romBase[size], &gb->memory.rom[size], GB_SIZE_CART_BANK0 - size);
		if (size > 0x100) {
			memcpy(&gb->memory.romBase[0x100], &gb->memory.rom[0x100], sizeof(struct GBCartridge));
		}

		cpu->a = 0;
		cpu->f.packed = 0;
		cpu->c = 0;
		cpu->e = 0;
		cpu->h = 0;
		cpu->l = 0;
		cpu->sp = 0;
		cpu->pc = 0;
	}
	if (!gb->biosVf) {
		const struct GBCartridge* cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
		if (cart->cgb & 0x80) {
			gb->model = GB_MODEL_CGB;
			gb->audio.style = GB_AUDIO_CGB;
			cpu->a = 0x11;
			cpu->f.packed = 0x80;
			cpu->c = 0;
			cpu->e = 0x08;
			cpu->h = 0;
			cpu->l = 0x7C;
		} else {
			// TODO: SGB
			gb->model = GB_MODEL_DMG;
			gb->audio.style = GB_AUDIO_DMG;
			cpu->a = 1;
			cpu->f.packed = 0xB0;
			cpu->c = 0x13;
			cpu->e = 0xD8;
			cpu->h = 1;
			cpu->l = 0x4D;
		}

		cpu->sp = 0xFFFE;
		cpu->pc = 0x100;
	}

	cpu->b = 0;
	cpu->d = 0;

	gb->eiPending = INT_MAX;
	gb->doubleSpeed = 0;

	cpu->memory.setActiveRegion(cpu, cpu->pc);

	if (gb->yankedRomSize) {
		gb->memory.romSize = gb->yankedRomSize;
		gb->yankedRomSize = 0;
	}
	GBMemoryReset(gb);
	GBVideoReset(&gb->video);
	GBTimerReset(&gb->timer);
	GBIOReset(gb);
	GBAudioReset(&gb->audio);
	GBSIOReset(&gb->sio);
}

void GBUpdateIRQs(struct GB* gb) {
	int irqs = gb->memory.ie & gb->memory.io[REG_IF];
	if (!irqs) {
		return;
	}
	gb->cpu->halted = false;

	if (!gb->memory.ime || gb->cpu->irqPending) {
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

		if (gb->eiPending != INT_MAX) {
			gb->eiPending -= cycles;
			if (gb->eiPending <= 0) {
				gb->memory.ime = true;
				GBUpdateIRQs(gb);
				gb->eiPending = INT_MAX;
			}
		}

		testEvent = GBVideoProcessEvents(&gb->video, cycles >> gb->doubleSpeed);
		if (testEvent != INT_MAX) {
			testEvent <<= gb->doubleSpeed;
			if (testEvent < nextEvent) {
				nextEvent = testEvent;
			}
		}

		testEvent = GBAudioProcessEvents(&gb->audio, cycles >> gb->doubleSpeed);
		if (testEvent != INT_MAX) {
			testEvent <<= gb->doubleSpeed;
			if (testEvent < nextEvent) {
				nextEvent = testEvent;
			}
		}

		testEvent = GBTimerProcessEvents(&gb->timer, cycles);
		if (testEvent < nextEvent) {
			nextEvent = testEvent;
		}

		testEvent = GBSIOProcessEvents(&gb->sio, cycles);
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
		gb->eiPending = INT_MAX;
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

void GBStop(struct LR35902Core* cpu) {
	struct GB* gb = (struct GB*) cpu->master;
	if (gb->memory.io[REG_KEY1] & 1) {
		gb->doubleSpeed ^= 1;
		gb->memory.io[REG_KEY1] &= 1;
		gb->memory.io[REG_KEY1] |= gb->doubleSpeed << 7;
	}
	// TODO: Actually stop
}

void GBIllegal(struct LR35902Core* cpu) {
	// TODO
	mLOG(GB, GAME_ERROR, "Hit illegal opcode at address %04X:%02X\n", cpu->pc, cpu->bus);
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
		cart = (const struct GBCartridge*) &((uint8_t*) gb->pristineRom)[0x100];
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

void GBGetGameCode(struct GB* gb, char* out) {
	memset(out, 0, 8);
	const struct GBCartridge* cart = NULL;
	if (gb->memory.rom) {
		cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
	}
	if (gb->pristineRom) {
		cart = (const struct GBCartridge*) &((uint8_t*) gb->pristineRom)[0x100];
	}
	if (!cart) {
		return;
	}
	if (cart->cgb == 0xC0) {
		memcpy(out, "CGB-????", 8);
	} else {
		memcpy(out, "DMG-????", 8);
	}
	if (cart->oldLicensee == 0x33) {
		memcpy(&out[4], cart->maker, 4);
	}
}

void GBFrameEnded(struct GB* gb) {
	if (gb->cpu->components && gb->cpu->components[CPU_COMPONENT_CHEAT_DEVICE]) {
		struct mCheatDevice* device = (struct mCheatDevice*) gb->cpu->components[CPU_COMPONENT_CHEAT_DEVICE];
		size_t i;
		for (i = 0; i < mCheatSetsSize(&device->cheats); ++i) {
			struct mCheatSet* cheats = *mCheatSetsGetPointer(&device->cheats, i);
			mCheatRefresh(device, cheats);
		}
	}
}
