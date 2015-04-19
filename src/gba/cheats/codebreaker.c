/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gba/cheats.h"

#include "gba/cheats/cheats-private.h"
#include "gba/gba.h"
#include "gba/io.h"
#include "util/string.h"

bool GBACheatAddCodeBreaker(struct GBACheatSet* cheats, uint32_t op1, uint16_t op2) {
	char line[14] = "XXXXXXXX XXXX";
	snprintf(line, sizeof(line), "%08X %04X", op1, op2);
	GBACheatRegisterLine(cheats, line);

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
