/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_MEMORY_H
#define GB_MEMORY_H

#include "util/common.h"

#include "lr35902/lr35902.h"

struct GB;

enum {
	GB_BASE_CART_BANK0 = 0x0000,
	GB_BASE_CART_BANK1 = 0x4000,
	GB_BASE_VRAM = 0x8000,
	GB_BASE_EXTERNAL_RAM = 0xA000,
	GB_BASE_WORKING_RAM_BANK0 = 0xC000,
	GB_BASE_WORKING_RAM_BANK1 = 0xD000,
	GB_BASE_OAM = 0xFE00,
	GB_BASE_IO = 0xFF00,
	GB_BASE_HRAM = 0xFF80,
	GB_BASE_IE = 0xFFFF
};

enum {
	GB_REGION_CART_BANK0 = 0x0,
	GB_REGION_CART_BANK1 = 0x4,
	GB_REGION_VRAM = 0x8,
	GB_REGION_EXTERNAL_RAM = 0xA,
	GB_REGION_WORKING_RAM_BANK0 = 0xC,
	GB_REGION_WORKING_RAM_BANK1 = 0xD,
	GB_REGION_WORKING_RAM_BANK1_MIRROR = 0xE,
	GB_REGION_OTHER = 0xF,
};

enum {
	GB_SIZE_CART_BANK0 = 0x4000,
	GB_SIZE_VRAM = 0x2000,
	GB_SIZE_EXTERNAL_RAM = 0x2000,
	GB_SIZE_WORKING_RAM = 0x8000,
	GB_SIZE_WORKING_RAM_BANK0 = 0x1000,
	GB_SIZE_OAM = 0xA0,
	GB_SIZE_IO = 0x80,
	GB_SIZE_HRAM = 0x7F,
};

enum GBMemoryBankControllerType {
	GB_MBC_NONE = 0,
	GB_MBC1 = 1,
	GB_MBC2 = 2,
	GB_MBC3 = 3,
	GB_MBC4 = 4,
	GB_MBC5 = 5,
	GB_MMM01 = 0x10,
	GB_HuC1 = 0x11
};

struct GBMemory;
typedef void (*GBMemoryBankController)(struct GBMemory*, uint16_t address, uint8_t value);

struct GBMemory {
	uint8_t* rom;
	uint8_t* romBank;
	enum GBMemoryBankControllerType mbcType;
	GBMemoryBankController mbc;
	int currentBank;

	uint8_t* wram;
	uint8_t* wramBank;

	uint8_t io[GB_SIZE_IO];
	bool ime;
	uint8_t ie;

	uint8_t hram[GB_SIZE_HRAM];

	size_t romSize;
};

void GBMemoryInit(struct GB* gb);
void GBMemoryDeinit(struct GB* gb);

void GBMemoryReset(struct GB* gb);

uint16_t GBLoad16(struct LR35902Core* cpu, uint16_t address);
uint8_t GBLoad8(struct LR35902Core* cpu, uint16_t address);

void GBStore16(struct LR35902Core* cpu, uint16_t address, int16_t value);
void GBStore8(struct LR35902Core* cpu, uint16_t address, int8_t value);

uint16_t GBView16(struct LR35902Core* cpu, uint16_t address);
uint8_t GBView8(struct LR35902Core* cpu, uint16_t address);

void GBPatch16(struct LR35902Core* cpu, uint16_t address, int16_t value, int16_t* old);
void GBPatch8(struct LR35902Core* cpu, uint16_t address, int8_t value, int8_t* old);

#endif
