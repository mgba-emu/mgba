/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "memory.h"

#include "gb/gb.h"
#include "gb/io.h"

#include "util/memory.h"

mLOG_DEFINE_CATEGORY(GB_MBC);
mLOG_DEFINE_CATEGORY(GB_MEM);

static void _GBMBCNone(struct GBMemory* memory, uint16_t address, uint8_t value) {
	UNUSED(memory);
	UNUSED(address);
	UNUSED(value);

	mLOG(GB_MBC, GAME_ERROR, "Wrote to invalid MBC");
}

static void _GBMBC1(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC2(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC3(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC4(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC5(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC7(struct GBMemory*, uint16_t address, uint8_t value);

static void GBSetActiveRegion(struct LR35902Core* cpu, uint16_t address) {
	// TODO
}

static void _GBMemoryDMAService(struct GB* gb);


void GBMemoryInit(struct GB* gb) {
	struct LR35902Core* cpu = gb->cpu;
	cpu->memory.load8 = GBLoad8;
	cpu->memory.store8 = GBStore8;
	cpu->memory.setActiveRegion = GBSetActiveRegion;

	gb->memory.wram = 0;
	gb->memory.wramBank = 0;
	gb->memory.rom = 0;
	gb->memory.romBank = 0;
	gb->memory.romSize = 0;
	gb->memory.mbcType = GB_MBC_NONE;
	gb->memory.mbc = 0;

	gb->memory.dmaNext = INT_MAX;
	gb->memory.dmaRemaining = 0;

	memset(gb->memory.hram, 0, sizeof(gb->memory.hram));

	GBIOInit(gb);
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
	gb->memory.romBank = &gb->memory.rom[GB_SIZE_CART_BANK0];
	gb->memory.currentBank = 1;
	gb->memory.sramCurrentBank = 0;

	memset(&gb->video.oam, 0, sizeof(gb->video.oam));

	const struct GBCartridge* cart = &gb->memory.rom[0x100];
	switch (cart->type) {
	case 0:
	case 8:
	case 9:
		gb->memory.mbc = _GBMBCNone;
		gb->memory.mbcType = GB_MBC_NONE;
		break;
	case 1:
	case 2:
	case 3:
		gb->memory.mbc = _GBMBC1;
		gb->memory.mbcType = GB_MBC1;
		break;
	case 5:
	case 6:
		gb->memory.mbc = _GBMBC2;
		gb->memory.mbcType = GB_MBC2;
		break;
	case 0x0F:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
		gb->memory.mbc = _GBMBC3;
		gb->memory.mbcType = GB_MBC3;
		break;
	case 0x15:
	case 0x16:
	case 0x17:
		gb->memory.mbc = _GBMBC4;
		gb->memory.mbcType = GB_MBC4;
		break;
	default:
		mLOG(GB_MBC, WARN, "Unknown MBC type: %02X", cart->type);
	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
	case 0x1D:
	case 0x1E:
		gb->memory.mbc = _GBMBC5;
		gb->memory.mbcType = GB_MBC5;
		break;
	case 0x22:
		gb->memory.mbc = _GBMBC7;
		gb->memory.mbcType = GB_MBC7;
		break;
	}

	if (!gb->memory.wram) {
		GBMemoryDeinit(gb);
	}
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
		return gb->video.vram[address & (GB_SIZE_VRAM - 1)];
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->sramAccess) {
			return gb->memory.sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
		}
		return 0xFF;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		return memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
	case GB_REGION_WORKING_RAM_BANK1:
		return memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
	default:
		if (address < GB_BASE_OAM) {
			return memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
		}
		if (address < GB_BASE_UNUSABLE) {
			if (gb->video.mode < 2) {
				return gb->video.oam.raw[address & 0xFF];
			}
			return 0xFF;
		}
		if (address < GB_BASE_IO) {
			mLOG(GB_MEM, GAME_ERROR, "Attempt to read from unusable memory: %04X", address);
			return 0xFF;
		}
		if (address < GB_BASE_HRAM) {
			return GBIORead(gb, address & (GB_SIZE_IO - 1));
		}
		if (address < GB_BASE_IE) {
			return memory->hram[address & GB_SIZE_HRAM];
		}
		return GBIORead(gb, REG_IE);
	}
}

void GBStore8(struct LR35902Core* cpu, uint16_t address, int8_t value) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		memory->mbc(memory, address, value);
		return;
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		// TODO: Block access in wrong modes
		gb->video.vram[address & (GB_SIZE_VRAM - 1)] = value;
		gb->video.renderer->writeVRAM(gb->video.renderer, address & (GB_SIZE_VRAM - 1));
		return;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->sramAccess) {
			gb->memory.sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)] = value;
		}
		return;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		return;
	case GB_REGION_WORKING_RAM_BANK1:
		memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		return;
	default:
		if (address < GB_BASE_OAM) {
			memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		} else if (address < GB_BASE_UNUSABLE) {
			if (gb->video.mode < 2) {
				gb->video.oam.raw[address & 0xFF] = value;
				gb->video.renderer->writeOAM(gb->video.renderer, address & 0xFF);
			}
		} else if (address < GB_BASE_IO) {
			mLOG(GB_MEM, GAME_ERROR, "Attempt to write to unusable memory: %04X:%02X", address, value);
		} else if (address < GB_BASE_HRAM) {
			GBIOWrite(gb, address & (GB_SIZE_IO - 1), value);
		} else if (address < GB_BASE_IE) {
			memory->hram[address & GB_SIZE_HRAM] = value;
		} else {
			GBIOWrite(gb, REG_IE, value);
		}
	}
}

