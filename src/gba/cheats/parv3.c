/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/cheats.h>

#include "gba/cheats/gameshark.h"
#include <mgba/internal/gba/gba.h>
#include <mgba-util/string.h>

const uint32_t GBACheatProActionReplaySeeds[4] = { 0x7AA9648F, 0x7FAE6994, 0xC0EFAAD5, 0x42712C57 };

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
static uint32_t _parAddr(uint32_t x) {
	return (x & 0xFFFFF) | ((x << 4) & 0x0F000000);
}

static void _parEndBlock(struct GBACheatSet* cheats) {
	size_t size = mCheatListSize(&cheats->d.list) - cheats->currentBlock - 1;
	struct mCheat* currentBlock = mCheatListGetPointer(&cheats->d.list, cheats->currentBlock);
	if (currentBlock->repeat) {
		currentBlock->negativeRepeat = size - currentBlock->repeat;
	} else {
		currentBlock->repeat = size;
	}
	cheats->currentBlock = COMPLETE;
}

static void _parElseBlock(struct GBACheatSet* cheats) {
	size_t size = mCheatListSize(&cheats->d.list) - cheats->currentBlock - 1;
	struct mCheat* currentBlock = mCheatListGetPointer(&cheats->d.list, cheats->currentBlock);
	currentBlock->repeat = size;
}

static bool _addPAR3Cond(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2) {
	enum GBAActionReplay3Condition condition = op1 & PAR3_COND;
	int width = 1 << ((op1 & PAR3_WIDTH) >> PAR3_WIDTH_BASE);
	if ((op1 & PAR3_ACTION) == PAR3_ACTION_DISABLE) {
		// TODO: Codes that disable
		mLOG(CHEATS, STUB, "Disable-type PARv3 codes not yet supported");
		return false;
	}

	struct mCheat* cheat = mCheatListAppend(&cheats->d.list);
	cheat->address = _parAddr(op1);
	cheat->width = width;
	cheat->operand = op2 & (0xFFFFFFFFU >> ((4 - width) * 8));
	cheat->addressOffset = 0;
	cheat->operandOffset = 0;

	switch (op1 & PAR3_ACTION) {
	case PAR3_ACTION_NEXT:
		cheat->repeat = 1;
		cheat->negativeRepeat = 0;
		break;
	case PAR3_ACTION_NEXT_TWO:
		cheat->repeat = 2;
		cheat->negativeRepeat = 0;
		break;
	case PAR3_ACTION_BLOCK:
		cheat->repeat = 0;
		cheat->negativeRepeat = 0;
		if (cheats->currentBlock != COMPLETE) {
			_parEndBlock(cheats);
		}
		cheats->currentBlock = mCheatListIndex(&cheats->d.list, cheat);
		break;
	}

	switch (condition) {
	case PAR3_COND_OTHER:
		// We shouldn't be able to get here
		mLOG(CHEATS, ERROR, "Unexpectedly created 'other' PARv3 code");
		cheat->type = CHEAT_IF_LAND;
		cheat->operand = 0;
		break;
	case PAR3_COND_EQ:
		cheat->type = CHEAT_IF_EQ;
		break;
	case PAR3_COND_NE:
		cheat->type = CHEAT_IF_NE;
		break;
	case PAR3_COND_LT:
		cheat->type = CHEAT_IF_LT;
		break;
	case PAR3_COND_GT:
		cheat->type = CHEAT_IF_GT;
		break;
	case PAR3_COND_ULT:
		cheat->type = CHEAT_IF_ULT;
		break;
	case PAR3_COND_UGT:
		cheat->type = CHEAT_IF_UGT;
		break;
	case PAR3_COND_AND:
		cheat->type = CHEAT_IF_AND;
		break;
	}

	if (width > 4) {
		cheat->width = 0;
		cheat->type = CHEAT_NEVER;
	}
	return true;
}

