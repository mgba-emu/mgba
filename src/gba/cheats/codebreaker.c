/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/cheats.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba-util/string.h>

static void _cbLoadByteswap(uint8_t* buffer, uint32_t op1, uint16_t op2) {
	buffer[0] = op1 >> 24;
	buffer[1] = op1 >> 16;
	buffer[2] = op1 >> 8;
	buffer[3] = op1;
	buffer[4] = op2 >> 8;
	buffer[5] = op2;
}

static void _cbStoreByteswap(uint8_t* buffer, uint32_t* op1, uint16_t* op2) {
	*op1 = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
	*op2 = (buffer[4] << 8) | buffer[5];
}

static void _cbDecrypt(struct GBACheatSet* cheats, uint32_t* op1, uint16_t* op2) {
	uint8_t buffer[6];
	int i;

	_cbLoadByteswap(buffer, *op1, *op2);
	for (i = sizeof(cheats->cbTable) - 1; i >= 0; --i) {
		size_t offsetX = i >> 3;
		size_t offsetY = cheats->cbTable[i] >> 3;
		int bitX = i & 7;
		int bitY = cheats->cbTable[i] & 7;

		uint8_t x = (buffer[offsetX] >> bitX) & 1;
		uint8_t y = (buffer[offsetY] >> bitY) & 1;
		uint8_t x2 = buffer[offsetX] & ~(1 << bitX);
		if (y) {
			x2 |= 1 << bitX;
		}
		buffer[offsetX] = x2;

		// This can't be moved earlier due to pointer aliasing
		uint8_t y2 = buffer[offsetY] & ~(1 << bitY);
		if (x) {
			y2 |= 1 << bitY;
		}
		buffer[offsetY] = y2;
	}
	_cbStoreByteswap(buffer, op1, op2);

	*op1 ^= cheats->cbSeeds[0];
	*op2 ^= cheats->cbSeeds[1];

	_cbLoadByteswap(buffer, *op1, *op2);
	uint32_t master = cheats->cbMaster;
	for (i = 0; i < 5; ++i) {
		buffer[i] ^= (master >> 8) ^ buffer[i + 1];
	}
	buffer[5] ^= master >> 8;

	for (i = 5; i > 0; --i) {
		buffer[i] ^= master ^ buffer[i - 1];
	}
	buffer[0] ^= master;
	_cbStoreByteswap(buffer, op1, op2);

	*op1 ^= cheats->cbSeeds[2];
	*op2 ^= cheats->cbSeeds[3];
}

static uint32_t _cbRand(struct GBACheatSet* cheats) {
	// Roll LCG three times to get enough bits of entropy
	uint32_t roll = cheats->cbRngState * 0x41C64E6D + 0x3039;
	uint32_t roll2 = roll * 0x41C64E6D + 0x3039;
	uint32_t roll3 = roll2 * 0x41C64E6D + 0x3039;
	uint32_t mix = (roll << 14) & 0xC0000000;
	mix |= (roll2 >> 1) & 0x3FFF8000;
	mix |= (roll3 >> 16) & 0x7FFF;
	cheats->cbRngState = roll3;
	return mix;
}

static size_t _cbSwapIndex(struct GBACheatSet* cheats) {
	uint32_t roll = _cbRand(cheats);
	uint32_t count = sizeof(cheats->cbTable);

	if (roll == count) {
		roll = 0;
	}

	if (roll < count) {
		return roll;
	}

	uint32_t bit = 1;

	while (count < 0x10000000 && count < roll) {
		count <<= 4;
		bit <<= 4;
	}

	while (count < 0x80000000 && count < roll) {
		count <<= 1;
		bit <<= 1;
	}

	uint32_t mask;
	while (true) {
		mask = 0;
		if (roll >= count) {
			roll -= count;
		}
		if (roll >= count >> 1) {
			roll -= count >> 1;
			mask |= ROR(bit, 1);
		}
		if (roll >= count >> 2) {
			roll -= count >> 2;
			mask |= ROR(bit, 2);
		}
		if (roll >= count >> 3) {
			roll -= count >> 3;
			mask |= ROR(bit, 3);
		}
		if (!roll || !(bit >> 4)) {
			break;
		}
		bit >>= 4;
		count >>= 4;
	}

	mask &= 0xE0000000;
	if (!mask || !(bit & 7)) {
		return roll;
	}

	if (mask & ROR(bit, 3)) {
		roll += count >> 3;
	}
	if (mask & ROR(bit, 2)) {
		roll += count >> 2;
	}
	if (mask & ROR(bit, 1)) {
		roll += count >> 1;
	}

	return roll;
}