int32_t GBMemoryProcessEvents(struct GB* gb, int32_t cycles) {
	if (!gb->memory.dmaRemaining) {
		return INT_MAX;
	}
	gb->memory.dmaNext -= cycles;
	if (gb->memory.dmaNext <= 0) {
		_GBMemoryDMAService(gb);
	}
	return gb->memory.dmaNext;
}

void GBMemoryDMA(struct GB* gb, uint16_t base) {
	if (base > 0xF100) {
		return;
	}
	gb->cpu->memory.store8 = GBDMAStore8;
	gb->cpu->memory.load8 = GBDMALoad8;
	gb->memory.dmaNext = gb->cpu->cycles;
	if (gb->memory.dmaNext < gb->cpu->nextEvent) {
		gb->cpu->nextEvent = gb->memory.dmaNext;
	}
	gb->memory.dmaSource = base;
	gb->memory.dmaDest = GB_BASE_OAM;
	gb->memory.dmaRemaining = 0xA0;
}

void _GBMemoryDMAService(struct GB* gb) {
	uint8_t b = GBLoad8(gb->cpu, gb->memory.dmaSource);
	GBStore8(gb->cpu, gb->memory.dmaDest, b);
	++gb->memory.dmaSource;
	++gb->memory.dmaDest;
	--gb->memory.dmaRemaining;
	if (gb->memory.dmaRemaining) {
		gb->memory.dmaNext += 4;
	} else {
		gb->memory.dmaNext = INT_MAX;
		gb->cpu->memory.store8 = GBStore8;
		gb->cpu->memory.load8 = GBLoad8;
	}
}

uint8_t GBDMALoad8(struct LR35902Core* cpu, uint16_t address) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	if (address < 0xFF80 || address == 0xFFFF) {
		return 0xFF;
	}
	return memory->hram[address & GB_SIZE_HRAM];
}

void GBDMAStore8(struct LR35902Core* cpu, uint16_t address, int8_t value) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	if (address < 0xFF80 || address == 0xFFFF) {
		return;
	}
	memory->hram[address & GB_SIZE_HRAM] = value;
}

uint8_t GBView8(struct LR35902Core* cpu, uint16_t address);

void GBPatch8(struct LR35902Core* cpu, uint16_t address, int8_t value, int8_t* old);

static void _switchBank(struct GBMemory* memory, int bank) {
	size_t bankStart = bank * GB_SIZE_CART_BANK0;
	if (bankStart + GB_SIZE_CART_BANK0 > memory->romSize) {
		mLOG(GB_MBC, GAME_ERROR, "Attempting to switch to an invalid ROM bank: %0X", bank);
		return;
	}
	memory->romBank = &memory->rom[bankStart];
	memory->currentBank = bank;
}

static void _switchSramBank(struct GBMemory* memory, int bank) {
	size_t bankStart = bank * GB_SIZE_EXTERNAL_RAM;
	memory->sramBank = &memory->sram[bankStart];
	memory->sramCurrentBank = bank;
}

void _GBMBC1(struct GBMemory* memory, uint16_t address, uint8_t value) {
	int bank = value & 0x1F;
	switch (address >> 13) {
	case 0x0:
		switch (value) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			_switchSramBank(memory, memory->sramCurrentBank);
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MBC1 unknown value %02X", value);
			break;
		}
		break;
		break;
	case 0x1:
		if (!bank) {
			++bank;
		}
		_switchBank(memory, bank | (memory->currentBank & 0x60));
		break;
	}
}

void _GBMBC2(struct GBMemory* memory, uint16_t address, uint8_t value) {
	mLOG(GB_MBC, STUB, "MBC2 unimplemented");
}

void _GBMBC3(struct GBMemory* memory, uint16_t address, uint8_t value) {
	int bank = value & 0x7F;
	switch (address >> 13) {
	case 0x0:
		switch (value) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			_switchSramBank(memory, memory->sramCurrentBank);
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MBC3 unknown value %02X", value);
			break;
		}
		break;
	case 0x1:
		if (!bank) {
			++bank;
		}
		_switchBank(memory, bank);
		break;
	case 0x2:
		if (value < 4) {
			_switchSramBank(memory, value);
		}
		break;
	}
}

void _GBMBC4(struct GBMemory* memory, uint16_t address, uint8_t value) {
	// TODO
	mLOG(GB_MBC, STUB, "MBC4 unimplemented");
}

void _GBMBC5(struct GBMemory* memory, uint16_t address, uint8_t value) {
	int bank = value & 0x7F;
	switch (address >> 13) {
	case 0x0:
		switch (value) {
		case 0:
			memory->sramAccess = false;
			break;
		case 0xA:
			memory->sramAccess = true;
			_switchSramBank(memory, memory->sramCurrentBank);
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MBC5 unknown value %02X", value);
			break;
		}
		break;
		break;
	case 0x1:
		_switchBank(memory, bank);
		break;
	}
}

void _GBMBC7(struct GBMemory* memory, uint16_t address, uint8_t value) {
	// TODO
	mLOG(GB_MBC, STUB, "MBC7 unimplemented");
}
