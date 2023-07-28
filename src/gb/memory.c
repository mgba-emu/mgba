/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/memory.h>

#include <mgba/core/interface.h>
#include <mgba/internal/defines.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/mbc.h>
#include <mgba/internal/gb/serialize.h>
#include <mgba/internal/sm83/sm83.h>

#include <mgba-util/memory.h>
#include <mgba-util/vfs.h>

mLOG_DEFINE_CATEGORY(GB_MEM, "GB Memory", "gb.memory");

static const uint8_t _yankBuffer[] = { 0xFF };

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

static const uint8_t _blockedRegion[1] = { 0xFF };

static void _pristineCow(struct GB* gba);

static uint8_t GBCartLoad8(struct SM83Core* cpu, uint16_t address) {
	if (UNLIKELY(address >= cpu->memory.activeRegionEnd)) {
		cpu->memory.setActiveRegion(cpu, address);
		return cpu->memory.cpuLoad8(cpu, address);
	}
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	memory->cartBusPc = address;
	uint8_t value = cpu->memory.activeRegion[address & cpu->memory.activeMask];
	memory->cartBus = value;
	return value;
}

static void GBSetActiveRegion(struct SM83Core* cpu, uint16_t address) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		if (gb->memory.mbcReadBank0) {
			cpu->memory.cpuLoad8 = GBLoad8;
			break;
		}
		cpu->memory.cpuLoad8 = GBCartLoad8;
		cpu->memory.activeRegion = memory->romBase;
		cpu->memory.activeRegionEnd = GB_BASE_CART_BANK1;
		cpu->memory.activeMask = GB_SIZE_CART_BANK0 - 1;
		if (gb->memory.romSize < GB_SIZE_CART_BANK0) {
			if (address >= gb->memory.romSize) {
				cpu->memory.activeRegion = _yankBuffer;
				cpu->memory.activeMask = 0;
			} else {
				cpu->memory.activeRegionEnd = gb->memory.romSize;
			}
		}
		break;
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		if (gb->memory.mbcReadBank1) {
			cpu->memory.cpuLoad8 = GBLoad8;
			break;
		}
		cpu->memory.cpuLoad8 = GBCartLoad8;
		if (gb->memory.mbcType != GB_MBC6 && !(gb->memory.mbcType == GB_UNL_NT_NEW && gb->memory.mbcState.ntNew.splitMode)) {
			cpu->memory.activeRegion = memory->romBank;
			cpu->memory.activeRegionEnd = GB_BASE_VRAM;
			cpu->memory.activeMask = GB_SIZE_CART_BANK0 - 1;
		} else {
			cpu->memory.activeMask = GB_SIZE_CART_HALFBANK - 1;
			if (address & 0x2000) {
				cpu->memory.activeRegion = memory->romBank1;
				cpu->memory.activeRegionEnd = GB_BASE_VRAM;
			} else {
				cpu->memory.activeRegion = memory->romBank;
				cpu->memory.activeRegionEnd = GB_BASE_CART_BANK1 + 0x2000;
			}
		}
		if (gb->memory.romSize < GB_SIZE_CART_BANK0 * 2) {
			if (address >= gb->memory.romSize) {
				cpu->memory.activeRegion = _yankBuffer;
				cpu->memory.activeMask = 0;
			} else {
				cpu->memory.activeRegionEnd = gb->memory.romSize;
			}
		}
		break;
	default:
		cpu->memory.cpuLoad8 = GBLoad8;
		break;
	}
	if (gb->memory.dmaRemaining) {
		const enum GBBus* block = gb->model < GB_MODEL_CGB ? _oamBlockDMG : _oamBlockCGB;
		enum GBBus dmaBus = block[memory->dmaSource >> 13];
		enum GBBus accessBus = block[address >> 13];
		if ((dmaBus != GB_BUS_CPU && dmaBus == accessBus) || (address >= GB_BASE_OAM && address < GB_BASE_UNUSABLE)) {
			cpu->memory.activeRegion = _blockedRegion;
			cpu->memory.activeMask = 0;
		}
	}
}

