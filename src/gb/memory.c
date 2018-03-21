/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/memory.h>

#include <mgba/core/interface.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/mbc.h>
#include <mgba/internal/gb/serialize.h>
#include <mgba/internal/lr35902/lr35902.h>

#include <mgba-util/memory.h>

mLOG_DEFINE_CATEGORY(GB_MEM, "GB Memory", "gb.memory");

enum GBBus {
	GB_BUS_CPU,
	GB_BUS_MAIN,
	GB_BUS_VRAM,
	GB_BUS_RAM
};

static const enum GBBus _oamBlockDMG[] = {
	GB_BUS_MAIN, // 0x0000
	GB_BUS_MAIN, // 0x2000
	GB_BUS_MAIN, // 0x4000
	GB_BUS_MAIN, // 0x6000
	GB_BUS_VRAM, // 0x8000
	GB_BUS_MAIN, // 0xA000
	GB_BUS_MAIN, // 0xC000
	GB_BUS_CPU, // 0xE000
};

static const enum GBBus _oamBlockCGB[] = {
	GB_BUS_MAIN, // 0x0000
	GB_BUS_MAIN, // 0x2000
	GB_BUS_MAIN, // 0x4000
	GB_BUS_MAIN, // 0x6000
	GB_BUS_VRAM, // 0x8000
	GB_BUS_MAIN, // 0xA000
	GB_BUS_RAM, // 0xC000
	GB_BUS_CPU // 0xE000
};

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

static void _GBMemoryDMAService(struct mTiming* timing, void* context, uint32_t cyclesLate);
static void _GBMemoryHDMAService(struct mTiming* timing, void* context, uint32_t cyclesLate);

