/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CHEATS_H
#define GBA_CHEATS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/arm/arm.h>
#include <mgba/core/cheats.h>
#include <mgba-util/vector.h>

#define COMPLETE ((size_t) -1)

enum GBACheatType {
	GBA_CHEAT_AUTODETECT,
	GBA_CHEAT_CODEBREAKER,
	GBA_CHEAT_GAMESHARK,
	GBA_CHEAT_PRO_ACTION_REPLAY,
	GBA_CHEAT_VBA
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
	PAR3_COND_AND = 0x38000000,
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

struct GBACheatHook {
	uint32_t address;
	enum ExecutionMode mode;
	uint32_t patchedOpcode;
	size_t refs;
	size_t reentries;
};

DECLARE_VECTOR(GBACheatPatchList, struct GBACheatPatch);

struct GBACheatSet {
	struct mCheatSet d;
	struct GBACheatHook* hook;

	size_t incompleteCheat;
	struct mCheatPatch* incompletePatch;
	size_t currentBlock;

	int gsaVersion;
	uint32_t gsaSeeds[4];
	uint32_t cbRngState;
	uint32_t cbMaster;
	uint8_t cbTable[0x30];
	uint32_t cbSeeds[4];
	int remainingAddresses;
};

struct VFile;

struct mCheatDevice* GBACheatDeviceCreate(void);

bool GBACheatAddCodeBreaker(struct GBACheatSet*, uint32_t op1, uint16_t op2);
bool GBACheatAddCodeBreakerLine(struct GBACheatSet*, const char* line);

bool GBACheatAddGameShark(struct GBACheatSet*, uint32_t op1, uint32_t op2);
bool GBACheatAddGameSharkLine(struct GBACheatSet*, const char* line);

bool GBACheatAddProActionReplay(struct GBACheatSet*, uint32_t op1, uint32_t op2);
bool GBACheatAddProActionReplayLine(struct GBACheatSet*, const char* line);

bool GBACheatAddVBALine(struct GBACheatSet*, const char* line);

int GBACheatAddressIsReal(uint32_t address);

CXX_GUARD_END

#endif
