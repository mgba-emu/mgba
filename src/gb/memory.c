/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "memory.h"

#include "core/interface.h"
#include "gb/gb.h"
#include "gb/io.h"
#include "gb/mbc.h"
#include "gb/serialize.h"

#include "util/memory.h"

mLOG_DEFINE_CATEGORY(GB_MEM, "GB Memory");

static void _pristineCow(struct GB* gba);

static uint8_t GBFastLoad8(struct LR35902Core* cpu, uint16_t address) {
	if (UNLIKELY(address >= cpu->memory.activeRegionEnd)) {
		cpu->memory.setActiveRegion(cpu, address);
		return cpu->memory.cpuLoad8(cpu, address);
	}
	return cpu->memory.activeRegion[address & cpu->memory.activeMask];
}

static void GBSetActiveRegion(struct LR35902Core* cpu, uint16_t address) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		cpu->memory.cpuLoad8 = GBFastLoad8;
		cpu->memory.activeRegion = memory->romBase;
		cpu->memory.activeRegionEnd = GB_BASE_CART_BANK1;
		cpu->memory.activeMask = GB_SIZE_CART_BANK0 - 1;
		break;
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		cpu->memory.cpuLoad8 = GBFastLoad8;
		cpu->memory.activeRegion = memory->romBank;
		cpu->memory.activeRegionEnd = GB_BASE_VRAM;
		cpu->memory.activeMask = GB_SIZE_CART_BANK0 - 1;
		break;
	default:
		cpu->memory.cpuLoad8 = GBLoad8;
		break;
	}
}

static void _GBMemoryDMAService(struct GB* gb);
static void _GBMemoryHDMAService(struct GB* gb);

void GBMemoryInit(struct GB* gb) {
	struct LR35902Core* cpu = gb->cpu;
	cpu->memory.cpuLoad8 = GBLoad8;
	cpu->memory.load8 = GBLoad8;
	cpu->memory.store8 = GBStore8;
	cpu->memory.setActiveRegion = GBSetActiveRegion;

	gb->memory.wram = 0;
	gb->memory.wramBank = 0;
	gb->memory.rom = 0;
	gb->memory.romBank = 0;
	gb->memory.romSize = 0;
	gb->memory.sram = 0;
	gb->memory.mbcType = GB_MBC_AUTODETECT;
	gb->memory.mbc = 0;

	gb->memory.rtc = NULL;

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
	if (gb->model >= GB_MODEL_CGB) {
		uint32_t* base = (uint32_t*) gb->memory.wram;
		size_t i;
		uint32_t pattern = 0;
		for (i = 0; i < GB_SIZE_WORKING_RAM / 4; i += 4) {
			if ((i & 0x1FF) == 0) {
				pattern = ~pattern;
			}
			base[i + 0] = pattern;
			base[i + 1] = pattern;
			base[i + 2] = ~pattern;
			base[i + 3] = ~pattern;
		}
	}
	GBMemorySwitchWramBank(&gb->memory, 1);
	gb->memory.romBank = &gb->memory.rom[GB_SIZE_CART_BANK0];
	gb->memory.currentBank = 1;
	gb->memory.sramCurrentBank = 0;

	gb->memory.ime = false;
	gb->memory.ie = 0;

	gb->memory.dmaNext = INT_MAX;
	gb->memory.dmaRemaining = 0;
	gb->memory.dmaSource = 0;
	gb->memory.dmaDest = 0;
	gb->memory.hdmaNext = INT_MAX;
	gb->memory.hdmaRemaining = 0;
	gb->memory.hdmaSource = 0;
	gb->memory.hdmaDest = 0;
	gb->memory.isHdma = false;

	gb->memory.sramAccess = false;
	gb->memory.rtcAccess = false;
	gb->memory.activeRtcReg = 0;
	gb->memory.rtcLatched = false;
	memset(&gb->memory.rtcRegs, 0, sizeof(gb->memory.rtcRegs));

	memset(&gb->memory.hram, 0, sizeof(gb->memory.hram));
	memset(&gb->memory.mbcState, 0, sizeof(gb->memory.mbcState));

	GBMBCInit(gb);
	gb->memory.sramBank = gb->memory.sram;

	if (!gb->memory.wram) {
		GBMemoryDeinit(gb);
	}
}

