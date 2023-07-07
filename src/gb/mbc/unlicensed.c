/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gb/mbc/mbc-private.h"

#include <mgba/internal/gb/gb.h>

void _GBWisdomTree(struct GB* gb, uint16_t address, uint8_t value) {
	UNUSED(value);
	int bank = address & 0x3F;
	switch (address >> 14) {
	case 0x0:
		GBMBCSwitchBank0(gb, bank * 2);
		GBMBCSwitchBank(gb, bank * 2 + 1);
		break;
	default:
		// TODO
		mLOG(GB_MBC, STUB, "Wisdom Tree unknown address: %04X:%02X", address, value);
		break;
	}
}

void _GBPKJD(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	switch (address >> 13) {
	case 0x2:
		if (value < 8) {
			memory->directSramAccess = true;
			memory->activeRtcReg = 0;
		} else if (value >= 0xD && value <= 0xF) {
			memory->directSramAccess = false;
			memory->rtcAccess = false;
			memory->activeRtcReg = value - 8;
		}
		break;
	case 0x5:
		if (!memory->sramAccess) {
			return;
		}
		switch (memory->activeRtcReg) {
		case 0:
			memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)] = value;
			break;
		case 5:
		case 6:
			memory->mbcState.pkjd.reg[memory->activeRtcReg - 5] = value;
			break;
		case 7:
			switch (value) {
			case 0x11:
				memory->mbcState.pkjd.reg[0]--;
				break;
			case 0x12:
				memory->mbcState.pkjd.reg[1]--;
				break;
			case 0x41:
				memory->mbcState.pkjd.reg[0] += memory->mbcState.pkjd.reg[1];
				break;
			case 0x42:
				memory->mbcState.pkjd.reg[1] += memory->mbcState.pkjd.reg[0];
				break;
			case 0x51:
				memory->mbcState.pkjd.reg[0]++;
				break;
			case 0x52:
				memory->mbcState.pkjd.reg[1]--;
				break;
			}
			break;
		}
		return;
	}
	_GBMBC3(gb, address, value);
}

uint8_t _GBPKJDRead(struct GBMemory* memory, uint16_t address) {
	if (!memory->sramAccess) {
		return 0xFF;
	}
	switch (memory->activeRtcReg) {
	case 0:
		return memory->sramBank[address & (GB_SIZE_EXTERNAL_RAM - 1)];
	case 5:
	case 6:
		return memory->mbcState.pkjd.reg[memory->activeRtcReg - 5];
	default:
		return 0;
	}
}


static uint8_t _reorderBits(uint8_t input, const uint8_t* reorder) {
	uint8_t newbyte = 0;
	int i;
	for(i = 0; i < 8; ++i) {
		int oldbit = reorder[i];
		int newbit = i;
		newbyte += ((input >> oldbit) & 1) << newbit;
	}

	return newbyte;
}

static const uint8_t _ntOld1Reorder[8] = {
	0, 2, 1, 4, 3, 5, 6, 7
};

void _ntOldMulticart(struct GB* gb, uint16_t address, uint8_t value, const uint8_t reorder[8]) {
	struct GBMemory* memory = &gb->memory;
	struct GBNTOldState* mbcState = &memory->mbcState.ntOld;
	int bank = value;

	switch (address & 3) {
	case 0:
		mLOG(GB_MBC, STUB, "Unimplemented NT Old 1 address 0");
		break;
	case 1:
		value &= 0x3F;
		mbcState->baseBank = value * 2;
		if (mbcState->baseBank) {
			GBMBCSwitchBank0(gb, mbcState->baseBank);
			GBMBCSwitchBank(gb, mbcState->baseBank + 1);
		}
		break;
	case 2:
		if ((value & 0xF0) == 0xE0) {
			gb->sramSize = 0x2000;
			GBResizeSram(gb, gb->sramSize);
		}
		switch (value & 0xF) {
		case 0x00:
			mbcState->bankCount = 32;
			break;
		case 0x08:
			mbcState->bankCount = 16;
			break;
		case 0xC:
			mbcState->bankCount = 8;
			break;
		case 0xE:
			mbcState->bankCount = 4;
			break;
		case 0xF:
			mbcState->bankCount = 2;
			break;
		default:
			mbcState->bankCount = 32;
			break;
		}
		break;
	case 3:
		mbcState->swapped = !!(value & 0x10);

		bank = memory->currentBank;
		if (mbcState->swapped) {
			bank = _reorderBits(bank, reorder);
		}
		GBMBCSwitchBank(gb, bank);
		break;
	}
}

