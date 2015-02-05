/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CHEATS_H
#define GBA_CHEATS_H

#include "util/common.h"

#include "arm.h"
#include "util/vector.h"

enum GBACheatType {
	CHEAT_ASSIGN,
	CHEAT_AND,
	CHEAT_ADD,
	CHEAT_OR,
	CHEAT_IF_EQ,
	CHEAT_IF_NE,
	CHEAT_IF_LT,
	CHEAT_IF_GT,
	CHEAT_IF_AND
};

enum GBACodeBreakerType {
	CB_GAME_ID = 0x0,
	CB_HOOK = 0x1,
	CB_OR_2 = 0x2,
	CB_ASSIGN_1 = 0x3,
	CB_FILL = 0x4,
	CB_FILL_8 = 0x5,
	CB_AND_2 = 0x6,
	CB_IF_EQ = 0x7,
	CB_ASSIGN_2 = 0x8,
	CB_ENCRYPT = 0x9,
	CB_IF_NE = 0xA,
	CB_IF_GT = 0xB,
	CB_IF_LT = 0xC,
	CB_IF_SPECIAL = 0xD,
	CB_ADD_2 = 0xE,
	CB_IF_AND = 0xF,
};

struct GBACheat {
	enum GBACheatType type;
	int width;
	uint32_t address;
	uint32_t operand;
	uint32_t repeat;

	int32_t addressOffset;
	int32_t operandOffset;
};

DECLARE_VECTOR(GBACheatList, struct GBACheat);

struct GBACheatSet {
	uint32_t hookAddress;
	enum ExecutionMode hookMode;
	struct GBACheatList list;

	struct GBACheat* incompleteCheat;
	uint32_t patchedOpcode;
};

struct GBACheatDevice {
	struct ARMComponent d;
	struct GBA* p;

	struct GBACheatSet* cheats;
};

void GBACheatDeviceCreate(struct GBACheatDevice*);

void GBACheatSetInit(struct GBACheatSet*);
void GBACheatSetDeinit(struct GBACheatSet*);

void GBACheatAttachDevice(struct GBA* gba, struct GBACheatDevice*);
void GBACheatInstallSet(struct GBACheatDevice*, struct GBACheatSet*);

bool GBACheatAddCodeBreaker(struct GBACheatSet*, uint32_t op1, uint16_t op2);
bool GBACheatAddCodeBreakerLine(struct GBACheatSet*, const char* line);

void GBACheatRefresh(struct GBACheatDevice*);

#endif