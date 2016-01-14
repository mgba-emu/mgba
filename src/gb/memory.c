/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "memory.h"

#include "gb/gb.h"

#include "util/memory.h"

static void GBSetActiveRegion(struct LR35902Core* cpu, uint16_t address) {
	// TODO
}

void GBMemoryInit(struct GB* gb) {
	struct LR35902Core* cpu = gb->cpu;
	cpu->memory.load16 = GBLoad16;
	cpu->memory.load8 = GBLoad8;
	cpu->memory.store16 = GBStore16;
	cpu->memory.store8 = GBStore8;
	cpu->memory.setActiveRegion = GBSetActiveRegion;

	gb->memory.wram = 0;
	gb->memory.wramBank = 0;
	gb->memory.rom = 0;
	gb->memory.romBank = 0;
	gb->memory.romSize = 0;
}

void GBMemoryDeinit(struct GB* gb) {
	mappedMemoryFree(gb->memory.wram, GB_SIZE_WORKING_RAM);
	if (gb->memory.rom) {
		mappedMemoryFree(gb->memory.rom, gb->memory.romSize);
	}
}

void GBMemoryReset(struct GB* gb) {
	if (gb->memory.wram) {
		mappedMemoryFree(gb->memory.wram, GB_SIZE_WORKING_RAM);
	}
	gb->memory.wram = anonymousMemoryMap(GB_SIZE_WORKING_RAM);
	gb->memory.wramBank = &gb->memory.wram[GB_SIZE_WORKING_RAM_BANK0];
	gb->memory.romBank = &gb->memory.rom[GB_BASE_CART_BANK0];

	if (!gb->memory.wram) {
		GBMemoryDeinit(gb);
	}
}

uint16_t GBLoad16(struct LR35902Core* cpu, uint16_t address) {
	// TODO
}

uint8_t GBLoad8(struct LR35902Core* cpu, uint16_t address) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		return memory->rom[address & (GB_SIZE_CART_BANK0 - 1)];
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		return memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		// TODO
		return 0;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		// TODO
		return 0;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		return memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
	case GB_REGION_WORKING_RAM_BANK1:
		return memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
	default:
		// TODO
		return 0;
	}
}

void GBStore16(struct LR35902Core* cpu, uint16_t address, int16_t value) {
	// TODO
}

void GBStore8(struct LR35902Core* cpu, uint16_t address, int8_t value) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		// TODO
		return;
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		// TODO
		return;
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		// TODO
		return;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		// TODO
		return;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		return;
	case GB_REGION_WORKING_RAM_BANK1:
		memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		return;
	default:
		// TODO
		return;
	}
}

uint16_t GBView16(struct LR35902Core* cpu, uint16_t address);
uint8_t GBView8(struct LR35902Core* cpu, uint16_t address);

void GBPatch16(struct LR35902Core* cpu, uint16_t address, int16_t value, int16_t* old);
void GBPatch8(struct LR35902Core* cpu, uint16_t address, int8_t value, int8_t* old);