static void _GBMemoryDMAService(struct mTiming* timing, void* context, uint32_t cyclesLate);
static void _GBMemoryHDMAService(struct mTiming* timing, void* context, uint32_t cyclesLate);

void GBMemoryInit(struct GB* gb) {
	struct SM83Core* cpu = gb->cpu;
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
	gb->memory.cam = NULL;

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

	GBMBCReset(gb);
}

void GBMemorySwitchWramBank(struct GBMemory* memory, int bank) {
	bank &= 7;
	if (!bank) {
		bank = 1;
	}
	memory->wramBank = &memory->wram[GB_SIZE_WORKING_RAM_BANK0 * bank];
	memory->wramCurrentBank = bank;
}

uint8_t GBLoad8(struct SM83Core* cpu, uint16_t address) {
	struct GB* gb = (struct GB*) cpu->master;
	struct GBMemory* memory = &gb->memory;
	if (gb->memory.dmaRemaining) {
		const enum GBBus* block = gb->model < GB_MODEL_CGB ? _oamBlockDMG : _oamBlockCGB;
		enum GBBus dmaBus = block[memory->dmaSource >> 13];
		enum GBBus accessBus = block[address >> 13];
		if (dmaBus != GB_BUS_CPU && dmaBus == accessBus) {
			return 0xFF;
		}
		if (address >= GB_BASE_OAM && address < GB_BASE_IO) {
			return 0xFF;
		}
	}
	switch (address >> 12) {
	case GB_REGION_CART_BANK0:
	case GB_REGION_CART_BANK0 + 1:
	case GB_REGION_CART_BANK0 + 2:
	case GB_REGION_CART_BANK0 + 3:
		if (address >= memory->romSize) {
			memory->cartBus = 0xFF;
		} else if (gb->memory.mbcReadBank0) {
			memory->cartBus = memory->mbcRead(memory, address);
		} else {
			memory->cartBus = memory->romBase[address & (GB_SIZE_CART_BANK0 - 1)];
		}
		memory->cartBusPc = cpu->pc;
		return memory->cartBus;
	case GB_REGION_CART_BANK1 + 2:
	case GB_REGION_CART_BANK1 + 3:
		if (gb->memory.mbcType == GB_MBC6 || (gb->memory.mbcType == GB_UNL_NT_NEW && gb->memory.mbcState.ntNew.splitMode)) {
			memory->cartBus = memory->romBank1[address & (GB_SIZE_CART_HALFBANK - 1)];
			memory->cartBusPc = cpu->pc;
			return memory->cartBus;
		}
		// Fall through
	case GB_REGION_CART_BANK1:
	case GB_REGION_CART_BANK1 + 1:
		if (address >= memory->romSize) {
			memory->cartBus = 0xFF;
		} else if (gb->memory.mbcReadBank1) {
			memory->cartBus = memory->mbcRead(memory, address);
		} else {
			memory->cartBus = memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
		}
		memory->cartBusPc = cpu->pc;
		return memory->cartBus;
	case GB_REGION_VRAM:
	case GB_REGION_VRAM + 1:
		if (gb->video.mode != 3) {
			return gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)];
		}
		return 0xFF;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->rtcAccess) {
			memory->cartBus = memory->rtcRegs[memory->activeRtcReg];
		} else if (memory->mbcRead) {
			memory->cartBus = memory->mbcRead(memory, address);
		} else if (memory->sramAccess && memory->sram) {
			memory->cartBus = memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
		} else if (memory->mbcType == GB_HuC3) {
			memory->cartBus = 0x01; // TODO: Is this supposed to be the current SRAM bank?
		} else if (cpu->tMultiplier * (cpu->pc - memory->cartBusPc) >= memory->cartBusDecay) {
			memory->cartBus = 0xFF;
		}
		memory->cartBusPc = cpu->pc;
		return memory->cartBus;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		if (gb->memory.mbcReadHigh) {
			memory->mbcRead(memory, address);
		}
		return memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)];
	case GB_REGION_WORKING_RAM_BANK1:
		if (gb->memory.mbcReadHigh) {
			memory->mbcRead(memory, address);
		}
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
		return GBIORead(gb, GB_REG_IE);
	}
}

