/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "memory.h"

#include "core/interface.h"
#include "gb/gb.h"
#include "gb/io.h"

#include "util/memory.h"

#include <time.h>

mLOG_DEFINE_CATEGORY(GB_MBC, "GB MBC");
mLOG_DEFINE_CATEGORY(GB_MEM, "GB Memory");

static void _GBMBCNone(struct GBMemory* memory, uint16_t address, uint8_t value) {
	UNUSED(memory);
	UNUSED(address);
	UNUSED(value);

	mLOG(GB_MBC, GAME_ERROR, "Wrote to invalid MBC");
}

static void _GBMBC1(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC2(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC3(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC5(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC6(struct GBMemory*, uint16_t address, uint8_t value);
static void _GBMBC7(struct GBMemory*, uint16_t address, uint8_t value);
static uint8_t _GBMBC7Read(struct GBMemory*, uint16_t address);
static void _GBMBC7Write(struct GBMemory*, uint16_t address, uint8_t value);

static void GBSetActiveRegion(struct LR35902Core* cpu, uint16_t address) {
	// TODO
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
	gb->memory.mbcType = GB_MBC_NONE;
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
	GBMemorySwitchWramBank(&gb->memory, 1);
	gb->memory.romBank = &gb->memory.rom[GB_SIZE_CART_BANK0];
	gb->memory.currentBank = 1;
	gb->memory.sramCurrentBank = 0;
	gb->memory.sramBank = gb->memory.sram;

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
	gb->memory.rtcLatched = 0;
	memset(&gb->memory.rtcRegs, 0, sizeof(gb->memory.rtcRegs));

	memset(&gb->memory.hram, 0, sizeof(gb->memory.hram));

	const struct GBCartridge* cart = (const struct GBCartridge*) &gb->memory.rom[0x100];
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
	case 0x20:
		gb->memory.mbc = _GBMBC6;
		gb->memory.mbcType = GB_MBC6;
		break;
	case 0x22:
		gb->memory.mbc = _GBMBC7;
		gb->memory.mbcType = GB_MBC7;
		memset(&gb->memory.mbcState.mbc7, 0, sizeof(gb->memory.mbcState.mbc7));
		break;
	}

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
		return memory->rom[address & (GB_SIZE_CART_BANK0 - 1)];
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
			return gb->memory.rtcRegs[memory->activeRtcReg];
		} else if (memory->sramAccess) {
			return gb->memory.sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
		} else if (memory->mbcType == GB_MBC7) {
			return _GBMBC7Read(memory, address);
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
		gb->video.vramBank[address & (GB_SIZE_VRAM_BANK0 - 1)] = value;
		return;
	case GB_REGION_EXTERNAL_RAM:
	case GB_REGION_EXTERNAL_RAM + 1:
		if (memory->rtcAccess) {
			gb->memory.rtcRegs[memory->activeRtcReg] = value;
		} else if (memory->sramAccess) {
			gb->memory.sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)] = value;
		} else if (gb->memory.mbcType == GB_MBC7) {
			_GBMBC7Write(&gb->memory, address, value);
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
	if (!wasHdma && !gb->memory.isHdma) {
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
		} else {
			gb->memory.io[REG_HDMA5] |= 0x80;
		}
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
		bankStart &= (GB_SIZE_CART_BANK0 - 1);
		bank /= GB_SIZE_CART_BANK0;
	}
	memory->romBank = &memory->rom[bankStart];
	memory->currentBank = bank;
}

static void _switchSramBank(struct GBMemory* memory, int bank) {
	size_t bankStart = bank * GB_SIZE_EXTERNAL_RAM;
	memory->sramBank = &memory->sram[bankStart];
	memory->sramCurrentBank = bank;
}

static void _latchRtc(struct GBMemory* memory) {
	time_t t;
	struct mRTCSource* rtc = memory->rtc;
	if (rtc) {
		if (rtc->sample) {
			rtc->sample(rtc);
		}
		t = rtc->unixTime(rtc);
	} else {
		t = time(0);
	}
	struct tm date;
	localtime_r(&t, &date);
	memory->rtcRegs[0] = date.tm_sec;
	memory->rtcRegs[1] = date.tm_min;
	memory->rtcRegs[2] = date.tm_hour;
	memory->rtcRegs[3] = date.tm_yday; // TODO: Persist day counter
	memory->rtcRegs[4] &= 0xF0;
	memory->rtcRegs[4] |= date.tm_yday >> 8;
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
	case 0x1:
		if (!bank) {
			++bank;
		}
		_switchBank(memory, bank | (memory->currentBank & 0x60));
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "MBC1 unknown address: %04X:%02X", address, value);
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
			memory->rtcAccess = false;
		} else if (value >= 8 && value <= 0xC) {
			memory->activeRtcReg = value - 8;
			memory->rtcAccess = true;
		}
		break;
	case 0x3:
		if (memory->rtcLatched && value == 0) {
			memory->rtcLatched = value;
		} else if (!memory->rtcLatched && value == 1) {
			_latchRtc(memory);
		}
		break;
	}
}