void _GBNTOld1(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	struct GBNTOldState* mbcState = &memory->mbcState.ntOld;
	int bank = value;

	switch (address >> 12) {
	case 0x0:
	case 0x1:
		_GBMBC3(gb, address, value);
		break;
	case 0x2:
	case 0x3:
		bank &= 0x1F;
		if (!bank) {
			bank = 1;
		}
		if (mbcState->swapped) {
			bank = _reorderBits(bank, _ntOld1Reorder);
		}
		if (mbcState->bankCount) {
			bank &= mbcState->bankCount - 1;
		}
		GBMBCSwitchBank(gb, bank + mbcState->baseBank);
		break;
	case 0x5:
		_ntOldMulticart(gb, address, value, _ntOld1Reorder);
		break;
	}
}

static const uint8_t _ntOld2Reorder[8] = {
	1, 2, 0, 3, 4, 5, 6, 7
};

void _GBNTOld2(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	struct GBNTOldState* mbcState = &memory->mbcState.ntOld;
	int bank = value;

	switch (address >> 12) {
	case 0x0:
	case 0x1:
		_GBMBC3(gb, address, value);
		break;
	case 0x2:
	case 0x3:
		if (!bank) {
			bank = 1;
		}
		if (mbcState->swapped) {
			bank = _reorderBits(bank, _ntOld2Reorder);
		}
		if (mbcState->bankCount) {
			bank &= mbcState->bankCount - 1;
		}
		GBMBCSwitchBank(gb, bank + mbcState->baseBank);
		break;
	case 0x5:
		_ntOldMulticart(gb, address, value, _ntOld2Reorder);
		// Fall through
	case 0x4:
		if (address == 0x5001) {
			mbcState->rumble = !!(value & 0x80);
		}

		if (mbcState->rumble && memory->rumble) {
			memory->rumble->setRumble(memory->rumble, !!(mbcState->swapped ? value & 0x08 : value & 0x02));
		}
		break;
	}
}

void _GBNTNew(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	if (address >> 8 == 0x14) {
		memory->mbcState.ntNew.splitMode = true;
		return;
	}
	if (memory->mbcState.ntNew.splitMode) {
		int bank = value;
		if (bank < 2) {
			bank = 2;
		}
		switch (address >> 10) {
		case 8:
			GBMBCSwitchHalfBank(gb, 0, bank);
			return;
		case 9:
			GBMBCSwitchHalfBank(gb, 1, bank);
			return;
		}
	}
	_GBMBC5(gb, address, value);
}

static const uint8_t _bbdDataReordering[8][8] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 00 - Normal
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 01 - NOT KNOWN YET
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 02 - NOT KNOWN YET
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 03 - NOT KNOWN YET
	{ 0, 5, 1, 3, 4, 2, 6, 7 }, // 04 - Garou
	{ 0, 4, 2, 3, 1, 5, 6, 7 }, // 05 - Harry
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 06 - NOT KNOWN YET
	{ 0, 1, 5, 3, 4, 2, 6, 7 }, // 07 - Digimon
};

static const uint8_t _bbdBankReordering[8][8] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 00 - Normal
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 01 - NOT KNOWN YET
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 02 - NOT KNOWN YET
	{ 3, 4, 2, 0, 1, 5, 6, 7 }, // 03 - 0,1 unconfirmed. Digimon/Garou
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 04 - NOT KNOWN YET
	{ 1, 2, 3, 4, 0, 5, 6, 7 }, // 05 - 0,1 unconfirmed. Harry
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 06 - NOT KNOWN YET
	{ 0, 1, 2, 3, 4, 5, 6, 7 }, // 07 - NOT KNOWN YET
};

