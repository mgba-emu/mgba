/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "cheats.h"

#include "gba/gba.h"
#include "gba/io.h"
#include "util/vfs.h"

#define MAX_LINE_LENGTH 128

const uint32_t GBA_CHEAT_DEVICE_ID = 0xABADC0DE;

DEFINE_VECTOR(GBACheatList, struct GBACheat);
DEFINE_VECTOR(GBACheatSets, struct GBACheatSet*);
DEFINE_VECTOR(StringList, char*);

static const uint32_t _gsa1S[4] = { 0x09F4FBBD, 0x9681884A, 0x352027E9, 0xF3DEE5A7 };
static const uint32_t _par3S[4] = { 0x7AA9648F, 0x7FAE6994, 0xC0EFAAD5, 0x42712C57 };

static const uint8_t _gsa1T1[256] = {
	0x31, 0x1C, 0x23, 0xE5, 0x89, 0x8E, 0xA1, 0x37, 0x74, 0x6D, 0x67, 0xFC, 0x1F, 0xC0, 0xB1, 0x94,
	0x3B, 0x05, 0x56, 0x86, 0x00, 0x24, 0xF0, 0x17, 0x72, 0xA2, 0x3D, 0x1B, 0xE3, 0x17, 0xC5, 0x0B,
	0xB9, 0xE2, 0xBD, 0x58, 0x71, 0x1B, 0x2C, 0xFF, 0xE4, 0xC9, 0x4C, 0x5E, 0xC9, 0x55, 0x33, 0x45,
	0x7C, 0x3F, 0xB2, 0x51, 0xFE, 0x10, 0x7E, 0x75, 0x3C, 0x90, 0x8D, 0xDA, 0x94, 0x38, 0xC3, 0xE9,
	0x95, 0xEA, 0xCE, 0xA6, 0x06, 0xE0, 0x4F, 0x3F, 0x2A, 0xE3, 0x3A, 0xE4, 0x43, 0xBD, 0x7F, 0xDA,
	0x55, 0xF0, 0xEA, 0xCB, 0x2C, 0xA8, 0x47, 0x61, 0xA0, 0xEF, 0xCB, 0x13, 0x18, 0x20, 0xAF, 0x3E,
	0x4D, 0x9E, 0x1E, 0x77, 0x51, 0xC5, 0x51, 0x20, 0xCF, 0x21, 0xF9, 0x39, 0x94, 0xDE, 0xDD, 0x79,
	0x4E, 0x80, 0xC4, 0x9D, 0x94, 0xD5, 0x95, 0x01, 0x27, 0x27, 0xBD, 0x6D, 0x78, 0xB5, 0xD1, 0x31,
	0x6A, 0x65, 0x74, 0x74, 0x58, 0xB3, 0x7C, 0xC9, 0x5A, 0xED, 0x50, 0x03, 0xC4, 0xA2, 0x94, 0x4B,
	0xF0, 0x58, 0x09, 0x6F, 0x3E, 0x7D, 0xAE, 0x7D, 0x58, 0xA0, 0x2C, 0x91, 0xBB, 0xE1, 0x70, 0xEB,
	0x73, 0xA6, 0x9A, 0x44, 0x25, 0x90, 0x16, 0x62, 0x53, 0xAE, 0x08, 0xEB, 0xDC, 0xF0, 0xEE, 0x77,
	0xC2, 0xDE, 0x81, 0xE8, 0x30, 0x89, 0xDB, 0xFE, 0xBC, 0xC2, 0xDF, 0x26, 0xE9, 0x8B, 0xD6, 0x93,
	0xF0, 0xCB, 0x56, 0x90, 0xC0, 0x46, 0x68, 0x15, 0x43, 0xCB, 0xE9, 0x98, 0xE3, 0xAF, 0x31, 0x25,
	0x4D, 0x7B, 0xF3, 0xB1, 0x74, 0xE2, 0x64, 0xAC, 0xD9, 0xF6, 0xA0, 0xD5, 0x0B, 0x9B, 0x49, 0x52,
	0x69, 0x3B, 0x71, 0x00, 0x2F, 0xBB, 0xBA, 0x08, 0xB1, 0xAE, 0xBB, 0xB3, 0xE1, 0xC9, 0xA6, 0x7F,
	0x17, 0x97, 0x28, 0x72, 0x12, 0x6E, 0x91, 0xAE, 0x3A, 0xA2, 0x35, 0x46, 0x27, 0xF8, 0x12, 0x50
};