static void _cbReseed(struct GBACheatSet* cheats, uint32_t op1, uint16_t op2) {
	cheats->cbRngState = (op2 & 0xFF) ^ 0x1111;
	size_t i;
	// Populate the initial seed table
	for (i = 0; i < sizeof(cheats->cbTable); ++i) {
		cheats->cbTable[i] = i;
	}
	// Swap pseudo-random table entries based on the input code
	for (i = 0; i < 0x50; ++i) {
		size_t x = _cbSwapIndex(cheats);
		size_t y = _cbSwapIndex(cheats);
		uint8_t swap = cheats->cbTable[x];
		cheats->cbTable[x] = cheats->cbTable[y];
		cheats->cbTable[y] = swap;
	}

	// Spin the RNG some to make the initial seed
	cheats->cbRngState = 0x4EFAD1C3;
	for (i = 0; i < ((op1 >> 24) & 0xF); ++i) {
		cheats->cbRngState = _cbRand(cheats);
	}

	cheats->cbSeeds[2] = _cbRand(cheats);
	cheats->cbSeeds[3] = _cbRand(cheats);

	cheats->cbRngState = (op2 >> 8) ^ 0xF254;
	for (i = 0; i < (op2 >> 8); ++i) {
		cheats->cbRngState = _cbRand(cheats);
	}

	cheats->cbSeeds[0] = _cbRand(cheats);
	cheats->cbSeeds[1] = _cbRand(cheats);

	cheats->cbMaster = op1;
}

bool GBACheatAddCodeBreaker(struct GBACheatSet* cheats, uint32_t op1, uint16_t op2) {
	char line[14] = "XXXXXXXX XXXX";
	snprintf(line, sizeof(line), "%08X %04X", op1, op2);

	if (cheats->cbMaster) {
		_cbDecrypt(cheats, &op1, &op2);
	}

	enum GBACodeBreakerType type = op1 >> 28;
	struct mCheat* cheat = NULL;

	if (cheats->incompleteCheat != COMPLETE) {
		struct mCheat* incompleteCheat = mCheatListGetPointer(&cheats->d.list, cheats->incompleteCheat);
		incompleteCheat->repeat = op1 & 0xFFFF;
		incompleteCheat->addressOffset = op2;
		incompleteCheat->operandOffset = op1 >> 16;
		cheats->incompleteCheat = COMPLETE;
		return true;
	}

	switch (type) {
	case CB_GAME_ID:
		// TODO: Run checksum
		return true;
	case CB_HOOK:
		if (cheats->hook) {
			return false;
		}
		cheats->hook = malloc(sizeof(*cheats->hook));
		cheats->hook->address = GBA_BASE_ROM0 | (op1 & (GBA_SIZE_ROM0 - 1));
		cheats->hook->mode = MODE_THUMB;
		cheats->hook->refs = 1;
		cheats->hook->reentries = 0;
		return true;
	case CB_OR_2:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_OR;
		cheat->width = 2;
		break;
	case CB_ASSIGN_1:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 1;
		break;
	case CB_FILL:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		cheats->incompleteCheat = mCheatListIndex(&cheats->d.list, cheat);
		break;
	case CB_FILL_8:
		mLOG(CHEATS, STUB, "CodeBreaker code %08X %04X not supported", op1, op2);
		return false;
	case CB_AND_2:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_AND;
		cheat->width = 2;
		break;
	case CB_IF_EQ:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_IF_EQ;
		cheat->width = 2;
		break;
	case CB_ASSIGN_2:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		break;
	case CB_ENCRYPT:
		_cbReseed(cheats, op1, op2);
		return true;
	case CB_IF_NE:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_IF_NE;
		cheat->width = 2;
		break;
	case CB_IF_GT:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_IF_GT;
		cheat->width = 2;
		break;
	case CB_IF_LT:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_IF_LT;
		cheat->width = 2;
		break;
	case CB_IF_SPECIAL:
		switch (op1 & 0x0FFFFFFF) {
		case 0x20:
			cheat = mCheatListAppend(&cheats->d.list);
			cheat->type = CHEAT_IF_NAND;
			cheat->width = 2;
			cheat->address = GBA_BASE_IO | REG_KEYINPUT;
			cheat->operand = op2;
			cheat->repeat = 1;
			return true;
		default:
			mLOG(CHEATS, STUB, "CodeBreaker code %08X %04X not supported", op1, op2);
			return false;
		}
	case CB_ADD_2:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ADD;
		cheat->width = 2;
		break;
	case CB_IF_AND:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_IF_AND;
		cheat->width = 2;
		break;
	}

	cheat->address = op1 & 0x0FFFFFFF;
	cheat->operand = op2;
	cheat->repeat = 1;
	cheat->negativeRepeat = 0;
	return true;
}

bool GBACheatAddCodeBreakerLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint16_t op2;
	line = hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (*line == ' ') {
		++line;
	}
	line = hex16(line, &op2);
	if (!line) {
		return false;
	}
	return GBACheatAddCodeBreaker(cheats, op1, op2);
}
