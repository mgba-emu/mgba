/* Copyright (c) 2016 taizou
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/cart/vfame.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/memory.h>

static const uint8_t ADDRESS_REORDERING[4][16] = {
	{ 15, 14, 9, 1, 8, 10, 7, 3, 5, 11, 4, 0, 13, 12, 2, 6 },
	{ 15, 7, 13, 5, 11, 6, 0, 9, 12, 2, 10, 14, 3, 1, 8, 4 },
	{ 15, 0, 3, 12, 2, 4, 14, 13, 1, 8, 6, 7, 9, 5, 11, 10 }
};
static const uint8_t ADDRESS_REORDERING_GEORGE[4][16] = {
	{ 15, 7, 13, 1, 11, 10, 14, 9, 12, 2, 4, 0, 3, 5, 8, 6 },
	{ 15, 14, 3, 12, 8, 4, 0, 13, 5, 11, 6, 7, 9, 1, 2, 10 },
	{ 15, 0, 9, 5, 2, 6, 7, 3, 1, 8, 10, 14, 13, 12, 11, 4 }
};
static const uint8_t VALUE_REORDERING[4][16] = {
	{ 5, 4, 3, 2, 1, 0, 7, 6 },
	{ 3, 2, 1, 0, 7, 6, 5, 4 },
	{ 1, 0, 7, 6, 5, 4, 3, 2 }
};
static const uint8_t VALUE_REORDERING_GEORGE[4][16] = {
	{ 3, 0, 7, 2, 1, 4, 5, 6 },
	{ 1, 4, 3, 0, 5, 6, 7, 2 },
	{ 5, 2, 1, 6, 7, 0, 3, 4 }
};

static const int8_t MODE_CHANGE_START_SEQUENCE[5] = { 0x99, 0x02, 0x05, 0x02, 0x03 };
static const int8_t MODE_CHANGE_END_SEQUENCE[5] = { 0x99, 0x03, 0x62, 0x02, 0x56 };

// A portion of the initialisation routine that gets copied into RAM - Always seems to be present at 0x15C in VFame game ROM
static const char INIT_SEQUENCE[16] = { 0xB4, 0x00, 0x9F, 0xE5, 0x99, 0x10, 0xA0, 0xE3, 0x00, 0x10, 0xC0, 0xE5, 0xAC, 0x00, 0x9F, 0xE5 };

static bool _isInMirroredArea(uint32_t address, size_t romSize);
static uint32_t _getPatternValue(uint32_t addr);
static uint32_t _patternRightShift2(uint32_t addr);
static int8_t _modifySramValue(enum GBAVFameCartType type, uint8_t value, int mode);
static uint32_t _modifySramAddress(enum GBAVFameCartType type, uint32_t address, int mode);
static int _reorderBits(uint32_t value, const uint8_t* reordering, int reorderLength);

void GBAVFameInit(struct GBAVFameCart* cart) {
	cart->cartType = VFAME_NO;
	cart->sramMode = -1;
	cart->romMode = -1;
	cart->acceptingModeChange = false;
}

void GBAVFameDetect(struct GBAVFameCart* cart, uint32_t* rom, size_t romSize) {
	cart->cartType = VFAME_NO;

	// The initialisation code is also present & run in the dumps of Digimon Ruby & Sapphire from hacked/deprotected reprint carts,
	// which would break if run in "proper" VFame mode so we need to exclude those..
	if (romSize == 0x2000000) { // the deprotected dumps are 32MB but no real VF games are this size
		return;
	}

	// Most games have the same init sequence in the same place
	// but LOTR/Mo Jie Qi Bing doesn't, probably because it's based on the Kiki KaiKai engine, so just detect based on its title
	if (memcmp(INIT_SEQUENCE, &rom[0x57], sizeof(INIT_SEQUENCE)) == 0 || memcmp("\0LORD\0WORD\0\0AKIJ", &((struct GBACartridge*) rom)->title, 16) == 0) {
		cart->cartType = VFAME_STANDARD;
		mLOG(GBA_MEM, INFO, "Vast Fame game detected");
	}

	// This game additionally operates with a different set of SRAM modes
	// Its initialisation seems to be identical so the difference must be in the cart HW itself
	// Other undumped games may have similar differences
	if (memcmp("George Sango", &((struct GBACartridge*) rom)->title, 12) == 0) {
		cart->cartType = VFAME_GEORGE;
		mLOG(GBA_MEM, INFO, "George mode");
	}
}

// This is not currently being used but would be called on ROM reads
// Emulates mirroring used by real VF carts, but no games seem to rely on this behaviour
uint32_t GBAVFameModifyRomAddress(struct GBAVFameCart* cart, uint32_t address, size_t romSize) {
	if (cart->romMode == -1 && (address & 0x01000000) == 0) {
		// When ROM mode is uninitialised, it just mirrors the first 0x80000 bytes
		// All known games set the ROM mode to 00 which enables full range of reads, it's currently unknown what other values do
		address &= 0x7FFFF;
	} else if (_isInMirroredArea(address, romSize)) {
		address -= 0x800000;
	}
	return address;
}

static bool _isInMirroredArea(uint32_t address, size_t romSize) {
	address &= 0x01FFFFFF;
	// For some reason known 4m games e.g. Zook, Sango repeat the game at 800000 but the 8m Digimon R. does not
	if (romSize != 0x400000) {
		return false;
	}
	if (address < 0x800000) {
		return false;
	}
	if (address >= 0x800000 + romSize) {
		return false;
	}
	return true;
}

// Looks like only 16-bit reads are done by games but others are possible...
uint32_t GBAVFameGetPatternValue(uint32_t address, int bits) {
	switch (bits) {
	case 8:
		if (address & 1) {
			return _getPatternValue(address) & 0xFF;
		} else {
			return (_getPatternValue(address) & 0xFF00) >> 8;
		}
	case 16:
		return _getPatternValue(address);
	case 32:
		return (_getPatternValue(address) << 2) + _getPatternValue(address + 2);
	}
	return 0;
}

// when you read from a ROM location outside the actual ROM data or its mirror, it returns a value based on some 16-bit transformation of the address
// which the game relies on to run
static uint32_t _getPatternValue(uint32_t addr) {
	addr &= 0x1FFFFF;
	uint32_t value = 0;
	switch (addr & 0x1F0000) {
	case 0x000000:
	case 0x010000:
		value = (addr >> 1) & 0xFFFF;
		break;
	case 0x020000:
		value = addr & 0xFFFF;
		break;
	case 0x030000:
		value = (addr & 0xFFFF) + 1;
		break;
	case 0x040000:
		value = 0xFFFF - (addr & 0xFFFF);
		break;
	case 0x050000:
		value = (0xFFFF - (addr & 0xFFFF)) - 1;
		break;
	case 0x060000:
		value = (addr & 0xFFFF) ^ 0xAAAA;
		break;
	case 0x070000:
		value = ((addr & 0xFFFF) ^ 0xAAAA) + 1;
		break;
	case 0x080000:
		value = (addr & 0xFFFF) ^ 0x5555;
		break;
	case 0x090000:
		value = ((addr & 0xFFFF) ^ 0x5555) - 1;
		break;
	case 0x0A0000:
	case 0x0B0000:
		value = _patternRightShift2(addr);
		break;
	case 0x0C0000:
	case 0x0D0000:
		value = 0xFFFF - _patternRightShift2(addr);
		break;
	case 0x0E0000:
	case 0x0F0000:
		value = _patternRightShift2(addr) ^ 0xAAAA;
		break;
	case 0x100000:
	case 0x110000:
		value = _patternRightShift2(addr) ^ 0x5555;
		break;
	case 0x120000:
		value = 0xFFFF - ((addr & 0xFFFF) >> 1);
		break;
	case 0x130000:
		value = 0xFFFF - ((addr & 0xFFFF) >> 1) - 0x8000;
		break;
	case 0x140000:
	case 0x150000:
		value = ((addr >> 1) & 0xFFFF) ^ 0xAAAA;
		break;
	case 0x160000:
	case 0x170000:
		value = ((addr >> 1) & 0xFFFF) ^ 0x5555;
		break;
	case 0x180000:
	case 0x190000:
		value = ((addr >> 1) & 0xFFFF) ^ 0xF0F0;
		break;
	case 0x1A0000:
	case 0x1B0000:
		value = ((addr >> 1) & 0xFFFF) ^ 0x0F0F;
		break;
	case 0x1C0000:
	case 0x1D0000:
		value = ((addr >> 1) & 0xFFFF) ^ 0xFF00;
		break;
	case 0x1E0000:
	case 0x1F0000:
		value = ((addr >> 1) & 0xFFFF) ^ 0x00FF;
		break;
	}

	return value & 0xFFFF;
}

static uint32_t _patternRightShift2(uint32_t addr) {
	uint32_t value = addr & 0xFFFF;
	value >>= 2;
	value += (addr & 3) == 2 ? 0x8000 : 0;
	value += (addr & 0x10000) ? 0x4000 : 0;
	return value;
}

void GBAVFameSramWrite(struct GBAVFameCart* cart, uint32_t address, uint8_t value, uint8_t* sramData) {
	address &= 0x00FFFFFF;
	// A certain sequence of writes to SRAM FFF8->FFFC can enable or disable "mode change" mode
	// Currently unknown if these writes have to be sequential, or what happens if you write different values, if anything
	if (address >= 0xFFF8 && address <= 0xFFFC) {
		cart->writeSequence[address - 0xFFF8] = value;
		if (address == 0xFFFC) {
			if (memcmp(MODE_CHANGE_START_SEQUENCE, cart->writeSequence, sizeof(MODE_CHANGE_START_SEQUENCE)) == 0) {
				cart->acceptingModeChange = true;
			}
			if (memcmp(MODE_CHANGE_END_SEQUENCE, cart->writeSequence, sizeof(MODE_CHANGE_END_SEQUENCE)) == 0) {
				cart->acceptingModeChange = false;
			}
		}
	}

	// If we are in "mode change mode" we can change either SRAM or ROM modes
	// Currently unknown if other SRAM writes in this mode should have any effect
	if (cart->acceptingModeChange) {
		if (address == 0xFFFE) {
			cart->sramMode = value;
		} else if (address == 0xFFFD) {
			cart->romMode = value;
		}
	}

	if (cart->sramMode == -1) {
		// when SRAM mode is uninitialised you can't write to it
		return;
	}

	// if mode has been set - the address and value of the SRAM write will be modified
	address = _modifySramAddress(cart->cartType, address, cart->sramMode);
	value = _modifySramValue(cart->cartType, value, cart->sramMode);
	address &= (GBA_SIZE_SRAM - 1);
	sramData[address] = value;
}

static uint32_t _modifySramAddress(enum GBAVFameCartType type, uint32_t address, int mode) {
	mode &= 0x3;
	if (mode == 0) {
		return address;
	} else if (type == VFAME_GEORGE) {
		return _reorderBits(address, ADDRESS_REORDERING_GEORGE[mode - 1], 16);
	} else {
		return _reorderBits(address, ADDRESS_REORDERING[mode - 1], 16);
	}
}

static int8_t _modifySramValue(enum GBAVFameCartType type, uint8_t value, int mode) {
	int reorderType = (mode & 0xF) >> 2;
	if (reorderType != 0) {
		if (type == VFAME_GEORGE) {
			value = _reorderBits(value, VALUE_REORDERING_GEORGE[reorderType - 1], 8);
		} else {
			value = _reorderBits(value, VALUE_REORDERING[reorderType - 1], 8);
		}
	}
	if (mode & 0x80) {
		value ^= 0xAA;
	}
	return value;
}

// Reorder bits in a byte according to the reordering given
static int _reorderBits(uint32_t value, const uint8_t* reordering, int reorderLength) {
	uint32_t retval = value;

	int x;
	for (x = reorderLength; x > 0; x--) {
		uint8_t reorderPlace = reordering[reorderLength - x]; // get the reorder position

		uint32_t mask = 1 << reorderPlace; // move the bit to the position we want
		uint32_t val = value & mask; // AND it with the original value
		val >>= reorderPlace; // move the bit back, so we have the correct 0 or 1

		unsigned destinationPlace = x - 1;

		uint32_t newMask = 1 << destinationPlace;
		if (val == 1) {
			retval |= newMask;
		} else {
			retval &= ~newMask;
		}
	}

	return retval;
}