static const uint8_t _gsa1T2[256] = {
	0xD8, 0x65, 0x04, 0xC2, 0x65, 0xD5, 0xB0, 0x0C, 0xDF, 0x9D, 0xF0, 0xC3, 0x9A, 0x17, 0xC9, 0xA6,
	0xE1, 0xAC, 0x0D, 0x14, 0x2F, 0x3C, 0x2C, 0x87, 0xA2, 0xBF, 0x4D, 0x5F, 0xAC, 0x2D, 0x9D, 0xE1,
	0x0C, 0x9C, 0xE7, 0x7F, 0xFC, 0xA8, 0x66, 0x59, 0xAC, 0x18, 0xD7, 0x05, 0xF0, 0xBF, 0xD1, 0x8B,
	0x35, 0x9F, 0x59, 0xB4, 0xBA, 0x55, 0xB2, 0x85, 0xFD, 0xB1, 0x72, 0x06, 0x73, 0xA4, 0xDB, 0x48,
	0x7B, 0x5F, 0x67, 0xA5, 0x95, 0xB9, 0xA5, 0x4A, 0xCF, 0xD1, 0x44, 0xF3, 0x81, 0xF5, 0x6D, 0xF6,
	0x3A, 0xC3, 0x57, 0x83, 0xFA, 0x8E, 0x15, 0x2A, 0xA2, 0x04, 0xB2, 0x9D, 0xA8, 0x0D, 0x7F, 0xB8,
	0x0F, 0xF6, 0xAC, 0xBE, 0x97, 0xCE, 0x16, 0xE6, 0x31, 0x10, 0x60, 0x16, 0xB5, 0x83, 0x45, 0xEE,
	0xD7, 0x5F, 0x2C, 0x08, 0x58, 0xB1, 0xFD, 0x7E, 0x79, 0x00, 0x34, 0xAD, 0xB5, 0x31, 0x34, 0x39,
	0xAF, 0xA8, 0xDD, 0x52, 0x6A, 0xB0, 0x60, 0x35, 0xB8, 0x1D, 0x52, 0xF5, 0xF5, 0x30, 0x00, 0x7B,
	0xF4, 0xBA, 0x03, 0xCB, 0x3A, 0x84, 0x14, 0x8A, 0x6A, 0xEF, 0x21, 0xBD, 0x01, 0xD8, 0xA0, 0xD4,
	0x43, 0xBE, 0x23, 0xE7, 0x76, 0x27, 0x2C, 0x3F, 0x4D, 0x3F, 0x43, 0x18, 0xA7, 0xC3, 0x47, 0xA5,
	0x7A, 0x1D, 0x02, 0x55, 0x09, 0xD1, 0xFF, 0x55, 0x5E, 0x17, 0xA0, 0x56, 0xF4, 0xC9, 0x6B, 0x90,
	0xB4, 0x80, 0xA5, 0x07, 0x22, 0xFB, 0x22, 0x0D, 0xD9, 0xC0, 0x5B, 0x08, 0x35, 0x05, 0xC1, 0x75,
	0x4F, 0xD0, 0x51, 0x2D, 0x2E, 0x5E, 0x69, 0xE7, 0x3B, 0xC2, 0xDA, 0xFF, 0xF6, 0xCE, 0x3E, 0x76,
	0xE8, 0x36, 0x8C, 0x39, 0xD8, 0xF3, 0xE9, 0xA6, 0x42, 0xE6, 0xC1, 0x4C, 0x05, 0xBE, 0x17, 0xF2,
	0x5C, 0x1B, 0x19, 0xDB, 0x0F, 0xF3, 0xF8, 0x49, 0xEB, 0x36, 0xF6, 0x40, 0x6F, 0xAD, 0xC1, 0x8C
};