void _GBMBC5(struct GBMemory* memory, uint16_t address, uint8_t value) {
	int bank = value;
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
	case 0x1:
		_switchBank(memory, bank);
		break;
	case 0x2:
		if (value < 0x10) {
			_switchSramBank(memory, value);
		}
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "MBC5 unknown address: %04X:%02X", address, value);
		break;
	}
}

void _GBMBC6(struct GBMemory* memory, uint16_t address, uint8_t value) {
	// TODO
	mLOG(GB_MBC, STUB, "MBC6 unimplemented");
}

void _GBMBC7(struct GBMemory* memory, uint16_t address, uint8_t value) {
	int bank = value & 0x7F;
	switch (address >> 13) {
	case 0x1:
		_switchBank(memory, bank);
		break;
	case 0x2:
		if (value < 0x10) {
			_switchSramBank(memory, value);
		}
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "MBC7 unknown address: %04X:%02X", address, value);
		break;
	}
}

uint8_t _GBMBC7Read(struct GBMemory* memory, uint16_t address) {
	struct GBMBC7State* mbc7 = &memory->mbcState.mbc7;
	switch (address & 0xF0) {
	case 0x00:
	case 0x10:
	case 0x60:
	case 0x70:
		return 0;
	case 0x20:
		if (memory->rotation && memory->rotation->readTiltX) {
			int32_t x = -memory->rotation->readTiltX(memory->rotation);
			x >>= 21;
			x += 2047;
			return x;
		}
		return 0xFF;
	case 0x30:
		if (memory->rotation && memory->rotation->readTiltX) {
			int32_t x = -memory->rotation->readTiltX(memory->rotation);
			x >>= 21;
			x += 2047;
			return x >> 8;
		}
		return 7;
	case 0x40:
		if (memory->rotation && memory->rotation->readTiltY) {
			int32_t y = -memory->rotation->readTiltY(memory->rotation);
			y >>= 21;
			y += 2047;
			return y;
		}
		return 0xFF;
	case 0x50:
		if (memory->rotation && memory->rotation->readTiltY) {
			int32_t y = -memory->rotation->readTiltY(memory->rotation);
			y >>= 21;
			y += 2047;
			return y >> 8;
		}
		return 7;
	case 0x80:
		return (mbc7->sr >> 16) & 1;
	default:
		return 0xFF;
	}
}