void  _GBBBD(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	switch (address & 0xF0FF) {
	case 0x2000:
		value = _reorderBits(value, _bbdBankReordering[memory->mbcState.bbd.bankSwapMode]);
		break;
	case 0x2001:
		memory->mbcState.bbd.dataSwapMode = value & 0x07;
		if (!(memory->mbcState.bbd.dataSwapMode == 0x07 || memory->mbcState.bbd.dataSwapMode == 0x05 || memory->mbcState.bbd.dataSwapMode == 0x04 || memory->mbcState.bbd.dataSwapMode == 0x00)) {
			mLOG(GB_MBC, STUB, "Bitswap mode unsupported: %X", memory->mbcState.bbd.dataSwapMode);
		}
		break;
	case 0x2080:
		memory->mbcState.bbd.bankSwapMode = value & 0x07;
		if (!(memory->mbcState.bbd.bankSwapMode == 0x03 || memory->mbcState.bbd.bankSwapMode == 0x05 || memory->mbcState.bbd.bankSwapMode == 0x00)) {
			mLOG(GB_MBC, STUB, "Bankswap mode unsupported: %X", memory->mbcState.bbd.dataSwapMode);
		}
		break;
	}
	_GBMBC5(gb, address, value);
}

uint8_t _GBBBDRead(struct GBMemory* memory, uint16_t address) {
	switch (address >> 14) {
	case 0:
	default:
		return memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
	case 1:
		return _reorderBits(memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)], _bbdDataReordering[memory->mbcState.bbd.dataSwapMode]);
	}
}

static const uint8_t _hitekDataReordering[8][8] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0, 6, 5, 3, 4, 1, 2, 7 },
	{ 0, 5, 6, 3, 4, 2, 1, 7 },
	{ 0, 6, 2, 3, 4, 5, 1, 7 },
	{ 0, 6, 1, 3, 4, 5, 2, 7 },
	{ 0, 1, 6, 3, 4, 5, 2, 7 },
	{ 0, 2, 6, 3, 4, 1, 5, 7 },
	{ 0, 6, 2, 3, 4, 1, 5, 7 },
};

static const uint8_t _hitekBankReordering[8][8] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 3, 2, 1, 0, 4, 5, 6, 7 },
	{ 2, 1, 0, 3, 4, 5, 6, 7 },
	{ 1, 0, 3, 2, 4, 5, 6, 7 },
	{ 0, 3, 2, 1, 4, 5, 6, 7 },
	{ 2, 3, 0, 1, 4, 5, 6, 7 },
	{ 3, 0, 1, 2, 4, 5, 6, 7 },
	{ 2, 0, 3, 1, 4, 5, 6, 7 },
};

void  _GBHitek(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	switch (address & 0xF0FF) {
	case 0x2000:
		value = _reorderBits(value, _hitekBankReordering[memory->mbcState.bbd.bankSwapMode]);
		break;
	case 0x2001:
		memory->mbcState.bbd.dataSwapMode = value & 0x07;
		break;
	case 0x2080:
		memory->mbcState.bbd.bankSwapMode = value & 0x07;
		break;
	case 0x300:
		// See hhugboy src/memory/mbc/MbcUnlHitek.cpp for commentary on this return
		return;	
	}
	_GBMBC5(gb, address, value);
}

uint8_t _GBHitekRead(struct GBMemory* memory, uint16_t address) {
	switch (address >> 14) {
	case 0:
	default:
		return memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
	case 1:
		return _reorderBits(memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)], _hitekDataReordering[memory->mbcState.bbd.dataSwapMode]);
	}
}

static const uint8_t _ggb81DataReordering[8][8] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0, 2, 1, 3, 4, 6, 5, 7 },
	{ 0, 6, 5, 3, 4, 2, 1, 7 },
	{ 0, 5, 1, 3, 4, 2, 6, 7 },
	{ 0, 5, 2, 3, 4, 1, 6, 7 },
	{ 0, 2, 6, 3, 4, 5, 1, 7 },
	{ 0, 1, 6, 3, 4, 2, 5, 7 },
	{ 0, 2, 5, 3, 4, 6, 1, 7 },
};