static const uint8_t _par3T1[256] = {
	0xD0, 0xFF, 0xBA, 0xE5, 0xC1, 0xC7, 0xDB, 0x5B, 0x16, 0xE3, 0x6E, 0x26, 0x62, 0x31, 0x2E, 0x2A,
	0xD1, 0xBB, 0x4A, 0xE6, 0xAE, 0x2F, 0x0A, 0x90, 0x29, 0x90, 0xB6, 0x67, 0x58, 0x2A, 0xB4, 0x45,
	0x7B, 0xCB, 0xF0, 0x73, 0x84, 0x30, 0x81, 0xC2, 0xD7, 0xBE, 0x89, 0xD7, 0x4E, 0x73, 0x5C, 0xC7,
	0x80, 0x1B, 0xE5, 0xE4, 0x43, 0xC7, 0x46, 0xD6, 0x6F, 0x7B, 0xBF, 0xED, 0xE5, 0x27, 0xD1, 0xB5,
	0xD0, 0xD8, 0xA3, 0xCB, 0x2B, 0x30, 0xA4, 0xF0, 0x84, 0x14, 0x72, 0x5C, 0xFF, 0xA4, 0xFB, 0x54,
	0x9D, 0x70, 0xE2, 0xFF, 0xBE, 0xE8, 0x24, 0x76, 0xE5, 0x15, 0xFB, 0x1A, 0xBC, 0x87, 0x02, 0x2A,
	0x58, 0x8F, 0x9A, 0x95, 0xBD, 0xAE, 0x8D, 0x0C, 0xA5, 0x4C, 0xF2, 0x5C, 0x7D, 0xAD, 0x51, 0xFB,
	0xB1, 0x22, 0x07, 0xE0, 0x29, 0x7C, 0xEB, 0x98, 0x14, 0xC6, 0x31, 0x97, 0xE4, 0x34, 0x8F, 0xCC,
	0x99, 0x56, 0x9F, 0x78, 0x43, 0x91, 0x85, 0x3F, 0xC2, 0xD0, 0xD1, 0x80, 0xD1, 0x77, 0xA7, 0xE2,
	0x43, 0x99, 0x1D, 0x2F, 0x8B, 0x6A, 0xE4, 0x66, 0x82, 0xF7, 0x2B, 0x0B, 0x65, 0x14, 0xC0, 0xC2,
	0x1D, 0x96, 0x78, 0x1C, 0xC4, 0xC3, 0xD2, 0xB1, 0x64, 0x07, 0xD7, 0x6F, 0x02, 0xE9, 0x44, 0x31,
	0xDB, 0x3C, 0xEB, 0x93, 0xED, 0x9A, 0x57, 0x05, 0xB9, 0x0E, 0xAF, 0x1F, 0x48, 0x11, 0xDC, 0x35,
	0x6C, 0xB8, 0xEE, 0x2A, 0x48, 0x2B, 0xBC, 0x89, 0x12, 0x59, 0xCB, 0xD1, 0x18, 0xEA, 0x72, 0x11,
	0x01, 0x75, 0x3B, 0xB5, 0x56, 0xF4, 0x8B, 0xA0, 0x41, 0x75, 0x86, 0x7B, 0x94, 0x12, 0x2D, 0x4C,
	0x0C, 0x22, 0xC9, 0x4A, 0xD8, 0xB1, 0x8D, 0xF0, 0x55, 0x2E, 0x77, 0x50, 0x1C, 0x64, 0x77, 0xAA,
	0x3E, 0xAC, 0xD3, 0x3D, 0xCE, 0x60, 0xCA, 0x5D, 0xA0, 0x92, 0x78, 0xC6, 0x51, 0xFE, 0xF9, 0x30
};

static const uint8_t _par3T2[256] = {
	0xAA, 0xAF, 0xF0, 0x72, 0x90, 0xF7, 0x71, 0x27, 0x06, 0x11, 0xEB, 0x9C, 0x37, 0x12, 0x72, 0xAA,
	0x65, 0xBC, 0x0D, 0x4A, 0x76, 0xF6, 0x5C, 0xAA, 0xB0, 0x7A, 0x7D, 0x81, 0xC1, 0xCE, 0x2F, 0x9F,
	0x02, 0x75, 0x38, 0xC8, 0xFC, 0x66, 0x05, 0xC2, 0x2C, 0xBD, 0x91, 0xAD, 0x03, 0xB1, 0x88, 0x93,
	0x31, 0xC6, 0xAB, 0x40, 0x23, 0x43, 0x76, 0x54, 0xCA, 0xE7, 0x00, 0x96, 0x9F, 0xD8, 0x24, 0x8B,
	0xE4, 0xDC, 0xDE, 0x48, 0x2C, 0xCB, 0xF7, 0x84, 0x1D, 0x45, 0xE5, 0xF1, 0x75, 0xA0, 0xED, 0xCD,
	0x4B, 0x24, 0x8A, 0xB3, 0x98, 0x7B, 0x12, 0xB8, 0xF5, 0x63, 0x97, 0xB3, 0xA6, 0xA6, 0x0B, 0xDC,
	0xD8, 0x4C, 0xA8, 0x99, 0x27, 0x0F, 0x8F, 0x94, 0x63, 0x0F, 0xB0, 0x11, 0x94, 0xC7, 0xE9, 0x7F,
	0x3B, 0x40, 0x72, 0x4C, 0xDB, 0x84, 0x78, 0xFE, 0xB8, 0x56, 0x08, 0x80, 0xDF, 0x20, 0x2F, 0xB9,
	0x66, 0x2D, 0x60, 0x63, 0xF5, 0x18, 0x15, 0x1B, 0x86, 0x85, 0xB9, 0xB4, 0x68, 0x0E, 0xC6, 0xD1,
	0x8A, 0x81, 0x2B, 0xB3, 0xF6, 0x48, 0xF0, 0x4F, 0x9C, 0x28, 0x1C, 0xA4, 0x51, 0x2F, 0xD7, 0x4B,
	0x17, 0xE7, 0xCC, 0x50, 0x9F, 0xD0, 0xD1, 0x40, 0x0C, 0x0D, 0xCA, 0x83, 0xFA, 0x5E, 0xCA, 0xEC,
	0xBF, 0x4E, 0x7C, 0x8F, 0xF0, 0xAE, 0xC2, 0xD3, 0x28, 0x41, 0x9B, 0xC8, 0x04, 0xB9, 0x4A, 0xBA,
	0x72, 0xE2, 0xB5, 0x06, 0x2C, 0x1E, 0x0B, 0x2C, 0x7F, 0x11, 0xA9, 0x26, 0x51, 0x9D, 0x3F, 0xF8,
	0x62, 0x11, 0x2E, 0x89, 0xD2, 0x9D, 0x35, 0xB1, 0xE4, 0x0A, 0x4D, 0x93, 0x01, 0xA7, 0xD1, 0x2D,
	0x00, 0x87, 0xE2, 0x2D, 0xA4, 0xE9, 0x0A, 0x06, 0x66, 0xF8, 0x1F, 0x44, 0x75, 0xB5, 0x6B, 0x1C,
	0xFC, 0x31, 0x09, 0x48, 0xA3, 0xFF, 0x92, 0x12, 0x58, 0xE9, 0xFA, 0xAE, 0x4F, 0xE2, 0xB4, 0xCC
};