void GBStore8(struct SM83Core* cpu, uint16_t address, int8_t value) {
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
		if (gb->video.mode != 3) {
			gb->video.renderer->writeVRAM(gb->video.renderer, (address & (GB_SIZE_VRAM_BANK0 - 1)) | (GB_SIZE_VRAM_BANK0 * gb->video.vramCurrentBank));
			gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)] = value;
		}
		return;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->rtcAccess) {
			memory->rtcRegs[memory->activeRtcReg] = value;
		} else if (memory->sramAccess && memory->sram && memory->directSramAccess) {
			if (memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)] != value) {
				memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)] = value;
				gb->sramDirty |= mSAVEDATA_DIRT_NEW;
			}
		} else {
			memory->mbcWrite(gb, address, value);
		}
		return;
	case GB_REGION_WORKING_RAM_BANK0:
	case GB_REGION_WORKING_RAM_BANK0 + 2:
		if (memory->mbcWriteHigh) {
			memory->mbcWrite(gb, address, value);
		}
		memory->wram[address & (GB_SIZE_WORKING_RAM_BANK0 - 1)] = value;
		return;
	case GB_REGION_WORKING_RAM_BANK1:
		if (memory->mbcWriteHigh) {
			memory->mbcWrite(gb, address, value);
		}
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
			GBIOWrite(gb, GB_REG_IE, value);
		}
	}
}

int GBCurrentSegment(struct SM83Core* cpu, uint16_t address) {
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

uint8_t GBView8(struct SM83Core* cpu, uint16_t address, int segment) {
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
			if (memory->sram) {
				if (segment < 0) {
					return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
				} else if ((size_t) segment * GB_SIZE_EXTERNAL_RAM < gb->sramSize) {
					return memory->sram[(address & (GB_SIZE_EXTERNAL_RAM - 1)) + segment *GB_SIZE_EXTERNAL_RAM];
				}
			}
			return 0xFF;
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
			if (gb->video.mode < 2) {
				switch (gb->model) {
				case GB_MODEL_AGB:
					return (address & 0xF0) | ((address >> 4) & 0xF);
				case GB_MODEL_CGB:
					// TODO: R/W behavior
					return 0x00;
				default:
					return 0x00;
				}
			}
			return 0xFF;
		}
		if (address < GB_BASE_HRAM) {
			return GBIORead(gb, address & (GB_SIZE_IO - 1));
		}
		if (address < GB_BASE_IE) {
			return memory->hram[address & GB_SIZE_HRAM];
		}
		return GBIORead(gb, GB_REG_IE);
	}
}

void GBMemoryDMA(struct GB* gb, uint16_t base) {
	if (base >= 0xE000) {
		base &= 0xDFFF;
	}
	mTimingDeschedule(&gb->timing, &gb->memory.dmaEvent);
	mTimingSchedule(&gb->timing, &gb->memory.dmaEvent, 8 * (2 - gb->doubleSpeed));
	gb->memory.dmaSource = base;
	gb->memory.dmaDest = 0;
	gb->memory.dmaRemaining = 0xA0;
}

uint8_t GBMemoryWriteHDMA5(struct GB* gb, uint8_t value) {
	gb->memory.hdmaSource = gb->memory.io[GB_REG_HDMA1] << 8;
	gb->memory.hdmaSource |= gb->memory.io[GB_REG_HDMA2];
	gb->memory.hdmaDest = gb->memory.io[GB_REG_HDMA3] << 8;
	gb->memory.hdmaDest |= gb->memory.io[GB_REG_HDMA4];
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
		mTimingSchedule(timing, &gb->memory.dmaEvent, 4 * (2 - gb->doubleSpeed) - cyclesLate);
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
		mTimingSchedule(timing, &gb->memory.hdmaEvent, 4 - cyclesLate);
	} else {
		gb->cpuBlocked = false;
		gb->memory.io[GB_REG_HDMA1] = gb->memory.hdmaSource >> 8;
		gb->memory.io[GB_REG_HDMA2] = gb->memory.hdmaSource;
		gb->memory.io[GB_REG_HDMA3] = gb->memory.hdmaDest >> 8;
		gb->memory.io[GB_REG_HDMA4] = gb->memory.hdmaDest;
		if (gb->memory.isHdma) {
			--gb->memory.io[GB_REG_HDMA5];
			if (gb->memory.io[GB_REG_HDMA5] == 0xFF) {
				gb->memory.isHdma = false;
			}
		} else {
			gb->memory.io[GB_REG_HDMA5] = 0xFF;
		}
	}
}

