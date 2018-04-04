/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CHEATS_GAMESHARK_H
#define GBA_CHEATS_GAMESHARK_H

#include <mgba-util/common.h>

CXX_GUARD_START

extern const uint32_t GBACheatGameSharkSeeds[4];

enum GBACheatGameSharkVersion {
	GBA_GS_NOT_SET = 0,
	GBA_GS_GSAV1 = 1,
	GBA_GS_GSAV1_RAW = 2,
	GBA_GS_PARV3 = 3,
	GBA_GS_PARV3_RAW = 4
};

struct GBACheatSet;
void GBACheatDecryptGameShark(uint32_t* op1, uint32_t* op2, const uint32_t* seeds);
void GBACheatReseedGameShark(uint32_t* seeds, uint16_t params, const uint8_t* t1, const uint8_t* t2);
void GBACheatSetGameSharkVersion(struct GBACheatSet* cheats, enum GBACheatGameSharkVersion version);
bool GBACheatAddGameSharkRaw(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2);
int GBACheatGameSharkProbability(uint32_t op1, uint32_t op2);

CXX_GUARD_END

#endif