static int32_t _readMem(struct ARMCore* cpu, uint32_t address, int width) {
	switch (width) {
	case 1:
		return cpu->memory.load8(cpu, address, 0);
	case 2:
		return cpu->memory.load16(cpu, address, 0);
	case 4:
		return cpu->memory.load32(cpu, address, 0);
	}
	return 0;
}

static void _writeMem(struct ARMCore* cpu, uint32_t address, int width, int32_t value) {
	switch (width) {
	case 1:
		cpu->memory.store8(cpu, address, value, 0);
		break;
	case 2:
		cpu->memory.store16(cpu, address, value, 0);
		break;
	case 4:
		cpu->memory.store32(cpu, address, value, 0);
		break;
	}
}

static int _hexDigit(char digit) {
	switch (digit) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return digit - '0';

	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
		return digit - 'a' + 10;

	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
		return digit - 'A' + 10;

	default:
		return -1;
	}
}

static const char* _hex32(const char* line, uint32_t* out) {
	uint32_t value = 0;
	int i;
	for (i = 0; i < 8; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = _hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

static const char* _hex16(const char* line, uint16_t* out) {
	uint16_t value = 0;
	int i;
	for (i = 0; i < 4; ++i, ++line) {
		char digit = *line;
		value <<= 4;
		int nybble = _hexDigit(digit);
		if (nybble < 0) {
			return 0;
		}
		value |= nybble;
	}
	*out = value;
	return line;
}

static void _registerLine(struct GBACheatSet* cheats, const char* line) {
	*StringListAppend(&cheats->lines) = strdup(line);
}

// http://en.wikipedia.org/wiki/Tiny_Encryption_Algorithm
static void _decryptGameShark(uint32_t* op1, uint32_t* op2, const uint32_t* seeds) {
	uint32_t sum = 0xC6EF3720;
	int i;
	for (i = 0; i < 32; ++i) {
		*op2 -= ((*op1 << 4) + seeds[2]) ^ (*op1 + sum) ^ ((*op1 >> 5) + seeds[3]);
		*op1 -= ((*op2 << 4) + seeds[0]) ^ (*op2 + sum) ^ ((*op2 >> 5) + seeds[1]);
		sum -= 0x9E3779B9;
	}
}

static void _reseedGameShark(uint32_t* seeds, uint16_t params, const uint8_t* t1, const uint8_t* t2) {
	int x, y;
	int s0 = params >> 8;
	int s1 = params & 0xFF;
	for (y = 0; y < 4; ++y) {
		for (x = 0; x < 4; ++x) {
			uint8_t z = t1[(s0 + x) & 0xFF] + t2[(s1 + y) & 0xFF];
			seeds[y] <<= 8;
			seeds[y] |= z;
		}
	}
}

static void _setGameSharkVersion(struct GBACheatSet* cheats, int version) {
	cheats->gsaVersion = 1;
	switch (version) {
	case 1:
		memcpy(cheats->gsaSeeds, _gsa1S, 4 * sizeof(uint32_t));
		break;
	case 3:
		memcpy(cheats->gsaSeeds, _par3S, 4 * sizeof(uint32_t));
		break;
	}
}

static bool _addGSA1(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2) {
	enum GBAGameSharkType type = op1 >> 28;
	struct GBACheat* cheat = 0;

	if (cheats->incompleteCheat) {
		if (cheats->remainingAddresses > 0) {
			cheat = GBACheatListAppend(&cheats->list);
			cheat->type = CHEAT_ASSIGN;
			cheat->width = 4;
			cheat->address = op1;
			cheat->operand = cheats->incompleteCheat->operand;
			cheat->repeat = 1;
			--cheats->remainingAddresses;
		}
		if (cheats->remainingAddresses > 0) {
			cheat = GBACheatListAppend(&cheats->list);
			cheat->type = CHEAT_ASSIGN;
			cheat->width = 4;
			cheat->address = op2;
			cheat->operand = cheats->incompleteCheat->operand;
			cheat->repeat = 1;
			--cheats->remainingAddresses;
		}
		if (cheats->remainingAddresses == 0) {
			cheats->incompleteCheat = 0;
		}
		return true;
	}

	switch (type) {
	case GSA_ASSIGN_1:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 1;
		cheat->address = op1 & 0x0FFFFFFF;
		break;
	case GSA_ASSIGN_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		cheat->address = op1 & 0x0FFFFFFF;
		break;
	case GSA_ASSIGN_4:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 4;
		cheat->address = op1 & 0x0FFFFFFF;
		break;
	case GSA_ASSIGN_LIST:
		cheats->remainingAddresses = (op1 & 0xFFFF) - 1;
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 4;
		cheat->address = op2;
		cheats->incompleteCheat = cheat;
		break;
	case GSA_PATCH:
		cheats->romPatches[0].address = (op1 & 0xFFFFFF) << 1;
		cheats->romPatches[0].newValue = op2;
		cheats->romPatches[0].applied = false;
		cheats->romPatches[0].exists = true;
		return true;
	case GSA_BUTTON:
		// TODO: Implement button
		return false;
	case GSA_IF_EQ:
		if (op1 == 0xDEADFACE) {
			_reseedGameShark(cheats->gsaSeeds, op2, _gsa1T1, _gsa1T2);
			return true;
		}
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_EQ;
		cheat->width = 2;
		cheat->address = op1 & 0x0FFFFFFF;
		break;
	case GSA_IF_EQ_RANGE:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_EQ;
		cheat->width = 2;
		cheat->address = op2 & 0x0FFFFFFF;
		cheat->operand = op1 & 0xFFFF;
		cheat->repeat = (op1 >> 16) & 0xFF;
		return true;
	case GSA_HOOK:
		if (cheats->hook) {
			return false;
		}
		cheats->hook = malloc(sizeof(*cheats->hook));
		cheats->hook->address = BASE_CART0 | (op1 & (SIZE_CART0 - 1));
		cheats->hook->mode = MODE_THUMB;
		cheats->hook->refs = 1;
		cheats->hook->reentries = 0;
		return true;
	default:
		return false;
	}
	cheat->operand = op2;
	cheat->repeat = 1;
	return true;
}

static bool _addGSA3(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2) {
	// TODO
	UNUSED(cheats);
	UNUSED(op1);
	UNUSED(op2);
	return false;
}

static void _addBreakpoint(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!device->p || !cheats->hook) {
		return;
	}
	++cheats->hook->reentries;
	if (cheats->hook->reentries > 1) {
		return;
	}
	GBASetBreakpoint(device->p, &device->d, cheats->hook->address, cheats->hook->mode, &cheats->hook->patchedOpcode);
}

static void _removeBreakpoint(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!device->p || !cheats->hook) {
		return;
	}
	--cheats->hook->reentries;
	if (cheats->hook->reentries > 0) {
		return;
	}
	GBAClearBreakpoint(device->p, cheats->hook->address, cheats->hook->mode, cheats->hook->patchedOpcode);
}