static bool _addPAR3Special(struct GBACheatSet* cheats, uint32_t op2) {
	int romPatch = -1;
	struct mCheat* cheat;
	switch (op2 & 0xFF000000) {
	case PAR3_OTHER_SLOWDOWN:
		// TODO: Slowdown
		mLOG(CHEATS, STUB, "Unimplemented PARv3 slowdown");
		return false;
	case PAR3_OTHER_BUTTON_1:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_IF_BUTTON;
		cheat->repeat = 1;
		cheat->negativeRepeat = 0;
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 1;
		cheat->repeat = 1;
		cheat->address = _parAddr(op2);
		cheats->incompleteCheat = mCheatListIndex(&cheats->d.list, cheat);
		break;
	case PAR3_OTHER_BUTTON_2:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_IF_BUTTON;
		cheat->repeat = 1;
		cheat->negativeRepeat = 0;
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 2;
		cheat->repeat = 1;
		cheat->address = _parAddr(op2);
		cheats->incompleteCheat = mCheatListIndex(&cheats->d.list, cheat);
		break;
	case PAR3_OTHER_BUTTON_4:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_IF_BUTTON;
		cheat->repeat = 1;
		cheat->negativeRepeat = 0;
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->width = 4;
		cheat->repeat = 1;
		cheat->address = _parAddr(op2);
		cheats->incompleteCheat = mCheatListIndex(&cheats->d.list, cheat);
		break;
	case PAR3_OTHER_PATCH_1:
		romPatch = 0;
		break;
	case PAR3_OTHER_PATCH_2:
		romPatch = 1;
		break;
	case PAR3_OTHER_PATCH_3:
		romPatch = 2;
		break;
	case PAR3_OTHER_PATCH_4:
		romPatch = 3;
		break;
	case PAR3_OTHER_ENDIF:
		if (cheats->currentBlock != COMPLETE) {
			_parEndBlock(cheats);
			return true;
		}
		return false;
	case PAR3_OTHER_ELSE:
		if (cheats->currentBlock != COMPLETE) {
			_parElseBlock(cheats);
			return true;
		}
		return false;
	case PAR3_OTHER_FILL_1:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->address = _parAddr(op2);
		cheat->width = 1;
		cheats->incompleteCheat = mCheatListIndex(&cheats->d.list, cheat);
		break;
	case PAR3_OTHER_FILL_2:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->address = _parAddr(op2);
		cheat->width = 2;
		cheats->incompleteCheat = mCheatListIndex(&cheats->d.list, cheat);
		break;
	case PAR3_OTHER_FILL_4:
		cheat = mCheatListAppend(&cheats->d.list);
		cheat->type = CHEAT_ASSIGN;
		cheat->address = _parAddr(op2);
		cheat->width = 4;
		cheats->incompleteCheat = mCheatListIndex(&cheats->d.list, cheat);
		break;
	}
	if (romPatch >= 0) {
		struct mCheatPatch* patch = mCheatPatchListAppend(&cheats->d.romPatches);
		patch->address = GBA_BASE_ROM0 | ((op2 & 0xFFFFFF) << 1);
		patch->applied = false;
		patch->check = false;
		patch->width = 2;
		cheats->incompletePatch = patch;
	}
	return true;
}

bool GBACheatAddProActionReplayRaw(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2) {
	if (cheats->incompletePatch) {
		cheats->incompletePatch->value = op1;
		cheats->incompletePatch = NULL;
		return true;
	}
	if (cheats->incompleteCheat != COMPLETE) {
		struct mCheat* incompleteCheat = mCheatListGetPointer(&cheats->d.list, cheats->incompleteCheat);
		incompleteCheat->operand = op1 & (0xFFFFFFFFU >> ((4 - incompleteCheat->width) * 8));
		if (cheats->incompleteCheat > 0) {
			struct mCheat* lastCheat = mCheatListGetPointer(&cheats->d.list, cheats->incompleteCheat - 1);
			if (lastCheat->type == CHEAT_IF_BUTTON) {
				cheats->incompleteCheat = COMPLETE;
				return true;
			}
		}
		incompleteCheat->operandOffset = op2 >> 24;
		incompleteCheat->repeat = (op2 >> 16) & 0xFF;
		incompleteCheat->addressOffset = (op2 & 0xFFFF) * incompleteCheat->width;
		cheats->incompleteCheat = COMPLETE;
		return true;
	}

	if (op2 == 0x001DC0DE) {
		return true;
	}

	switch (op1) {
	case 0x00000000:
		return _addPAR3Special(cheats, op2);
	case 0xDEADFACE:
		GBACheatReseedGameShark(cheats->gsaSeeds, op2, _par3T1, _par3T2);
		return true;
	}

	if (op1 >> 24 == 0xC4) {
		if (cheats->hook) {
			return false;
		}
		cheats->hook = malloc(sizeof(*cheats->hook));
		cheats->hook->address = GBA_BASE_ROM0 | (op1 & (GBA_SIZE_ROM0 - 2));
		cheats->hook->mode = MODE_THUMB;
		cheats->hook->refs = 1;
		cheats->hook->reentries = 0;
		return true;
	}

	if (op1 & PAR3_COND) {
		return _addPAR3Cond(cheats, op1, op2);
	}

	int width = 1 << ((op1 & PAR3_WIDTH) >> PAR3_WIDTH_BASE);
	struct mCheat* cheat = mCheatListAppend(&cheats->d.list);
	cheat->address = _parAddr(op1);
	cheat->operandOffset = 0;
	cheat->addressOffset = 0;
	cheat->repeat = 1;

	switch (op1 & PAR3_BASE) {
	case PAR3_BASE_ASSIGN:
		cheat->type = CHEAT_ASSIGN;
		cheat->addressOffset = width;
		if (width < 4) {
			cheat->repeat = (op2 >> (width * 8)) + 1;
		}
		break;
	case PAR3_BASE_INDIRECT:
		cheat->type = CHEAT_ASSIGN_INDIRECT;
		if (width < 4) {
			cheat->addressOffset = (op2 >> (width * 8)) * width;
		}
		break;
	case PAR3_BASE_ADD:
		cheat->type = CHEAT_ADD;
		break;
	case PAR3_BASE_OTHER:
		width = ((op1 >> 24) & 1) + 1;
		cheat->type = CHEAT_ASSIGN;
		cheat->address = GBA_BASE_IO | (op1 & OFFSET_MASK);
		break;
	}
	if (op1 & 0x01000000 && (op1 & 0xFE000000) != 0xC6000000) {
		return false;
	}


	cheat->width = width;
	cheat->operand = op2 & (0xFFFFFFFFU >> ((4 - width) * 8));
	return true;
}

