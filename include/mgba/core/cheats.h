/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef CHEATS_H
#define CHEATS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/cpu.h>
#include <mgba/core/log.h>
#include <mgba-util/table.h>
#include <mgba-util/vector.h>

enum mCheatType {
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
	CHEAT_IF_LAND,
	CHEAT_IF_NAND,
	CHEAT_IF_BUTTON,
	CHEAT_NEVER,
};

struct mCheat {
	enum mCheatType type;
	int width;
	uint32_t address;
	uint32_t operand;
	uint32_t repeat;
	uint32_t negativeRepeat;

	int32_t addressOffset;
	int32_t operandOffset;
};

struct mCheatPatch {
	uint32_t address;
	int segment;
	uint32_t value;
	int width;
	bool applied;
	uint32_t checkValue;
	bool check;
};

mLOG_DECLARE_CATEGORY(CHEATS);

DECLARE_VECTOR(mCheatList, struct mCheat);
DECLARE_VECTOR(mCheatPatchList, struct mCheatPatch);

struct mCheatDevice;
struct mCheatSet {
	struct mCheatList list;

	void (*deinit)(struct mCheatSet* set);
	void (*add)(struct mCheatSet* set, struct mCheatDevice* device);
	void (*remove)(struct mCheatSet* set, struct mCheatDevice* device);

	bool (*addLine)(struct mCheatSet* set, const char* cheat, int type);
	void (*copyProperties)(struct mCheatSet* set, struct mCheatSet* oldSet);

	void (*parseDirectives)(struct mCheatSet* set, const struct StringList* directives);
	void (*dumpDirectives)(struct mCheatSet* set, struct StringList* directives);

	void (*refresh)(struct mCheatSet* set, struct mCheatDevice* device);

	char* name;
	bool enabled;
	struct mCheatPatchList romPatches;
	struct StringList lines;
};

DECLARE_VECTOR(mCheatSets, struct mCheatSet*);

struct mCheatDevice {
	struct mCPUComponent d;
	struct mCore* p;

	struct mCheatSet* (*createSet)(struct mCheatDevice*, const char* name);

	struct mCheatSets cheats;
	struct Table unpatchedMemory;
	bool autosave;
	bool buttonDown;
};

struct VFile;

void mCheatDeviceCreate(struct mCheatDevice*);
void mCheatDeviceDestroy(struct mCheatDevice*);
void mCheatDeviceClear(struct mCheatDevice*);

void mCheatSetInit(struct mCheatSet*, const char* name);
void mCheatSetDeinit(struct mCheatSet*);
void mCheatSetRename(struct mCheatSet*, const char* name);

bool mCheatAddLine(struct mCheatSet*, const char* line, int type);

void mCheatAddSet(struct mCheatDevice*, struct mCheatSet*);
void mCheatRemoveSet(struct mCheatDevice*, struct mCheatSet*);

bool mCheatParseFile(struct mCheatDevice*, struct VFile*);
bool mCheatSaveFile(struct mCheatDevice*, struct VFile*);

bool mCheatParseLibretroFile(struct mCheatDevice*, struct VFile*);
bool mCheatParseEZFChtFile(struct mCheatDevice*, struct VFile*);

#if !defined(MINIMAL_CORE) || MINIMAL_CORE < 2
void mCheatAutosave(struct mCheatDevice*);
#endif

void mCheatRefresh(struct mCheatDevice*, struct mCheatSet*);
void mCheatPressButton(struct mCheatDevice*, bool down);

CXX_GUARD_END

#endif