static void _patchROM(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!device->p) {
		return;
	}
	int i;
	for (i = 0; i < MAX_ROM_PATCHES; ++i) {
		if (!cheats->romPatches[i].exists || cheats->romPatches[i].applied) {
			continue;
		}
		GBAPatch16(device->p->cpu, cheats->romPatches[i].address, cheats->romPatches[i].newValue, &cheats->romPatches[i].oldValue);
		cheats->romPatches[i].applied = true;
	}
}

static void _unpatchROM(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!device->p) {
		return;
	}
	int i;
	for (i = 0; i < MAX_ROM_PATCHES; ++i) {
		if (!cheats->romPatches[i].exists || !cheats->romPatches[i].applied) {
			continue;
		}
		GBAPatch16(device->p->cpu, cheats->romPatches[i].address, cheats->romPatches[i].oldValue, 0);
		cheats->romPatches[i].applied = false;
	}
}

static void GBACheatDeviceInit(struct ARMCore*, struct ARMComponent*);
static void GBACheatDeviceDeinit(struct ARMComponent*);

void GBACheatDeviceCreate(struct GBACheatDevice* device) {
	device->d.id = GBA_CHEAT_DEVICE_ID;
	device->d.init = GBACheatDeviceInit;
	device->d.deinit = GBACheatDeviceDeinit;
	GBACheatSetsInit(&device->cheats, 4);
}

void GBACheatDeviceDestroy(struct GBACheatDevice* device) {
	size_t i;
	for (i = 0; i < GBACheatSetsSize(&device->cheats); ++i) {
		struct GBACheatSet* set = *GBACheatSetsGetPointer(&device->cheats, i);
		GBACheatSetDeinit(set);
		free(set);
	}
	GBACheatSetsDeinit(&device->cheats);
}

void GBACheatSetInit(struct GBACheatSet* set, const char* name) {
	GBACheatListInit(&set->list, 4);
	StringListInit(&set->lines, 4);
	set->incompleteCheat = 0;
	set->gsaVersion = 0;
	set->remainingAddresses = 0;
	set->hook = 0;
	int i;
	for (i = 0; i < MAX_ROM_PATCHES; ++i) {
		set->romPatches[i].exists = false;
	}
	if (name) {
		set->name = strdup(name);
	} else {
		set->name = 0;
	}
	set->enabled = true;
}