void GBMemorySwitchWramBank(struct GBMemory* memory, int bank) {
	bank &= 7;
	if (!bank) {
		bank = 1;
	}
	memory->wramBank = &memory->wram[GB_SIZE_WORKING_RAM_BANK0 * bank];
	memory->wramCurrentBank = bank;
}

uint8_t GBLoad8(struct LR35902Core* cpu, uint16_t address) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		return memory->romBase[address & (GB_SIZE_CART_BANK0 - 1)];
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		return memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		return gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)];
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->rtcAccess) {
			return memory->rtcRegs[memory->activeRtcReg];
		} else if (memory->sramAccess) {
			return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
		} else if (memory->mbcType == GB_MBC7) {
			return GBMBC7Read(memory, address);
		} else if (memory->mbcType == GB_HuC3) {
			return 0x01; // TODO: Is this supposed to be the current SRAM bank?
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
		memory->mbc(gb, address, value);
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		return;
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		// TODO: Block access in wrong modes
		gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)] = value;
		return;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->rtcAccess) {
			memory->rtcRegs[memory->activeRtcReg] = value;
		} else if (memory->sramAccess) {
			memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)] = value;
		} else if (memory->mbcType == GB_MBC7) {
			GBMBC7Write(memory, address, value);
		}
		gb->sramDirty |= GB_SRAM_DIRT_NEW;
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
uint8_t GBView8(struct LR35902Core* cpu, uint16_t address, int segment) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		return memory->romBase[address & (GB_SIZE_CART_BANK0 - 1)];
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		if (segment < 0) {
			return memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
		} else if ((size_t) segment * GB_SIZE_CART_BANK0 < memory->romSize) {
			return memory->rom[(address & (GB_SIZE_CART_BANK0 - 1)) + segment * GB_SIZE_CART_BANK0];
		} else {
			return 0xFF;
		}
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		if (segment < 0) {
			return gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)];
		} else if (segment < 2) {
			return gb->video.vram[(address & (GB_SIZE_VRAM_BANK0 - 1)) + segment *GB_SIZE_VRAM_BANK0];
		} else {
			return 0xFF;
		}
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->rtcAccess) {
			return memory->rtcRegs[memory->activeRtcReg];
		} else if (memory->sramAccess) {
			if (segment < 0) {
				return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
			} else if ((size_t) segment * GB_SIZE_EXTERNAL_RAM < gb->sramSize) {
				return memory->sram[(address & (GB_SIZE_EXTERNAL_RAM - 1)) + segment *GB_SIZE_EXTERNAL_RAM];
			} else {
				return 0xFF;
			}
		} else if (memory->mbcType == GB_MBC7) {
			return GBMBC7Read(memory, address);
		} else if (memory->mbcType == GB_HuC3) {
			return 0x01; // TODO: Is this supposed to be the current SRAM bank?
		}
		return 0xFF;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		return memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
	case GB_REGION_WORKING_RAM_BANK1:
		if (segment < 0) {
			return memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
		} else if (segment < 8) {
			return memory->wram[(address & (GB_SIZE_WORKING_RAM_BANK0 - 1)) + segment *GB_SIZE_WORKING_RAM_BANK0];
		} else {
			return 0xFF;
		}
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

int32_t GBMemoryProcessEvents(struct GB* gb, int32_t cycles) {
	int nextEvent = INT_MAX;
	if (gb->memory.dmaRemaining) {
		gb->memory.dmaNext -= cycles;
		if (gb->memory.dmaNext <= 0) {
			_GBMemoryDMAService(gb);
		}
		nextEvent = gb->memory.dmaNext;
	}
	if (gb->memory.hdmaRemaining) {
		gb->memory.hdmaNext -= cycles;
		if (gb->memory.hdmaNext <= 0) {
			_GBMemoryHDMAService(gb);
		}
		if (gb->memory.hdmaNext < nextEvent) {
			nextEvent = gb->memory.hdmaNext;
		}
	}
	return nextEvent;
}

void GBMemoryDMA(struct GB* gb, uint16_t base) {
	if (base > 0xF100) {
		return;
	}
	gb->cpu->memory.store8 = GBDMAStore8;
	gb->cpu->memory.load8 = GBDMALoad8;
	gb->cpu->memory.cpuLoad8 = GBDMALoad8;
	gb->memory.dmaNext = gb->cpu->cycles + 8;
	if (gb->memory.dmaNext < gb->cpu->nextEvent) {
		gb->cpu->nextEvent = gb->memory.dmaNext;
	}
	gb->memory.dmaSource = base;
	gb->memory.dmaDest = 0;
	gb->memory.dmaRemaining = 0xA0;
}