void  _GBGGB81(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBMemory* memory = &gb->memory;
	switch (address & 0xF0FF) {
	case 0x2001:
		memory->mbcState.bbd.dataSwapMode = value & 0x07;
		break;
	}
	_GBMBC5(gb, address, value);
}

uint8_t _GBGGB81Read(struct GBMemory* memory, uint16_t address) {
	switch (address >> 14) {
	case 0:
	default:
		return memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
	case 1:
		return _reorderBits(memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)], _ggb81DataReordering[memory->mbcState.bbd.dataSwapMode]);
	}
}

void  _GBLiCheng(struct GB* gb, uint16_t address, uint8_t value) {
    if (address > 0x2100 && address < 0x3000) {
        return;
    }
    _GBMBC5(gb, address, value);
}

void _GBSachen(struct GB* gb, uint16_t address, uint8_t value) {
	struct GBSachenState* state = &gb->memory.mbcState.sachen;
	uint8_t bank = value;
	switch (address >> 13) {
	case 0:
		if ((state->unmaskedBank & 0x30) == 0x30) {
			state->baseBank = bank;
			GBMBCSwitchBank0(gb, state->baseBank & state->mask);
		}
		break;
	case 1:
		if (!bank) {
			bank = 1;
		}
		state->unmaskedBank = bank;
		bank = (bank & ~state->mask) | (state->baseBank & state->mask);
		GBMBCSwitchBank(gb, bank);
		break;
	case 2:
		if ((state->unmaskedBank & 0x30) == 0x30) {
			state->mask = value;
			bank = (state->unmaskedBank & ~state->mask) | (state->baseBank & state->mask);
			GBMBCSwitchBank(gb, bank);
			GBMBCSwitchBank0(gb, state->baseBank & state->mask);
		}
		break;
	case 6:
		if (gb->memory.mbcType == GB_UNL_SACHEN_MMC2 && state->locked == GB_SACHEN_LOCKED_DMG) {
			state->locked = GB_SACHEN_LOCKED_CGB;
			state->transition = 0;
		}
		break;
	}
}

static uint16_t _unscrambleSachen(uint16_t address) {
	uint16_t unscrambled = address & 0xFFAC;
	unscrambled |= (address & 0x40) >> 6;
	unscrambled |= (address & 0x10) >> 3;
	unscrambled |= (address & 0x02) << 3;
	unscrambled |= (address & 0x01) << 6;
	return unscrambled;
}

uint8_t _GBSachenMMC1Read(struct GBMemory* memory, uint16_t address) {
	struct GBSachenState* state = &memory->mbcState.sachen;
	if (state->locked != GB_SACHEN_UNLOCKED && (address & 0xFF00) == 0x100) {
		++state->transition;
		if (state->transition == 0x31) {
			state->locked = GB_SACHEN_UNLOCKED;
		} else {
			address |= 0x80;
		}
	}

	if ((address & 0xFF00) == 0x0100) {
		address = _unscrambleSachen(address);
	}

	if (address < GB_BASE_CART_BANK1) {
		return memory->romBase[address];
	} else if (address < GB_BASE_VRAM) {
		return memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
	} else {
		return 0xFF;
	}
}

uint8_t _GBSachenMMC2Read(struct GBMemory* memory, uint16_t address) {
	struct GBSachenState* state = &memory->mbcState.sachen;
	if (address >= 0xC000 && state->locked == GB_SACHEN_LOCKED_DMG) {
		state->transition = 0;
		state->locked = GB_SACHEN_LOCKED_CGB;
	}

	if (state->locked != GB_SACHEN_UNLOCKED && (address & 0x8700) == 0x0100) {
		++state->transition;
		if (state->transition == 0x31) {
			++state->locked;
			state->transition = 0;
		}
	}

	if ((address & 0xFF00) == 0x0100) {
		if (state->locked == GB_SACHEN_LOCKED_CGB) {
			address |= 0x80;
		}
		address = _unscrambleSachen(address);
	}

	if (address < GB_BASE_CART_BANK1) {
		return memory->romBase[address];
	} else if (address < GB_BASE_VRAM) {
		return memory->romBank[address & (GB_SIZE_CART_BANK0 - 1)];
	} else {
		return 0xFF;
	}
}
