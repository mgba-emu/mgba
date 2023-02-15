/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb/mbc/mbc-private.h"

#include <mgba/internal/gb/gb.h>

void _GBMMM01(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	if (!memory->mbcState.mmm01.locked) {
		switch (address >> 13) {
		case 0x0:
			memory->mbcState.mmm01.locked = true;
			GBMBCSwitchBank0(gb, memory->mbcState.mmm01.currentBank0);
			break;
		case 0x1:
			memory->mbcState.mmm01.currentBank0 &= ~0x7F;
			memory->mbcState.mmm01.currentBank0 |= value & 0x7F;
			break;
		case 0x2:
			memory->mbcState.mmm01.currentBank0 &= ~0x180;
			memory->mbcState.mmm01.currentBank0 |= (value & 0x30) << 3;
			break;
		default:
			// TODO
			mLOG(GB_MBC, STUB, "MMM01 unknown address: %04X:%02X", address, value);
			break;
		}
		return;
	}
	switch (address >> 13) {
	case 0x0:
		switch (value) {
		case 0xA:
			memory->sramAccess = true;
			GBMBCSwitchSramBank(gb, memory->sramCurrentBank);
			break;
		default:
			memory->sramAccess = false;
			break;
		}
		break;
	case 0x1:
		GBMBCSwitchBank(gb, value + memory->mbcState.mmm01.currentBank0);
		break;
	case 0x2:
		GBMBCSwitchSramBank(gb, value);
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "MMM01 unknown address: %04X:%02X", address, value);
		break;
	}
}

void _GBHuC1(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	int bank = value & 0x3F;
	switch (address >> 13) {
	case 0x0:
		switch (value) {
		case 0xE:
			memory->sramAccess = false;
			break;
		default:
			memory->sramAccess = true;
			GBMBCSwitchSramBank(gb, memory->sramCurrentBank);
			break;
		}
		break;
	case 0x1:
		GBMBCSwitchBank(gb, bank);
		break;
	case 0x2:
		GBMBCSwitchSramBank(gb, value);
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "HuC-1 unknown address: %04X:%02X", address, value);
		break;
	}
}