bool GBACheatAddProActionReplay(struct GBACheatSet* set, uint32_t op1, uint32_t op2) {
	uint32_t o1 = op1;
	uint32_t o2 = op2;
	char line[18] = "XXXXXXXX XXXXXXXX";
	snprintf(line, sizeof(line), "%08X %08X", op1, op2);

	switch (set->gsaVersion) {
	default:
		GBACheatSetGameSharkVersion(set, GBA_GS_PARV3);
	// Fall through
	case GBA_GS_PARV3:
		GBACheatDecryptGameShark(&o1, &o2, set->gsaSeeds);
	// Fall through
	case GBA_GS_PARV3_RAW:
		return GBACheatAddProActionReplayRaw(set, o1, o2);
	}
	return false;
}

bool GBACheatAddProActionReplayLine(struct GBACheatSet* cheats, const char* line) {
	uint32_t op1;
	uint32_t op2;
	line = hex32(line, &op1);
	if (!line) {
		return false;
	}
	while (*line == ' ') {
		++line;
	}
	line = hex32(line, &op2);
	if (!line) {
		return false;
	}
	return GBACheatAddProActionReplay(cheats, op1, op2);
}

int GBACheatProActionReplayProbability(uint32_t op1, uint32_t op2) {
	int probability = 0;
	if (op2 == 0x001DC0DE) {
		return 0x100;
	}
	if (op1 == 0xDEADFACE && !(op2 & 0xFFFF0000)) {
		return 0x100;
	}
	if (!op1) {
		probability += 0x40;
		uint32_t address = _parAddr(op2);
		switch (op2 & 0xFE000000) {
		case PAR3_OTHER_FILL_1:
		case PAR3_OTHER_FILL_2:
		case PAR3_OTHER_FILL_4:
			probability += GBACheatAddressIsReal(address);
			break;
		case PAR3_OTHER_PATCH_1:
		case PAR3_OTHER_PATCH_2:
		case PAR3_OTHER_PATCH_3:
		case PAR3_OTHER_PATCH_4:
			// TODO: Detect ROM address
			break;
		case PAR3_OTHER_END:
		case PAR3_OTHER_SLOWDOWN:
		case PAR3_OTHER_BUTTON_1:
		case PAR3_OTHER_BUTTON_2:
		case PAR3_OTHER_BUTTON_4:
		case PAR3_OTHER_ENDIF:
		case PAR3_OTHER_ELSE:
			if (op2 & 0x01000000) {
				probability -= 0x40;
			}
			break;
		default:
			probability -= 0x40;
			break;
		}
		return probability;
	}
	int width = ((op1 & PAR3_WIDTH) >> (PAR3_WIDTH_BASE - 3));
	if (op1 & PAR3_COND) {
		probability += 0x20;
		if (width >= 24) {
			return 0;
		}
		if (op2 & ~((1 << width) - 1)) {
			probability -= 0x10;
		}
	} else {
		uint32_t address = _parAddr(op1);
		probability += 0x20;
		switch (op1 & PAR3_BASE) {
		case PAR3_BASE_ADD:
			if (op2 & ~((1 << width) - 1)) {
				probability -= 0x10;
			}
			// Fall through
		case PAR3_BASE_ASSIGN:
		case PAR3_BASE_INDIRECT:
			probability += GBACheatAddressIsReal(address);
			if (op1 & 0x01000000) {
				return 0;
			}
			break;
		case PAR3_BASE_OTHER:
			break;
		}
	}
	return probability;
}