void GBMemoryWriteHDMA5(struct GB* gb, uint8_t value) {
	gb->memory.hdmaSource = gb->memory.io[REG_HDMA1] << 8;
	gb->memory.hdmaSource |= gb->memory.io[REG_HDMA2];
	gb->memory.hdmaDest = gb->memory.io[REG_HDMA3] << 8;
	gb->memory.hdmaDest |= gb->memory.io[REG_HDMA4];
	gb->memory.hdmaSource &= 0xFFF0;
	if (gb->memory.hdmaSource >= 0x8000 && gb->memory.hdmaSource < 0xA000) {
		mLOG(GB_MEM, GAME_ERROR, "Invalid HDMA source: %04X", gb->memory.hdmaSource);
		return;
	}
	gb->memory.hdmaDest &= 0x1FF0;
	gb->memory.hdmaDest |= 0x8000;
	bool wasHdma = gb->memory.isHdma;
	gb->memory.isHdma = value & 0x80;
	if ((!wasHdma && !gb->memory.isHdma) || gb->video.mode == 0) {
		gb->memory.hdmaRemaining = ((value & 0x7F) + 1) * 0x10;
		gb->memory.hdmaNext = gb->cpu->cycles;
		gb->cpu->nextEvent = gb->cpu->cycles;
	}
}

void _GBMemoryDMAService(struct GB* gb) {
	uint8_t b = GBLoad8(gb->cpu, gb->memory.dmaSource);
	// TODO: Can DMA write OAM during modes 2-3?
	gb->video.oam.raw[gb->memory.dmaDest] = b;
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

void _GBMemoryHDMAService(struct GB* gb) {
	uint8_t b = gb->cpu->memory.load8(gb->cpu, gb->memory.hdmaSource);
	gb->cpu->memory.store8(gb->cpu, gb->memory.hdmaDest, b);
	++gb->memory.hdmaSource;
	++gb->memory.hdmaDest;
	--gb->memory.hdmaRemaining;
	gb->cpu->cycles += 2;
	if (gb->memory.hdmaRemaining) {
		gb->memory.hdmaNext += 2;
	} else {
		gb->memory.io[REG_HDMA1] = gb->memory.hdmaSource >> 8;
		gb->memory.io[REG_HDMA2] = gb->memory.hdmaSource;
		gb->memory.io[REG_HDMA3] = gb->memory.hdmaDest >> 8;
		gb->memory.io[REG_HDMA4] = gb->memory.hdmaDest;
		if (gb->memory.isHdma) {
			--gb->memory.io[REG_HDMA5];
			if (gb->memory.io[REG_HDMA5] == 0xFF) {
				gb->memory.isHdma = false;
			}
		} else {
			gb->memory.io[REG_HDMA5] = 0xFF;
		}
	}
}

struct OAMBlock {
	uint16_t low;
	uint16_t high;
};

static const struct OAMBlock _oamBlockDMG[] = {
	{ 0xA000, 0xFE00 },
	{ 0xA000, 0xFE00 },
	{ 0xA000, 0xFE00 },
	{ 0xA000, 0xFE00 },
	{ 0x8000, 0xA000 },
	{ 0xA000, 0xFE00 },
	{ 0xA000, 0xFE00 },
	{ 0xA000, 0xFE00 },
};

static const struct OAMBlock _oamBlockCGB[] = {
	{ 0xA000, 0xC000 },
	{ 0xA000, 0xC000 },
	{ 0xA000, 0xC000 },
	{ 0xA000, 0xC000 },
	{ 0x8000, 0xA000 },
	{ 0xA000, 0xC000 },
	{ 0xC000, 0xFE00 },
	{ 0xA000, 0xC000 },
};

uint8_t GBDMALoad8(struct LR35902Core* cpu, uint16_t address) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	const struct OAMBlock* block = gb->model < GB_MODEL_CGB ? _oamBlockDMG : _oamBlockCGB;
	block = &block[memory->dmaSource >> 13];
	if (address >= block->low && address < block->high) {
		return 0xFF;
	}
	if (address >= GB_BASE_OAM && address < GB_BASE_UNUSABLE) {
		return 0xFF;
	}
	return GBLoad8(cpu, address);
}

