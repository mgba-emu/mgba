/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CHEATS_PARV3_H
#define GBA_CHEATS_PARV3_H

#include <mgba-util/common.h>

CXX_GUARD_START

extern const uint32_t GBACheatProActionReplaySeeds[4];

struct GBACheatSet;
bool GBACheatAddProActionReplayRaw(struct GBACheatSet* cheats, uint32_t op1, uint32_t op2);
int GBACheatProActionReplayProbability(uint32_t op1, uint32_t op2);

CXX_GUARD_END

#endif
