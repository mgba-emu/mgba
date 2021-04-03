/* Copyright (c) 2016 taizou
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Support for copy protected unlicensed games from the company Vast Fame

#ifndef GBA_VFAME_H
#define GBA_VFAME_H

#include <mgba-util/common.h>

CXX_GUARD_START

enum GBAVFameCartType {
	VFAME_NO = 0,
	VFAME_STANDARD = 1,
	VFAME_GEORGE = 2
};

struct GBAVFameCart {
	enum GBAVFameCartType cartType;
	int sramMode;
	int romMode;
	int8_t writeSequence[5];
	bool acceptingModeChange;
};

void GBAVFameInit(struct GBAVFameCart* cart);
void GBAVFameDetect(struct GBAVFameCart* cart, uint32_t* rom, size_t romSize);
void GBAVFameSramWrite(struct GBAVFameCart* cart, uint32_t address, uint8_t value, uint8_t* sramData);
uint32_t GBAVFameModifyRomAddress(struct GBAVFameCart* cart, uint32_t address, size_t romSize);
uint32_t GBAVFameGetPatternValue(uint32_t address, int bits);

CXX_GUARD_END

#endif