void GBMemoryInit(struct GB* gb) {
	struct LR35902Core* cpu = gb->cpu;
	cpu->memory.cpuLoad8 = GBLoad8;
	cpu->memory.load8 = GBLoad8;
	cpu->memory.store8 = GBStore8;
	cpu->memory.currentSegment = GBCurrentSegment;
	cpu->memory.setActiveRegion = GBSetActiveRegion;

	gb->memory.wram = 0;
	gb->memory.wramBank = 0;
	gb->memory.rom = 0;
	gb->memory.romBank = 0;
	gb->memory.romSize = 0;
	gb->memory.sram = 0;
	gb->memory.mbcType = GB_MBC_AUTODETECT;
	gb->memory.mbcRead = NULL;
	gb->memory.mbcWrite = NULL;

	gb->memory.rtc = NULL;
	gb->memory.rotation = NULL;
	gb->memory.rumble = NULL;

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

	gb->memory.dmaRemaining = 0;
	gb->memory.dmaSource = 0;
	gb->memory.dmaDest = 0;
	gb->memory.hdmaRemaining = 0;
	gb->memory.hdmaSource = 0;
	gb->memory.hdmaDest = 0;
	gb->memory.isHdma = false;


	gb->memory.dmaEvent.context = gb;
	gb->memory.dmaEvent.name = "GB DMA";
	gb->memory.dmaEvent.callback = _GBMemoryDMAService;
	gb->memory.dmaEvent.priority = 0x40;
	gb->memory.hdmaEvent.context = gb;
	gb->memory.hdmaEvent.name = "GB HDMA";
	gb->memory.hdmaEvent.callback = _GBMemoryHDMAService;
	gb->memory.hdmaEvent.priority = 0x41;

	memset(&gb->memory.hram, 0, sizeof(gb->memory.hram));
	switch (gb->memory.mbcType) {
	case GB_MBC1:
		gb->memory.mbcState.mbc1.mode = 0;
		break;
	default:
		memset(&gb->memory.mbcState, 0, sizeof(gb->memory.mbcState));
	}

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
	if (gb->memory.dmaRemaining) {
		const enum GBBus* block = gb->model < GB_MODEL_CGB ? _oamBlockDMG : _oamBlockCGB;
		enum GBBus dmaBus = block[memory->dmaSource >> 13];
		enum GBBus accessBus = block[address >> 13];
		if (dmaBus != GB_BUS_CPU && dmaBus == accessBus) {
			return 0xFF;
		}
		if (address >= GB_BASE_OAM && address < GB_BASE_UNUSABLE) {
			return 0xFF;
		}
	}
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
		} else if (memory->mbcRead) {
			return memory->mbcRead(memory, address);
		} else if (memory->sramAccess && memory->sram) {
			return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
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
	if (gb->memory.dmaRemaining) {
		const enum GBBus* block = gb->model < GB_MODEL_CGB ? _oamBlockDMG : _oamBlockCGB;
		enum GBBus dmaBus = block[memory->dmaSource >> 13];
		enum GBBus accessBus = block[address >> 13];
		if (dmaBus != GB_BUS_CPU && dmaBus == accessBus) {
			return;
		}
		if (address >= GB_BASE_OAM && address < GB_BASE_UNUSABLE) {
			return;
		}
	}
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		memory->mbcWrite(gb, address, value);
		cpu->memory.setActiveRegion(cpu, cpu->pc);
		return;
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		gb->video.renderer->writeVRAM(gb->video.renderer, (address & (GB_SIZE_VRAM_BANK0 - 1)) | (GB_SIZE_VRAM_BANK0 * gb->video.vramCurrentBank));
		gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)] = value;
		return;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->rtcAccess) {
			memory->rtcRegs[memory->activeRtcReg] = value;
		} else if (memory->sramAccess && memory->sram && memory->mbcType != GB_MBC2) {
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

int GBCurrentSegment(struct LR35902Core* cpu, uint16_t address) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		return 0;
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		return memory->currentBank;
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		return gb->video.vramCurrentBank;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		return memory->sramCurrentBank;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		return 0;
	case GB_REGION_WORKING_RAM_BANK1:
		return memory->wramCurrentBank;
	default:
		return 0;
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
			if (segment < 0 && memory->sram) {
				return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
			} else if ((size_t) segment * GB_SIZE_EXTERNAL_RAM < gb->sramSize) {
				return memory->sram[(address & (GB_SIZE_EXTERNAL_RAM - 1)) + segment *GB_SIZE_EXTERNAL_RAM];
			} else {
				return 0xFF;
			}
		} else if (memory->mbcRead) {
			return memory->mbcRead(memory, address);
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

void GBMemoryDMA(struct GB* gb, uint16_t base) {
	if (base > 0xF100) {
		return;
	}
	mTimingDeschedule(&gb->timing, &gb->memory.dmaEvent);
	mTimingSchedule(&gb->timing, &gb->memory.dmaEvent, 8);
	if (gb->cpu->cycles + 8 < gb->cpu->nextEvent) {
		gb->cpu->nextEvent = gb->cpu->cycles + 8;
	}
	gb->memory.dmaSource = base;
	gb->memory.dmaDest = 0;
	gb->memory.dmaRemaining = 0xA0;
}

uint8_t GBMemoryWriteHDMA5(struct GB* gb, uint8_t value) {
	gb->memory.hdmaSource = gb->memory.io[REG_HDMA1] << 8;
	gb->memory.hdmaSource |= gb->memory.io[REG_HDMA2];
	gb->memory.hdmaDest = gb->memory.io[REG_HDMA3] << 8;
	gb->memory.hdmaDest |= gb->memory.io[REG_HDMA4];
	gb->memory.hdmaSource &= 0xFFF0;
	if (gb->memory.hdmaSource >= 0x8000 && gb->memory.hdmaSource < 0xA000) {
		mLOG(GB_MEM, GAME_ERROR, "Invalid HDMA source: %04X", gb->memory.hdmaSource);
		return value | 0x80;
	}
	gb->memory.hdmaDest &= 0x1FF0;
	gb->memory.hdmaDest |= 0x8000;
	bool wasHdma = gb->memory.isHdma;
	gb->memory.isHdma = value & 0x80;
	if ((!wasHdma && !gb->memory.isHdma) || gb->video.mode == 0) {
		if (gb->memory.isHdma) {
			gb->memory.hdmaRemaining = 0x10;
		} else {
			gb->memory.hdmaRemaining = ((value & 0x7F) + 1) * 0x10;
		}
		gb->cpuBlocked = true;
		mTimingSchedule(&gb->timing, &gb->memory.hdmaEvent, 0);
	} else if (gb->memory.isHdma && !GBRegisterLCDCIsEnable(gb->memory.io[REG_LCDC])) {
		return 0x80 | ((value + 1) & 0x7F);
	}
	return value & 0x7F;
}

void _GBMemoryDMAService(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GB* gb = context;
	int dmaRemaining = gb->memory.dmaRemaining;
	gb->memory.dmaRemaining = 0;
	uint8_t b = GBLoad8(gb->cpu, gb->memory.dmaSource);
	// TODO: Can DMA write OAM during modes 2-3?
	gb->video.oam.raw[gb->memory.dmaDest] = b;
	gb->video.renderer->writeOAM(gb->video.renderer, gb->memory.dmaDest);
	++gb->memory.dmaSource;
	++gb->memory.dmaDest;
	gb->memory.dmaRemaining = dmaRemaining - 1;
	if (gb->memory.dmaRemaining) {
		mTimingSchedule(timing, &gb->memory.dmaEvent, 4 - cyclesLate);
	}
}

void _GBMemoryHDMAService(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GB* gb = context;
	gb->cpuBlocked = true;
	uint8_t b = gb->cpu->memory.load8(gb->cpu, gb->memory.hdmaSource);
	gb->cpu->memory.store8(gb->cpu, gb->memory.hdmaDest, b);
	++gb->memory.hdmaSource;
	++gb->memory.hdmaDest;
	--gb->memory.hdmaRemaining;
	if (gb->memory.hdmaRemaining) {
		mTimingDeschedule(timing, &gb->memory.hdmaEvent);
		mTimingSchedule(timing, &gb->memory.hdmaEvent, 2 - cyclesLate);
	} else {
		gb->cpuBlocked = false;
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
			gb->video.renderer->writeVRAM(gb->video.renderer, (address & (GB_SIZE_VRAM_BANK0 - 1)) + GB_SIZE_VRAM_BANK0 * gb->video.vramCurrentBank);
		} else if (segment < 2) {
			oldValue = gb->video.vram[(address & (GB_SIZE_VRAM_BANK0 - 1)) + segment * GB_SIZE_VRAM_BANK0];
			gb->video.vramBank[(address & (GB_SIZE_VRAM_BANK0 - 1)) + segment * GB_SIZE_VRAM_BANK0] = value;
			gb->video.renderer->writeVRAM(gb->video.renderer, (address & (GB_SIZE_VRAM_BANK0 - 1)) + segment * GB_SIZE_VRAM_BANK0);
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
			gb->video.renderer->writeOAM(gb->video.renderer, address & 0xFF);
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

	STORE_16LE(memory->dmaSource, 0, &state->memory.dmaSource);
	STORE_16LE(memory->dmaDest, 0, &state->memory.dmaDest);

	STORE_16LE(memory->hdmaSource, 0, &state->memory.hdmaSource);
	STORE_16LE(memory->hdmaDest, 0, &state->memory.hdmaDest);

	STORE_16LE(memory->hdmaRemaining, 0, &state->memory.hdmaRemaining);
	state->memory.dmaRemaining = memory->dmaRemaining;
	memcpy(state->memory.rtcRegs, memory->rtcRegs, sizeof(state->memory.rtcRegs));

	STORE_32LE(memory->dmaEvent.when - mTimingCurrentTime(&gb->timing), 0, &state->memory.dmaNext);
	STORE_32LE(memory->hdmaEvent.when - mTimingCurrentTime(&gb->timing), 0, &state->memory.hdmaNext);

	GBSerializedMemoryFlags flags = 0;
	flags = GBSerializedMemoryFlagsSetSramAccess(flags, memory->sramAccess);
	flags = GBSerializedMemoryFlagsSetRtcAccess(flags, memory->rtcAccess);
	flags = GBSerializedMemoryFlagsSetRtcLatched(flags, memory->rtcLatched);
	flags = GBSerializedMemoryFlagsSetIme(flags, memory->ime);
	flags = GBSerializedMemoryFlagsSetIsHdma(flags, memory->isHdma);
	flags = GBSerializedMemoryFlagsSetActiveRtcReg(flags, memory->activeRtcReg);
	STORE_16LE(flags, 0, &state->memory.flags);

	switch (memory->mbcType) {
	case GB_MBC1:
		state->memory.mbc1.mode = memory->mbcState.mbc1.mode;
		state->memory.mbc1.multicartStride = memory->mbcState.mbc1.multicartStride;
		break;
	case GB_MBC3_RTC:
		STORE_64LE(gb->memory.rtcLastLatch, 0, &state->memory.rtc.lastLatch);
		break;
	case GB_MBC7:
		state->memory.mbc7.state = memory->mbcState.mbc7.state;
		state->memory.mbc7.eeprom = memory->mbcState.mbc7.eeprom;
		state->memory.mbc7.address = memory->mbcState.mbc7.address;
		state->memory.mbc7.access = memory->mbcState.mbc7.access;
		state->memory.mbc7.latch = memory->mbcState.mbc7.latch;
		state->memory.mbc7.srBits = memory->mbcState.mbc7.srBits;
		STORE_16LE(memory->mbcState.mbc7.sr, 0, &state->memory.mbc7.sr);
		STORE_32LE(memory->mbcState.mbc7.writable, 0, &state->memory.mbc7.writable);
		break;
	default:
		break;
	}
}

void GBMemoryDeserialize(struct GB* gb, const struct GBSerializedState* state) {
	struct GBMemory* memory = &gb->memory;
	memcpy(memory->wram, state->wram, GB_SIZE_WORKING_RAM);
	memcpy(memory->hram, state->hram, GB_SIZE_HRAM);
	LOAD_16LE(memory->currentBank, 0, &state->memory.currentBank);
	memory->wramCurrentBank = state->memory.wramCurrentBank;
	memory->sramCurrentBank = state->memory.sramCurrentBank;

	GBMBCSwitchBank(gb, memory->currentBank);
	GBMemorySwitchWramBank(memory, memory->wramCurrentBank);
	GBMBCSwitchSramBank(gb, memory->sramCurrentBank);

	LOAD_16LE(memory->dmaSource, 0, &state->memory.dmaSource);
	LOAD_16LE(memory->dmaDest, 0, &state->memory.dmaDest);

	LOAD_16LE(memory->hdmaSource, 0, &state->memory.hdmaSource);
	LOAD_16LE(memory->hdmaDest, 0, &state->memory.hdmaDest);

	LOAD_16LE(memory->hdmaRemaining, 0, &state->memory.hdmaRemaining);
	memory->dmaRemaining = state->memory.dmaRemaining;
	memcpy(memory->rtcRegs, state->memory.rtcRegs, sizeof(state->memory.rtcRegs));

	uint32_t when;
	LOAD_32LE(when, 0, &state->memory.dmaNext);
	if (memory->dmaRemaining) {
		mTimingSchedule(&gb->timing, &memory->dmaEvent, when);
	}
	LOAD_32LE(when, 0, &state->memory.hdmaNext);
	if (memory->hdmaRemaining) {
		mTimingSchedule(&gb->timing, &memory->hdmaEvent, when);
	}

	GBSerializedMemoryFlags flags;
	LOAD_16LE(flags, 0, &state->memory.flags);
	memory->sramAccess = GBSerializedMemoryFlagsGetSramAccess(flags);
	memory->rtcAccess = GBSerializedMemoryFlagsGetRtcAccess(flags);
	memory->rtcLatched = GBSerializedMemoryFlagsGetRtcLatched(flags);
	memory->ime = GBSerializedMemoryFlagsGetIme(flags);
	memory->isHdma = GBSerializedMemoryFlagsGetIsHdma(flags);
	memory->activeRtcReg = GBSerializedMemoryFlagsGetActiveRtcReg(flags);

	switch (memory->mbcType) {
	case GB_MBC1:
		memory->mbcState.mbc1.mode = state->memory.mbc1.mode;
		memory->mbcState.mbc1.multicartStride = state->memory.mbc1.multicartStride;
		if (memory->mbcState.mbc1.mode) {
			GBMBCSwitchBank0(gb, memory->currentBank >> memory->mbcState.mbc1.multicartStride);
		}
		break;
	case GB_MBC3_RTC:
		LOAD_64LE(gb->memory.rtcLastLatch, 0, &state->memory.rtc.lastLatch);
		break;
	case GB_MBC7:
		memory->mbcState.mbc7.state = state->memory.mbc7.state;
		memory->mbcState.mbc7.eeprom = state->memory.mbc7.eeprom;
		memory->mbcState.mbc7.address = state->memory.mbc7.address & 0x7F;
		memory->mbcState.mbc7.access = state->memory.mbc7.access;
		memory->mbcState.mbc7.latch = state->memory.mbc7.latch;
		memory->mbcState.mbc7.srBits = state->memory.mbc7.srBits;
		LOAD_16LE(memory->mbcState.mbc7.sr, 0, &state->memory.mbc7.sr);
		LOAD_32LE(memory->mbcState.mbc7.writable, 0, &state->memory.mbc7.writable);
		break;
	default:
		break;
	}
}

void _pristineCow(struct GB* gb) {
	if (!gb->isPristine) {
		return;
	}
	void* newRom = anonymousMemoryMap(GB_SIZE_CART_MAX);
	memcpy(newRom, gb->memory.rom, gb->memory.romSize);
	memset(((uint8_t*) newRom) + gb->memory.romSize, 0xFF, GB_SIZE_CART_MAX - gb->memory.romSize);
	if (gb->memory.rom == gb->memory.romBase) {
		gb->memory.romBase = newRom;
	}
	gb->memory.rom = newRom;
	GBMBCSwitchBank(gb, gb->memory.currentBank);
	gb->isPristine = false;
}
