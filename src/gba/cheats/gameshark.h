/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CHEATS_GAMESHARK_H
#define GBA_CHEATS_GAMESHARK_H

#include "gba/cheats.h"

extern const uint32_t GBACheatGameSharkSeeds[4];

void GBACheatDecryptGameShark(uint32_t* op1, uint32_t* op2, const uint32_t* seeds);
void GBACheatReseedGameShark(uint32_t* seeds, uint16_t params, const uint8_t* t1, const uint8_t* t2);
void GBACheatSetGameSharkVersion(struct GBACheatSet* cheats, int version);
bool GBACheatAddGameSharkRaw(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2);

#endif
