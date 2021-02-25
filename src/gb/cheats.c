/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/cheats.h>

#include <mgba/core/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/memory.h>
#include <mgba-util/string.h>

static void GBCheatSetCopyProperties(struct mCheatSet* set, struct mCheatSet* oldSet);
static void GBCheatParseDirectives(struct mCheatSet* set, const struct StringList* directives);
static void GBCheatDumpDirectives(struct mCheatSet* set, struct StringList* directives);
static bool GBCheatAddLine(struct mCheatSet*, const char* line, int type);

static struct mCheatSet* GBCheatSetCreate(struct mCheatDevice* device, const char* name) {
	UNUSED(device);
	struct mCheatSet* set = malloc(sizeof(*set));
	mCheatSetInit(set, name);

	set->deinit = NULL;
	set->add = NULL;
	set->remove = NULL;

	set->addLine = GBCheatAddLine;
	set->copyProperties = GBCheatSetCopyProperties;

	set->parseDirectives = GBCheatParseDirectives;
	set->dumpDirectives = GBCheatDumpDirectives;

	set->refresh = NULL;
	return set;
}

struct mCheatDevice* GBCheatDeviceCreate(void) {
	struct mCheatDevice* device = malloc(sizeof(*device));
	mCheatDeviceCreate(device);
	device->createSet = GBCheatSetCreate;
	return device;
}

static bool GBCheatAddCodebreaker(struct mCheatSet* cheats, uint16_t address, uint8_t data) {
	struct mCheat* cheat = mCheatListAppend(&cheats->list);
	cheat->type = CHEAT_ASSIGN;
	cheat->width = 1;
	cheat->address = address;
	cheat->operand = data;
	cheat->repeat = 1;
	cheat->negativeRepeat = 0;
	return true;
}

static bool GBCheatAddGameShark(struct mCheatSet* cheats, uint32_t op) {
	return GBCheatAddCodebreaker(cheats, ((op & 0xFF) << 8) | ((op >> 8) & 0xFF), (op >> 16) & 0xFF);
}

static bool GBCheatAddGameSharkLine(struct mCheatSet* cheats, const char* line) {
	uint32_t op;
	if (!hex32(line, &op)) {
		return false;
	}
	return GBCheatAddGameShark(cheats, op);
}

static bool GBCheatAddGameGenieLine(struct mCheatSet* cheats, const char* line) {
	uint16_t op1;
	uint16_t op2;
	uint16_t op3 = 0x1000;
	const char* lineNext = hex12(line, &op1);
	if (!lineNext || lineNext[0] != '-') {
		return false;
	}
	++lineNext;
	lineNext = hex12(lineNext, &op2);
	if (!lineNext) {
		return false;
	}
	if (lineNext[0] == '-') {
		++lineNext;
		lineNext = hex12(lineNext, &op3);
	}
	if (!lineNext || lineNext[0]) {
		return false;
	}
	uint16_t address = (op1 & 0xF) << 8;
	address |= (op2 >> 4) & 0xFF;
	address |= ((op2 & 0xF) ^ 0xF) << 12;
	struct mCheatPatch* patch = mCheatPatchListAppend(&cheats->romPatches);
	patch->address = address;
	patch->value = op1 >> 4;
	patch->applied = false;
	patch->width = 1;
	patch->segment = -1;
	if (op3 < 0x1000) {
		uint32_t value = ((op3 & 0xF00) << 20) | (op3 & 0xF);
		value = ROR(value, 2);
		value |= value >> 24;
		value ^= 0xBA;
		patch->checkValue = value & 0xFF;
		patch->check = true;
	} else {
		patch->check = false;
	}
	return true;
}

static bool GBCheatAddVBALine(struct mCheatSet* cheats, const char* line) {
	uint16_t address;
	uint8_t value;
	const char* lineNext = hex16(line, &address);
	if (!lineNext || lineNext[0] != ':') {
		return false;
	}
	if (!hex8(line, &value)) {
		return false;
	}
	struct mCheat* cheat = mCheatListAppend(&cheats->list);
	cheat->type = CHEAT_ASSIGN;
	cheat->width = 1;
	cheat->address = address;
	cheat->operand = value;
	cheat->repeat = 1;
	cheat->negativeRepeat = 0;
	return true;
}

bool GBCheatAddLine(struct mCheatSet* cheats, const char* line, int type) {
	switch (type) {
	case GB_CHEAT_AUTODETECT:
		break;
	case GB_CHEAT_GAME_GENIE:
		return GBCheatAddGameGenieLine(cheats, line);
	case GB_CHEAT_GAMESHARK:
		return GBCheatAddGameSharkLine(cheats, line);
	case GB_CHEAT_VBA:
		return GBCheatAddVBALine(cheats, line);
	default:
		return false;
	}

	uint16_t op1;
	uint8_t op2;
	uint8_t op3;
	bool codebreaker = false;
	const char* lineNext = hex16(line, &op1);
	if (!lineNext) {
		return GBCheatAddGameGenieLine(cheats, line);
	}
	if (lineNext[0] == ':') {
		return GBCheatAddVBALine(cheats, line);
	}
	lineNext = hex8(lineNext, &op2);
	if (!lineNext) {
		return false;
	}
	if (lineNext[0] == '-') {
		codebreaker = true;
		++lineNext;
	}
	lineNext = hex8(lineNext, &op3);
	if (!lineNext) {
		return false;
	}
	if (codebreaker) {
		uint16_t address = (op1 << 8) | op2;
		return GBCheatAddCodebreaker(cheats, address, op3);
	} else {
		uint32_t realOp = op1 << 16;
		realOp |= op2 << 8;
		realOp |= op3;
		return GBCheatAddGameShark(cheats, realOp);
	}
}

static void GBCheatSetCopyProperties(struct mCheatSet* set, struct mCheatSet* oldSet) {
	UNUSED(set);
	UNUSED(oldSet);
}

static void GBCheatParseDirectives(struct mCheatSet* set, const struct StringList* directives) {
	UNUSED(set);
	UNUSED(directives);
}

static void GBCheatDumpDirectives(struct mCheatSet* set, struct StringList* directives) {
	UNUSED(set);
	UNUSED(directives);
}