void GBACheatSetDeinit(struct GBACheatSet* set) {
	GBACheatListDeinit(&set->list);
	size_t i;
	for (i = 0; i < StringListSize(&set->lines); ++i) {
		free(*StringListGetPointer(&set->lines, i));
	}
	if (set->name) {
		free(set->name);
	}
	if (set->hook) {
		--set->hook->refs;
		if (set->hook->refs == 0) {
			free(set->hook);
		}
	}
}

void GBACheatAttachDevice(struct GBA* gba, struct GBACheatDevice* device) {
	if (gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE]) {
		ARMHotplugDetach(gba->cpu, GBA_COMPONENT_CHEAT_DEVICE);
	}
	gba->cpu->components[GBA_COMPONENT_CHEAT_DEVICE] = &device->d;
	ARMHotplugAttach(gba->cpu, GBA_COMPONENT_CHEAT_DEVICE);
}

void GBACheatAddSet(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	*GBACheatSetsAppend(&device->cheats) = cheats;
	_addBreakpoint(device, cheats);
	_patchROM(device, cheats);
}

void GBACheatRemoveSet(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	size_t i;
	for (i = 0; i < GBACheatSetsSize(&device->cheats); ++i) {
		if (*GBACheatSetsGetPointer(&device->cheats, i) == cheats) {
			break;
		}
	}
	if (i == GBACheatSetsSize(&device->cheats)) {
		return;
	}
	GBACheatSetsShift(&device->cheats, i, 1);
	_unpatchROM(device, cheats);
	_removeBreakpoint(device, cheats);
}

bool GBACheatAddCodeBreaker(struct GBACheatSet* cheats, uint32_t op1, uint16_t op2) {
	char line[14] = "XXXXXXXX XXXX";
	snprintf(line, sizeof(line), "%08X %04X", op1, op2);
	_registerLine(cheats, line);

	enum GBACodeBreakerType type = op1 >> 28;
	struct GBACheat* cheat = 0;

	if (cheats->incompleteCheat) {
		cheats->incompleteCheat->repeat = op1 & 0xFFFF;
		cheats->incompleteCheat->addressOffset = op2;
		cheats->incompleteCheat->operandOffset = 0;
		cheats->incompleteCheat = 0;
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
		cheats->hook->address = BASE_CART0 | (op1 & (SIZE_CART0 - 1));
		cheats->hook->mode = MODE_THUMB;
		cheats->hook->refs = 1;
		cheats->hook->reentries = 0;
		return true;
	case CB_OR_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_OR;
		cheat->width = 2;
		break;
	case CB_ASSIGN_1:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 1;
		break;
	case CB_FILL:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		cheats->incompleteCheat = cheat;
		break;
	case CB_FILL_8:
		GBALog(0, GBA_LOG_STUB, "[Cheat] CodeBreaker code %08X %04X not supported", op1, op2);
		return false;
	case CB_AND_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_AND;
		cheat->width = 2;
		break;
	case CB_IF_EQ:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_EQ;
		cheat->width = 2;
		break;
	case CB_ASSIGN_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		break;
	case CB_ENCRYPT:
		GBALog(0, GBA_LOG_STUB, "[Cheat] CodeBreaker encryption not supported");
		return false;
	case CB_IF_NE:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_NE;
		cheat->width = 2;
		break;
	case CB_IF_GT:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_GT;
		cheat->width = 2;
		break;
	case CB_IF_LT:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_LT;
		cheat->width = 2;
		break;
	case CB_IF_SPECIAL:
		switch (op1 & 0x0FFFFFFF) {
		case 0x20:
			cheat = GBACheatListAppend(&cheats->list);
			cheat->type = CHEAT_IF_AND;
			cheat->width = 2;
			cheat->address = BASE_IO | REG_JOYSTAT;
			cheat->operand = op2;
			cheat->repeat = 1;
			return true;
		default:
			GBALog(0, GBA_LOG_STUB, "[Cheat] CodeBreaker code %08X %04X not supported", op1, op2);
			return false;
		}
	case CB_ADD_2:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_ADD;
		cheat->width = 2;
		break;
	case CB_IF_AND:
		cheat = GBACheatListAppend(&cheats->list);
		cheat->type = CHEAT_IF_AND;
		cheat->width = 2;
		break;
	}

	cheat->address = op1 & 0x0FFFFFFF;
	cheat->operand = op2;
	cheat->repeat = 1;
	return true;
}

bool GBACheatAddCodeBreakerLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint16_t op2;
	line = _hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (*line == ' ') {
		++line;
	}
	line = _hex16(line, &op2);
	if (!line) {
		return false;
	}
	return GBACheatAddCodeBreaker(cheats, op1, op2);
}

bool GBACheatAddGameShark(struct GBACheatSet* set, uint32_t op1, uint32_t op2) {
	uint32_t o1 = op1;
	uint32_t o2 = op2;
	char line[18] = "XXXXXXXX XXXXXXXX";
	snprintf(line, sizeof(line), "%08X %08X", op1, op2);
	_registerLine(set, line);

	switch (set->gsaVersion) {
	case 0:
		_setGameSharkVersion(set, 1);
		// Fall through
	case 1:
		_decryptGameShark(&o1, &o2, set->gsaSeeds);
		return _addGSA1(set, o1, o2);
	}
	return false;
}

bool GBACheatAddGameSharkLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint32_t op2;
	line = _hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (*line == ' ') {
		++line;
	}
	line = _hex32(line, &op2);
	if (!line) {
		return false;
	}
	return GBACheatAddGameShark(cheats, op1, op2);
}

bool GBACheatAddAutodetect(struct GBACheatSet* set, uint32_t op1, uint32_t op2) {
	uint32_t o1 = op1;
	uint32_t o2 = op2;
	char line[18] = "XXXXXXXX XXXXXXXX";
	snprintf(line, sizeof(line), "%08X %08X", op1, op2);
	_registerLine(set, line);

	switch (set->gsaVersion) {
	case 0:
		// Try to detect GameShark version
		_decryptGameShark(&o1, &o2, _gsa1S);
		if ((o1 & 0xF0000000) == 0xF0000000 && !(o2 & 0xFFFFFCFE)) {
			_setGameSharkVersion(set, 1);
			return _addGSA1(set, o1, o2);
		}
		o1 = op1;
		o2 = op2;
		_decryptGameShark(&o1, &o2, _par3S);
		if ((o1 & 0xFE000000) == 0xC4000000 && !(o2 & 0xFFFF0000)) {
			_setGameSharkVersion(set, 3);
			return _addGSA3(set, o1, o2);
		}
		break;
	case 1:
		_decryptGameShark(&o1, &o2, set->gsaSeeds);
		return _addGSA1(set, o1, o2);
	case 3:
		_decryptGameShark(&o1, &o2, set->gsaSeeds);
		return _addGSA3(set, o1, o2);
	}
	return false;
}

bool GBACheatAutodetectLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint32_t op2;
	line = _hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (*line == ' ') {
		++line;
	}
	line = _hex32(line, &op2);
	if (!line) {
		return false;
	}
	return GBACheatAddAutodetect(cheats, op1, op2);
}

bool GBACheatParseFile(struct GBACheatDevice* device, struct VFile* vf) {
	char cheat[MAX_LINE_LENGTH];
	struct GBACheatSet* set = 0;
	struct GBACheatSet* newSet;
	int gsaVersion = 0;
	bool nextDisabled = false;
	bool reset = false;
	while (true) {
		size_t i = 0;
		ssize_t bytesRead = vf->readline(vf, cheat, sizeof(cheat));
		if (bytesRead == 0) {
			break;
		}
		if (bytesRead < 0) {
			return false;
		}
		while (isspace(cheat[i])) {
			++i;
		}
		switch (cheat[i]) {
		case '#':
			do {
				++i;
			} while (isspace(cheat[i]));
			newSet = malloc(sizeof(*set));
			GBACheatSetInit(newSet, &cheat[i]);
			newSet->enabled = !nextDisabled;
			nextDisabled = false;
			if (set) {
				GBACheatAddSet(device, set);
			}
			if (set && !reset) {
				newSet->gsaVersion = set->gsaVersion;
				memcpy(newSet->gsaSeeds, set->gsaSeeds, sizeof(newSet->gsaSeeds));
				if (set->hook) {
					newSet->hook = set->hook;
					++newSet->hook->refs;
				}
			} else {
				_setGameSharkVersion(newSet, gsaVersion);
			}
			reset = false;
			set = newSet;
			break;
		case '!':
			do {
				++i;
			} while (isspace(cheat[i]));
			if (strncasecmp(&cheat[i], "GSAv", 4) == 0 || strncasecmp(&cheat[i], "PARv", 4) == 0) {
				i += 4;
				gsaVersion = atoi(&cheat[i]);
				break;
			}
			if (strcasecmp(&cheat[i], "disabled") == 0) {
				nextDisabled = true;
				break;
			}
			if (strcasecmp(&cheat[i], "reset") == 0) {
				reset = true;
				break;
			}
			break;
		default:
			if (!set) {
				set = malloc(sizeof(*set));
				GBACheatSetInit(set, 0);
				set->enabled = !nextDisabled;
				nextDisabled = false;
				_setGameSharkVersion(set, gsaVersion);
			}
			GBACheatAddLine(set, cheat);
			break;
		}
	}
	if (set) {
		GBACheatAddSet(device, set);
	}
	return true;
}