void GBPatch8(struct SM83Core* cpu, uint16_t address, int8_t value, int8_t* old, int segment) {
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
		if (memory->rtcAccess) {
			memory->rtcRegs[memory->activeRtcReg] = value;
		} else if (memory->sramAccess && memory->sram && memory->mbcType != GB_MBC2) {
			// TODO: Remove sramAccess check?
			memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)] = value;
		} else {
			memory->mbcWrite(gb, address, value);
		}
		gb->sramDirty |= mSAVEDATA_DIRT_NEW;
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

	state->memory.cartBus = memory->cartBus;
	STORE_16LE(memory->cartBusPc, 0, &state->cartBusPc);

	int i;
	switch (memory->mbcType) {
	case GB_MBC1:
		state->memory.mbc1.mode = memory->mbcState.mbc1.mode;
		state->memory.mbc1.multicartStride = memory->mbcState.mbc1.multicartStride;
		state->memory.mbc1.bankLo = memory->mbcState.mbc1.bankLo;
		state->memory.mbc1.bankHi = memory->mbcState.mbc1.bankHi;
		break;
	case GB_MBC3_RTC:
		STORE_64LE(memory->rtcLastLatch, 0, &state->memory.rtc.lastLatch);
		break;
	case GB_MBC6:
		state->memory.mbc6.flags = GBSerializedMBC6FlagsSetFlashBank0(0, memory->mbcState.mbc6.flashBank0);
		state->memory.mbc6.flags = GBSerializedMBC6FlagsSetFlashBank1(state->memory.mbc6.flags, memory->mbcState.mbc6.flashBank1);
		state->memory.mbc6.bank1 = memory->currentBank1;
		state->memory.mbc6.sramBank1 = memory->currentSramBank1;
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
	case GB_TAMA5:
		STORE_64LE(memory->rtcLastLatch, 0, &state->memory.tama5.lastLatch);
		state->memory.tama5.reg = memory->mbcState.tama5.reg;
		for (i = 0; i < 4; ++i) {
			state->tama5Registers.registers[i] = memory->mbcState.tama5.registers[i * 2] & 0xF;
			state->tama5Registers.registers[i] |= memory->mbcState.tama5.registers[i * 2 + 1] << 4;
		}
		for (i = 0; i < 8; ++i) {
			state->tama5Registers.rtcTimerPage[i] = memory->mbcState.tama5.rtcTimerPage[i * 2] & 0xF;
			state->tama5Registers.rtcTimerPage[i] |= memory->mbcState.tama5.rtcTimerPage[i * 2 + 1] << 4;
			state->tama5Registers.rtcAlarmPage[i] = memory->mbcState.tama5.rtcAlarmPage[i * 2] & 0xF;
			state->tama5Registers.rtcAlarmPage[i] |= memory->mbcState.tama5.rtcAlarmPage[i * 2 + 1] << 4;
			state->tama5Registers.rtcFreePage0[i] = memory->mbcState.tama5.rtcFreePage0[i * 2] & 0xF;
			state->tama5Registers.rtcFreePage0[i] |= memory->mbcState.tama5.rtcFreePage0[i * 2 + 1] << 4;
			state->tama5Registers.rtcFreePage1[i] = memory->mbcState.tama5.rtcFreePage1[i * 2] & 0xF;
			state->tama5Registers.rtcFreePage1[i] |= memory->mbcState.tama5.rtcFreePage1[i * 2 + 1] << 4;
		}
		break;
	case GB_HuC3:
		STORE_64LE(memory->rtcLastLatch, 0, &state->memory.huc3.lastLatch);
		state->memory.huc3.index = memory->mbcState.huc3.index;
		state->memory.huc3.value = memory->mbcState.huc3.value;
		state->memory.huc3.mode = memory->mbcState.huc3.mode;
		for (i = 0; i < 0x80; ++i) {
			state->huc3Registers[i] = memory->mbcState.huc3.registers[i * 2] & 0xF;
			state->huc3Registers[i] |= memory->mbcState.huc3.registers[i * 2 + 1] << 4;
		}
		break;
	case GB_POCKETCAM:
		state->memory.pocketCam.registersActive = memory->mbcState.pocketCam.registersActive;
		memcpy(state->pocketCamRegisters, memory->mbcState.pocketCam.registers, sizeof(memory->mbcState.pocketCam.registers));
		break;
	case GB_MMM01:
		state->memory.mmm01.locked = memory->mbcState.mmm01.locked;
		state->memory.mmm01.bank0 = memory->mbcState.mmm01.currentBank0;
		break;
	case GB_UNL_NT_OLD_1:
	case GB_UNL_NT_OLD_2:
		state->memory.ntOld.flags = GBSerializedNTOldFlagsSetSwapped(0, memory->mbcState.ntOld.swapped);
		state->memory.ntOld.flags = GBSerializedNTOldFlagsSetRumble(state->memory.ntOld.flags, memory->mbcState.ntOld.rumble);
		state->memory.ntOld.baseBank = memory->mbcState.ntOld.baseBank;
		state->memory.ntOld.bankCount = memory->mbcState.ntOld.bankCount;
		break;
	case GB_UNL_NT_NEW:
		state->memory.ntNew.splitMode = memory->mbcState.ntNew.splitMode;
		state->memory.ntNew.bank1 = memory->currentBank1;
		break;
	case GB_UNL_BBD:
	case GB_UNL_HITEK:
	case GB_UNL_GGB81:
		state->memory.bbd.dataSwapMode = memory->mbcState.bbd.dataSwapMode;
		state->memory.bbd.bankSwapMode = memory->mbcState.bbd.bankSwapMode;
		break;
	case GB_UNL_SACHEN_MMC1:
	case GB_UNL_SACHEN_MMC2:
		state->memory.sachen.flags = GBSerializedSachenFlagsSetTransition(0, memory->mbcState.sachen.transition);
		state->memory.sachen.flags = GBSerializedSachenFlagsSetLocked(state->memory.sachen.flags, memory->mbcState.sachen.locked);
		state->memory.sachen.mask = memory->mbcState.sachen.mask;
		state->memory.sachen.unmaskedBank = memory->mbcState.sachen.unmaskedBank;
		state->memory.sachen.baseBank = memory->mbcState.sachen.baseBank;
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

	GBMemorySwitchWramBank(memory, memory->wramCurrentBank);
	if (memory->mbcType != GB_MBC6 && memory->mbcType != GB_UNL_NT_NEW) {
		GBMBCSwitchBank(gb, memory->currentBank);
		GBMBCSwitchSramBank(gb, memory->sramCurrentBank);
	}

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
	} else {
		memory->dmaEvent.when = when + mTimingCurrentTime(&gb->timing);
	}
	LOAD_32LE(when, 0, &state->memory.hdmaNext);
	if (memory->hdmaRemaining) {
		mTimingSchedule(&gb->timing, &memory->hdmaEvent, when);
	} else {
		memory->hdmaEvent.when = when + mTimingCurrentTime(&gb->timing);
	}

	GBSerializedMemoryFlags flags;
	LOAD_16LE(flags, 0, &state->memory.flags);
	memory->sramAccess = GBSerializedMemoryFlagsGetSramAccess(flags);
	memory->rtcAccess = GBSerializedMemoryFlagsGetRtcAccess(flags);
	memory->rtcLatched = GBSerializedMemoryFlagsGetRtcLatched(flags);
	memory->ime = GBSerializedMemoryFlagsGetIme(flags);
	memory->isHdma = GBSerializedMemoryFlagsGetIsHdma(flags);
	memory->activeRtcReg = GBSerializedMemoryFlagsGetActiveRtcReg(flags);

	memory->cartBus = state->memory.cartBus;
	LOAD_16LE(memory->cartBusPc, 0, &state->cartBusPc);

	int i;
	switch (memory->mbcType) {
	case GB_MBC1:
		memory->mbcState.mbc1.mode = state->memory.mbc1.mode;
		memory->mbcState.mbc1.multicartStride = state->memory.mbc1.multicartStride;
		memory->mbcState.mbc1.bankLo = state->memory.mbc1.bankLo;
		memory->mbcState.mbc1.bankHi = state->memory.mbc1.bankHi;
		if (!(memory->mbcState.mbc1.bankLo || memory->mbcState.mbc1.bankHi)) {
			// Backwards compat
			memory->mbcState.mbc1.bankLo = memory->currentBank & ((1 << memory->mbcState.mbc1.multicartStride) - 1);
			memory->mbcState.mbc1.bankHi = memory->currentBank >> memory->mbcState.mbc1.multicartStride;
		}
		if (memory->mbcState.mbc1.mode) {
			GBMBCSwitchBank0(gb, memory->mbcState.mbc1.bankHi << memory->mbcState.mbc1.multicartStride);
		}
		break;
	case GB_MBC3_RTC:
		LOAD_64LE(memory->rtcLastLatch, 0, &state->memory.rtc.lastLatch);
		break;
	case GB_MBC6:
		memory->mbcState.mbc6.flashBank0 = GBSerializedMBC6FlagsGetFlashBank0(state->memory.mbc6.flags);
		memory->mbcState.mbc6.flashBank1 = GBSerializedMBC6FlagsGetFlashBank1(state->memory.mbc6.flags);
		memory->currentBank1 = state->memory.mbc6.bank1;
		memory->currentSramBank1 = state->memory.mbc6.sramBank1;
		GBMBCSwitchHalfBank(gb, 0, memory->currentBank);
		GBMBCSwitchHalfBank(gb, 1, memory->currentBank1);
		GBMBCSwitchSramHalfBank(gb, 0, memory->sramCurrentBank);
		GBMBCSwitchSramHalfBank(gb, 1, memory->currentSramBank1);
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
	case GB_TAMA5:
		LOAD_64LE(memory->rtcLastLatch, 0, &state->memory.tama5.lastLatch);
		memory->mbcState.tama5.reg = state->memory.tama5.reg;
		for (i = 0; i < 4; ++i) {
			memory->mbcState.tama5.registers[i * 2] = state->tama5Registers.registers[i] & 0xF;
			memory->mbcState.tama5.registers[i * 2 + 1] = state->tama5Registers.registers[i] >> 4;
		}
		for (i = 0; i < 8; ++i) {
			memory->mbcState.tama5.rtcTimerPage[i * 2] = state->tama5Registers.rtcTimerPage[i] & 0xF;
			memory->mbcState.tama5.rtcTimerPage[i * 2 + 1] = state->tama5Registers.rtcTimerPage[i] >> 4;
			memory->mbcState.tama5.rtcAlarmPage[i * 2] = state->tama5Registers.rtcAlarmPage[i] & 0xF;
			memory->mbcState.tama5.rtcAlarmPage[i * 2 + 1] = state->tama5Registers.rtcAlarmPage[i] >> 4;
			memory->mbcState.tama5.rtcFreePage0[i * 2] = state->tama5Registers.rtcFreePage0[i] & 0xF;
			memory->mbcState.tama5.rtcFreePage0[i * 2 + 1] = state->tama5Registers.rtcFreePage0[i] >> 4;
			memory->mbcState.tama5.rtcFreePage1[i * 2] = state->tama5Registers.rtcFreePage1[i] & 0xF;
			memory->mbcState.tama5.rtcFreePage1[i * 2 + 1] = state->tama5Registers.rtcFreePage1[i] >> 4;
		}
		break;
	case GB_HuC3:
		LOAD_64LE(memory->rtcLastLatch, 0, &state->memory.huc3.lastLatch);
		memory->mbcState.huc3.index = state->memory.huc3.index;
		memory->mbcState.huc3.value = state->memory.huc3.value;
		memory->mbcState.huc3.mode = state->memory.huc3.mode;
		for (i = 0; i < 0x80; ++i) {
			memory->mbcState.huc3.registers[i * 2] = state->huc3Registers[i] & 0xF;
			memory->mbcState.huc3.registers[i * 2 + 1] = state->huc3Registers[i] >> 4;
		}
		break;
	case GB_POCKETCAM:
		memory->mbcState.pocketCam.registersActive = state->memory.pocketCam.registersActive;
		memcpy(memory->mbcState.pocketCam.registers, state->pocketCamRegisters, sizeof(memory->mbcState.pocketCam.registers));
		break;
	case GB_MMM01:
		memory->mbcState.mmm01.locked = state->memory.mmm01.locked;
		memory->mbcState.mmm01.currentBank0 = state->memory.mmm01.bank0;
		if (memory->mbcState.mmm01.locked) {
			GBMBCSwitchBank0(gb, memory->mbcState.mmm01.currentBank0);
		} else {
			GBMBCSwitchBank0(gb, gb->memory.romSize / GB_SIZE_CART_BANK0 - 2);
		}
		break;
	case GB_UNL_NT_OLD_1:
	case GB_UNL_NT_OLD_2:
		memory->mbcState.ntOld.swapped = GBSerializedNTOldFlagsGetSwapped(state->memory.ntOld.flags);
		memory->mbcState.ntOld.rumble = GBSerializedNTOldFlagsGetRumble(state->memory.ntOld.flags);
		memory->mbcState.ntOld.baseBank = state->memory.ntOld.baseBank;
		memory->mbcState.ntOld.bankCount = state->memory.ntOld.bankCount;
		GBMBCSwitchBank0(gb, memory->mbcState.ntOld.baseBank);
		break;
	case GB_UNL_NT_NEW:
		memory->mbcState.ntNew.splitMode = state->memory.ntNew.splitMode;
		memory->currentBank1 = state->memory.ntNew.bank1;
		if (memory->mbcState.ntNew.splitMode) {
			GBMBCSwitchHalfBank(gb, 0, memory->currentBank);
			GBMBCSwitchHalfBank(gb, 1, memory->currentBank1);
		} else {
			GBMBCSwitchBank(gb, memory->currentBank);
		}
		break;
	case GB_UNL_BBD:
	case GB_UNL_HITEK:
	case GB_UNL_GGB81:
		memory->mbcState.bbd.dataSwapMode = state->memory.bbd.dataSwapMode & 0x7;
		memory->mbcState.bbd.bankSwapMode = state->memory.bbd.bankSwapMode & 0x7;
		break;
	case GB_UNL_SACHEN_MMC1:
	case GB_UNL_SACHEN_MMC2:
		memory->mbcState.sachen.transition = GBSerializedSachenFlagsGetTransition(state->memory.sachen.flags);
		memory->mbcState.sachen.locked = GBSerializedSachenFlagsGetLocked(state->memory.sachen.flags);
		memory->mbcState.sachen.mask = state->memory.sachen.mask;
		memory->mbcState.sachen.unmaskedBank = state->memory.sachen.unmaskedBank;
		memory->mbcState.sachen.baseBank = state->memory.sachen.baseBank;
		GBMBCSwitchBank0(gb, memory->mbcState.sachen.baseBank & memory->mbcState.sachen.mask);
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
	if (gb->romVf) {
		gb->romVf->unmap(gb->romVf, gb->memory.rom, gb->memory.romSize);
		gb->romVf->close(gb->romVf);
		gb->romVf = NULL;
	}
	gb->memory.rom = newRom;
	GBMBCSwitchBank(gb, gb->memory.currentBank);
	gb->isPristine = false;
}