void GBDMAStore8(struct LR35902Core* cpu, uint16_t address, int8_t value) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	const struct OAMBlock* block = gb->model < GB_MODEL_CGB ? _oamBlockDMG : _oamBlockCGB;
	block = &block[memory->dmaSource >> 13];
	if (address >= block->low && address < block->high) {
		return;
	}
	if (address >= GB_BASE_OAM && address < GB_BASE_UNUSABLE) {
		return;
	}
	GBStore8(cpu, address, value);
}

void GBPatch8(struct LR35902Core* cpu, uint16_t address, int8_t value, int8_t* old, int segment) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	int8_t oldValue = -1;

	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		_pristineCow(gb);
		oldValue = memory->romBase[address & (GB_SIZE_CART_BANK0 - 1)];
		memory->romBase[address & (GB_SIZE_CART_BANK0 - 1)] =  value;
		break;
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		_pristineCow(gb);
		if (segment < 0) {
			oldValue = memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
			memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)] = value;
		} else if ((size_t) segment * GB_SIZE_CART_BANK0 < memory->romSize) {
			oldValue = memory->rom[(address & (GB_SIZE_CART_BANK0 - 1)) + segment * GB_SIZE_CART_BANK0];
			memory->rom[(address & (GB_SIZE_CART_BANK0 - 1)) + segment * GB_SIZE_CART_BANK0] = value;
		} else {
			return;
		}
		break;
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		if (segment < 0) {
			oldValue = gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)];
			gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)] = value;
		} else if (segment < 2) {
			oldValue = gb->video.vram[(address & (GB_SIZE_VRAM_BANK0 - 1)) + segment * GB_SIZE_VRAM_BANK0];
			gb->video.vramBank[(address & (GB_SIZE_VRAM_BANK0 - 1)) + segment * GB_SIZE_VRAM_BANK0] = value;
		} else {
			return;
		}
		break;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		mLOG(GB_MEM, STUB, "Unimplemented memory Patch8: 0x%08X", address);
		return;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		oldValue = memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
		memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		break;
	case GB_REGION_WORKING_RAM_BANK1:
		if (segment < 0) {
			oldValue = memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
			memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		} else if (segment < 8) {
			oldValue = memory->wram[(address & (GB_SIZE_WORKING_RAM_BANK0 - 1)) + segment * GB_SIZE_WORKING_RAM_BANK0];
			memory->wram[(address & (GB_SIZE_WORKING_RAM_BANK0 - 1)) + segment * GB_SIZE_WORKING_RAM_BANK0] = value;
		} else {
			return;
		}
		break;
	default:
		if (address < GB_BASE_OAM) {
			oldValue = memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
			memory->wramBank[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		} else if (address < GB_BASE_UNUSABLE) {
			oldValue = gb->video.oam.raw[address & 0xFF];
			gb->video.oam.raw[address & 0xFF] = value;
		} else if (address < GB_BASE_HRAM) {
			mLOG(GB_MEM, STUB, "Unimplemented memory Patch8: 0x%08X", address);
			return;
		} else if (address < GB_BASE_IE) {
			oldValue = memory->hram[address & GB_SIZE_HRAM];
			memory->hram[address & GB_SIZE_HRAM] = value;
		} else {
			mLOG(GB_MEM, STUB, "Unimplemented memory Patch8: 0x%08X", address);
			return;
		}
	}
	if (old) {
		*old = oldValue;
	}
}