bool GBACheatSaveFile(struct GBACheatDevice* device, struct VFile* vf) {
	static const char lineStart[3] = "# ";
	static const char lineEnd = '\n';

	struct GBACheatHook* lastHook = 0;

	size_t i;
	for (i = 0; i < GBACheatSetsSize(&device->cheats); ++i) {
		struct GBACheatSet* set = *GBACheatSetsGetPointer(&device->cheats, i);
		if (lastHook && set->hook != lastHook) {
			static const char* resetDirective = "!reset\n";
			vf->write(vf, resetDirective, strlen(resetDirective));
		}
		switch (set->gsaVersion) {
		case 1: {
			static const char* versionDirective = "!GSAv1\n";
			vf->write(vf, versionDirective, strlen(versionDirective));
			break;
		}
		case 3: {
			static const char* versionDirective = "!PARv3\n";
			vf->write(vf, versionDirective, strlen(versionDirective));
			break;
		}
		default:
			break;
		}
		lastHook = set->hook;
		if (!set->enabled) {
			static const char* disabledDirective = "!disabled\n";
			vf->write(vf, disabledDirective, strlen(disabledDirective));
		}

		vf->write(vf, lineStart, 2);
		if (set->name) {
			vf->write(vf, set->name, strlen(set->name));
		}
		vf->write(vf, &lineEnd, 1);
		size_t c;
		for (c = 0; c < StringListSize(&set->lines); ++c) {
			const char* line = *StringListGetPointer(&set->lines, c);
			vf->write(vf, line, strlen(line));
			vf->write(vf, &lineEnd, 1);
		}
	}
	return true;
}

bool GBACheatAddLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint16_t op2;
	uint16_t op3;
	line = _hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (isspace(line[0])) {
		++line;
	}
	line = _hex16(line, &op2);
	if (!line) {
		return false;
	}
	if (!line[0] || isspace(line[0])) {
		return GBACheatAddCodeBreaker(cheats, op1, op2);
	}
	line = _hex16(line, &op3);
	if (!line) {
		return false;
	}
	uint32_t realOp2 = op2;
	realOp2 <<= 16;
	realOp2 |= op3;
	return GBACheatAddAutodetect(cheats, op1, realOp2);
}

void GBACheatRefresh(struct GBACheatDevice* device, struct GBACheatSet* cheats) {
	if (!cheats->enabled) {
		return;
	}
	bool condition = true;
	int conditionRemaining = 0;
	_patchROM(device, cheats);

	size_t nCodes = GBACheatListSize(&cheats->list);
	size_t i;
	for (i = 0; i < nCodes; ++i) {
		if (conditionRemaining > 0) {
			--conditionRemaining;
			if (!condition) {
				continue;
			}
		} else {
			condition = true;
		}
		struct GBACheat* cheat = GBACheatListGetPointer(&cheats->list, i);
		int32_t value = 0;
		int32_t operand = cheat->operand;
		uint32_t operationsRemaining = cheat->repeat;
		uint32_t address = cheat->address;
		bool performAssignment = false;
		for (; operationsRemaining; --operationsRemaining) {
			switch (cheat->type) {
			case CHEAT_ASSIGN:
				value = operand;
				performAssignment = true;
				break;
			case CHEAT_AND:
				value = _readMem(device->p->cpu, address, cheat->width) & operand;
				performAssignment = true;
				break;
			case CHEAT_ADD:
				value = _readMem(device->p->cpu, address, cheat->width) + operand;
				performAssignment = true;
				break;
			case CHEAT_OR:
				value = _readMem(device->p->cpu, address, cheat->width) | operand;
				performAssignment = true;
				break;
			case CHEAT_IF_EQ:
				condition = _readMem(device->p->cpu, address, cheat->width) == operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_NE:
				condition = _readMem(device->p->cpu, address, cheat->width) != operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_LT:
				condition = _readMem(device->p->cpu, address, cheat->width) < operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_GT:
				condition = _readMem(device->p->cpu, address, cheat->width) > operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_ULT:
				condition = (uint32_t) _readMem(device->p->cpu, address, cheat->width) < (uint32_t) operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_UGT:
				condition = (uint32_t) _readMem(device->p->cpu, address, cheat->width) > (uint32_t) operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_AND:
				condition = _readMem(device->p->cpu, address, cheat->width) & operand;
				conditionRemaining = cheat->repeat;
				break;
			case CHEAT_IF_LAND:
				condition = _readMem(device->p->cpu, address, cheat->width) && operand;
				conditionRemaining = cheat->repeat;
				break;
			}

			if (performAssignment) {
				_writeMem(device->p->cpu, address, cheat->width, value);
			}

			address += cheat->addressOffset;
			operand += cheat->operandOffset;
		}
	}
}

void GBACheatDeviceInit(struct ARMCore* cpu, struct ARMComponent* component) {
	struct GBACheatDevice* device = (struct GBACheatDevice*) component;
	device->p = (struct GBA*) cpu->master;
	size_t i;
	for (i = 0; i < GBACheatSetsSize(&device->cheats); ++i) {
		struct GBACheatSet* cheats = *GBACheatSetsGetPointer(&device->cheats, i);
		_addBreakpoint(device, cheats);
		_patchROM(device, cheats);
	}
}

void GBACheatDeviceDeinit(struct ARMComponent* component) {
	struct GBACheatDevice* device = (struct GBACheatDevice*) component;
	size_t i;
	for (i = GBACheatSetsSize(&device->cheats); i--;) {
		struct GBACheatSet* cheats = *GBACheatSetsGetPointer(&device->cheats, i);
		_unpatchROM(device, cheats);
		_removeBreakpoint(device, cheats);
	}
}