void _GBMBC7Write(struct GBMemory* memory, uint16_t address, uint8_t value) {
	if ((address & 0xF0) != 0x80) {
		return;
	}
	struct GBMBC7State* mbc7 = &memory->mbcState.mbc7;
	GBMBC7Field old = memory->mbcState.mbc7.field;
	mbc7->field = GBMBC7FieldClearIO(value);
	if (!GBMBC7FieldIsCS(old) && GBMBC7FieldIsCS(value)) {
		if (mbc7->state == GBMBC7_STATE_WRITE) {
			if (mbc7->writable) {
				memory->sramBank[mbc7->address * 2] = mbc7->sr >> 8;
				memory->sramBank[mbc7->address * 2 + 1] = mbc7->sr;
			}
			mbc7->sr = 0x1FFFF;
			mbc7->state = GBMBC7_STATE_NULL;
		} else {
			mbc7->state = GBMBC7_STATE_IDLE;
		}
	}
	if (!GBMBC7FieldIsSK(old) && GBMBC7FieldIsSK(value)) {
		if (mbc7->state > GBMBC7_STATE_IDLE && mbc7->state != GBMBC7_STATE_READ) {
			mbc7->sr <<= 1;
			mbc7->sr |= GBMBC7FieldGetIO(value);
			++mbc7->srBits;
		}
		switch (mbc7->state) {
		case GBMBC7_STATE_IDLE:
			if (GBMBC7FieldIsIO(value)) {
				mbc7->state = GBMBC7_STATE_READ_COMMAND;
				mbc7->srBits = 0;
				mbc7->sr = 0;
			}
			break;
		case GBMBC7_STATE_READ_COMMAND:
			if (mbc7->srBits == 2) {
				mbc7->state = GBMBC7_STATE_READ_ADDRESS;
				mbc7->srBits = 0;
				mbc7->command = mbc7->sr;
			}
			break;
		case GBMBC7_STATE_READ_ADDRESS:
			if (mbc7->srBits == 8) {
				mbc7->state = GBMBC7_STATE_COMMAND_0 + mbc7->command;
				mbc7->srBits = 0;
				mbc7->address = mbc7->sr;
				if (mbc7->state == GBMBC7_STATE_COMMAND_0) {
					switch (mbc7->address >> 6) {
					case 0:
						mbc7->writable = false;
						mbc7->state = GBMBC7_STATE_NULL;
						break;
					case 3:
						mbc7->writable = true;
						mbc7->state = GBMBC7_STATE_NULL;
						break;
					}
				}
			}
			break;
		case GBMBC7_STATE_COMMAND_0:
			if (mbc7->srBits == 16) {
				switch (mbc7->address >> 6) {
				case 0:
					mbc7->writable = false;
					mbc7->state = GBMBC7_STATE_NULL;
					break;
				case 1:
					mbc7->state = GBMBC7_STATE_WRITE;
					if (mbc7->writable) {
						int i;
						for (i = 0; i < 256; ++i) {
							memory->sramBank[i * 2] = mbc7->sr >> 8;
							memory->sramBank[i * 2 + 1] = mbc7->sr;
						}
					}
					break;
				case 2:
					mbc7->state = GBMBC7_STATE_WRITE;
					if (mbc7->writable) {
						int i;
						for (i = 0; i < 256; ++i) {
							memory->sramBank[i * 2] = 0xFF;
							memory->sramBank[i * 2 + 1] = 0xFF;
						}
					}
					break;
				case 3:
					mbc7->writable = true;
					mbc7->state = GBMBC7_STATE_NULL;
					break;
				}
			}
			break;
		case GBMBC7_STATE_COMMAND_SR_WRITE:
			if (mbc7->srBits == 16) {
				mbc7->srBits = 0;
				mbc7->state = GBMBC7_STATE_WRITE;
			}
			break;
		case GBMBC7_STATE_COMMAND_SR_READ:
			if (mbc7->srBits == 1) {
				mbc7->sr = memory->sramBank[mbc7->address * 2] << 8;
				mbc7->sr |= memory->sramBank[mbc7->address * 2 + 1];
				mbc7->srBits = 0;
				mbc7->state = GBMBC7_STATE_READ;
			}
			break;
		case GBMBC7_STATE_COMMAND_SR_FILL:
			if (mbc7->srBits == 16) {
				mbc7->sr = 0xFFFF;
				mbc7->srBits = 0;
				mbc7->state = GBMBC7_STATE_WRITE;
			}
			break;
		default:
			break;
		}
	} else if (GBMBC7FieldIsSK(old) && !GBMBC7FieldIsSK(value)) {
		if (mbc7->state == GBMBC7_STATE_READ) {
			mbc7->sr <<= 1;
			++mbc7->srBits;
			if (mbc7->srBits == 16) {
				mbc7->srBits = 0;
				mbc7->state = GBMBC7_STATE_NULL;
			}
		}
	}
}
