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

#define MAX_ROM_PATCHES 4

enum GBACheatType {
	CHEAT_ASSIGN,
	CHEAT_ASSIGN_INDIRECT,
	CHEAT_AND,
	CHEAT_ADD,
	CHEAT_OR,
	CHEAT_IF_EQ,
	CHEAT_IF_NE,
	CHEAT_IF_LT,
	CHEAT_IF_GT,
	CHEAT_IF_ULT,
	CHEAT_IF_UGT,
	CHEAT_IF_AND,
	CHEAT_IF_LAND
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

enum GBAGameSharkType {
	GSA_ASSIGN_1 = 0x0,
	GSA_ASSIGN_2 = 0x1,
	GSA_ASSIGN_4 = 0x2,
	GSA_ASSIGN_LIST = 0x3,
	GSA_PATCH = 0x6,
	GSA_BUTTON = 0x8,
	GSA_IF_EQ = 0xD,
	GSA_IF_EQ_RANGE = 0xE,
	GSA_HOOK = 0xF
};

enum GBAActionReplay3Condition {
	PAR3_COND_OTHER = 0x00000000,
	PAR3_COND_EQ = 0x08000000,
	PAR3_COND_NE = 0x10000000,
	PAR3_COND_LT = 0x18000000,
	PAR3_COND_GT = 0x20000000,
	PAR3_COND_ULT = 0x28000000,
	PAR3_COND_UGT = 0x30000000,
	PAR3_COND_LAND = 0x38000000,
};

enum GBAActionReplay3Width {
	PAR3_WIDTH_1 = 0x00000000,
	PAR3_WIDTH_2 = 0x02000000,
	PAR3_WIDTH_4 = 0x04000000,
	PAR3_WIDTH_FALSE = 0x06000000,
};

enum GBAActionReplay3Action {
	PAR3_ACTION_NEXT = 0x00000000,
	PAR3_ACTION_NEXT_TWO = 0x40000000,
	PAR3_ACTION_BLOCK = 0x80000000,
	PAR3_ACTION_DISABLE = 0xC0000000,
};

enum GBAActionReplay3Base {
	PAR3_BASE_ASSIGN = 0x00000000,
	PAR3_BASE_INDIRECT = 0x40000000,
	PAR3_BASE_ADD = 0x80000000,
	PAR3_BASE_OTHER = 0xC0000000,

	PAR3_BASE_ASSIGN_1 = 0x00000000,
	PAR3_BASE_ASSIGN_2 = 0x02000000,
	PAR3_BASE_ASSIGN_4 = 0x04000000,
	PAR3_BASE_INDIRECT_1 = 0x40000000,
	PAR3_BASE_INDIRECT_2 = 0x42000000,
	PAR3_BASE_INDIRECT_4 = 0x44000000,
	PAR3_BASE_ADD_1 = 0x80000000,
	PAR3_BASE_ADD_2 = 0x82000000,
	PAR3_BASE_ADD_4 = 0x84000000,
	PAR3_BASE_HOOK = 0xC4000000,
	PAR3_BASE_IO_2 = 0xC6000000,
	PAR3_BASE_IO_3 = 0xC7000000,
};

enum GBAActionReplay3Other {
	PAR3_OTHER_END = 0x00000000,
	PAR3_OTHER_SLOWDOWN = 0x08000000,
	PAR3_OTHER_BUTTON_1 = 0x10000000,
	PAR3_OTHER_BUTTON_2 = 0x12000000,
	PAR3_OTHER_BUTTON_4 = 0x14000000,
	PAR3_OTHER_PATCH_1 = 0x18000000,
	PAR3_OTHER_PATCH_2 = 0x1A000000,
	PAR3_OTHER_PATCH_3 = 0x1C000000,
	PAR3_OTHER_PATCH_4 = 0x1E000000,
	PAR3_OTHER_ENDIF = 0x40000000,
	PAR3_OTHER_ELSE = 0x60000000,
	PAR3_OTHER_FILL_1 = 0x80000000,
	PAR3_OTHER_FILL_2 = 0x82000000,
	PAR3_OTHER_FILL_4 = 0x84000000,
};

enum {
	PAR3_COND = 0x38000000,
	PAR3_WIDTH = 0x06000000,
	PAR3_ACTION = 0xC0000000,
	PAR3_BASE = 0xC0000000,

	PAR3_WIDTH_BASE = 25
};

struct GBACheat {
	enum GBACheatType type;
	int width;
	uint32_t address;
	uint32_t operand;
	uint32_t repeat;
	uint32_t negativeRepeat;

	int32_t addressOffset;
	int32_t operandOffset;
};

struct GBACheatHook {
	uint32_t address;
	enum ExecutionMode mode;
	uint32_t patchedOpcode;
	size_t refs;
	size_t reentries;
};

DECLARE_VECTOR(GBACheatList, struct GBACheat);
DECLARE_VECTOR(StringList, char*);

struct GBACheatSet {
	struct GBACheatHook* hook;
	struct GBACheatList list;

	struct GBACheatPatch {
		uint32_t address;
		int16_t newValue;
		int16_t oldValue;
		bool applied;
		bool exists;
	} romPatches[MAX_ROM_PATCHES];

	struct GBACheat* incompleteCheat;
	struct GBACheatPatch* incompletePatch;
	struct GBACheat* currentBlock;

	int gsaVersion;
	uint32_t gsaSeeds[4];
	int remainingAddresses;

	char* name;
	bool enabled;
	struct StringList lines;
};

DECLARE_VECTOR(GBACheatSets, struct GBACheatSet*);

struct GBACheatDevice {
	struct ARMComponent d;
	struct GBA* p;

	struct GBACheatSets cheats;
};

struct VFile;

void GBACheatDeviceCreate(struct GBACheatDevice*);
void GBACheatDeviceDestroy(struct GBACheatDevice*);

void GBACheatSetInit(struct GBACheatSet*, const char* name);
void GBACheatSetDeinit(struct GBACheatSet*);

void GBACheatAttachDevice(struct GBA* gba, struct GBACheatDevice*);

void GBACheatAddSet(struct GBACheatDevice*, struct GBACheatSet*);
void GBACheatRemoveSet(struct GBACheatDevice*, struct GBACheatSet*);
void GBACheatSetCopyProperties(struct GBACheatSet* newSet, struct GBACheatSet* set);

bool GBACheatAddCodeBreaker(struct GBACheatSet*, uint32_t op1, uint16_t op2);
bool GBACheatAddCodeBreakerLine(struct GBACheatSet*, const char* line);

bool GBACheatAddGameShark(struct GBACheatSet*, uint32_t op1, uint32_t op2);
bool GBACheatAddGameSharkLine(struct GBACheatSet*, const char* line);

bool GBACheatAddProActionReplay(struct GBACheatSet*, uint32_t op1, uint32_t op2);
bool GBACheatAddProActionReplayLine(struct GBACheatSet*, const char* line);

bool GBACheatAddAutodetect(struct GBACheatSet*, uint32_t op1, uint32_t op2);
bool GBACheatAddAutodetectLine(struct GBACheatSet*, const char* line);

bool GBACheatParseFile(struct GBACheatDevice*, struct VFile*);
bool GBACheatSaveFile(struct GBACheatDevice*, struct VFile*);

bool GBACheatAddLine(struct GBACheatSet*, const char* line);

void GBACheatRefresh(struct GBACheatDevice*, struct GBACheatSet*);

#endif