void GBMemorySerialize(const struct GB* gb, struct GBSerializedState* state) {
	const struct GBMemory* memory = &gb->memory;
	memcpy(state->wram, memory->wram, GB_SIZE_WORKING_RAM);
	memcpy(state->hram, memory->hram, GB_SIZE_HRAM);
	STORE_16LE(memory->currentBank, 0, &state->memory.currentBank);
	state->memory.wramCurrentBank = memory->wramCurrentBank;
	state->memory.sramCurrentBank = memory->sramCurrentBank;

	STORE_32LE(memory->dmaNext, 0, &state->memory.dmaNext);
	STORE_16LE(memory->dmaSource, 0, &state->memory.dmaSource);
	STORE_16LE(memory->dmaDest, 0, &state->memory.dmaDest);

	STORE_32LE(memory->hdmaNext, 0, &state->memory.hdmaNext);
	STORE_16LE(memory->hdmaSource, 0, &state->memory.hdmaSource);
	STORE_16LE(memory->hdmaDest, 0, &state->memory.hdmaDest);

	STORE_16LE(memory->hdmaRemaining, 0, &state->memory.hdmaRemaining);
	state->memory.dmaRemaining = memory->dmaRemaining;
	memcpy(state->memory.rtcRegs, memory->rtcRegs, sizeof(state->memory.rtcRegs));

	GBSerializedMemoryFlags flags = 0;
	flags = GBSerializedMemoryFlagsSetSramAccess(flags, memory->sramAccess);
	flags = GBSerializedMemoryFlagsSetRtcAccess(flags, memory->rtcAccess);
	flags = GBSerializedMemoryFlagsSetRtcLatched(flags, memory->rtcLatched);
	flags = GBSerializedMemoryFlagsSetIme(flags, memory->ime);
	flags = GBSerializedMemoryFlagsSetIsHdma(flags, memory->isHdma);
	flags = GBSerializedMemoryFlagsSetActiveRtcReg(flags, memory->activeRtcReg);
	STORE_16LE(flags, 0, &state->memory.flags);
}

void GBMemoryDeserialize(struct GB* gb, const struct GBSerializedState* state) {
	struct GBMemory* memory = &gb->memory;
	memcpy(memory->wram, state->wram, GB_SIZE_WORKING_RAM);
	memcpy(memory->hram, state->hram, GB_SIZE_HRAM);
	LOAD_16LE(memory->currentBank, 0, &state->memory.currentBank);
	memory->wramCurrentBank = state->memory.wramCurrentBank;
	memory->sramCurrentBank = state->memory.sramCurrentBank;

	GBMBCSwitchBank(memory, memory->currentBank);
	GBMemorySwitchWramBank(memory, memory->wramCurrentBank);
	GBMBCSwitchSramBank(gb, memory->sramCurrentBank);

	LOAD_32LE(memory->dmaNext, 0, &state->memory.dmaNext);
	LOAD_16LE(memory->dmaSource, 0, &state->memory.dmaSource);
	LOAD_16LE(memory->dmaDest, 0, &state->memory.dmaDest);

	LOAD_32LE(memory->hdmaNext, 0, &state->memory.hdmaNext);
	LOAD_16LE(memory->hdmaSource, 0, &state->memory.hdmaSource);
	LOAD_16LE(memory->hdmaDest, 0, &state->memory.hdmaDest);

	LOAD_16LE(memory->hdmaRemaining, 0, &state->memory.hdmaRemaining);
	memory->dmaRemaining = state->memory.dmaRemaining;
	memcpy(memory->rtcRegs, state->memory.rtcRegs, sizeof(state->memory.rtcRegs));

	GBSerializedMemoryFlags flags;
	LOAD_16LE(flags, 0, &state->memory.flags);
	memory->sramAccess = GBSerializedMemoryFlagsGetSramAccess(flags);
	memory->rtcAccess = GBSerializedMemoryFlagsGetRtcAccess(flags);
	memory->rtcLatched = GBSerializedMemoryFlagsGetRtcLatched(flags);
	memory->ime = GBSerializedMemoryFlagsGetIme(flags);
	memory->isHdma = GBSerializedMemoryFlagsGetIsHdma(flags);
	memory->activeRtcReg = GBSerializedMemoryFlagsGetActiveRtcReg(flags);
}

void _pristineCow(struct GB* gb) {
	if (gb->memory.rom != gb->pristineRom) {
		return;
	}
	gb->memory.rom = anonymousMemoryMap(GB_SIZE_CART_MAX);
	memcpy(gb->memory.rom, gb->pristineRom, gb->memory.romSize);
	memset(((uint8_t*) gb->memory.rom) + gb->memory.romSize, 0xFF, GB_SIZE_CART_MAX - gb->memory.romSize);
	if (gb->pristineRom == gb->memory.romBase) {
		gb->memory.romBase = gb->memory.rom;
	}
	GBMBCSwitchBank(&gb->memory, gb->memory.currentBank);
}
