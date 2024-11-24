/* Copyright (c) 2013-2024 Jeffrey Pfau
 * Copyright (c) 2016 taizou
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_UNLICENSED_H
#define GBA_UNLICENSED_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/timing.h>

enum GBAVFameCartType {
	VFAME_STANDARD = 0,
	VFAME_GEORGE = 1,
	VFAME_ALTERNATE = 2,
};

enum GBAUnlCartType {
	GBA_UNL_CART_NONE = 0,
	GBA_UNL_CART_VFAME = 1,
	GBA_UNL_CART_MULTICART = 2,
};

struct GBAVFameCart {
	enum GBAVFameCartType cartType;
	int sramMode;
	int romMode;
	int8_t writeSequence[5];
	bool acceptingModeChange;
};

struct GBAMulticart {
	struct mTimingEvent settle;
	uint32_t* rom;
	size_t fullSize;

	uint8_t bank;
	uint8_t offset;
	uint8_t size;
	bool sramActive;
	bool locked;
	uint8_t unk;
};

struct GBAUnlCart {
	enum GBAUnlCartType type;
	union {
		struct GBAVFameCart vfame;
		struct GBAMulticart multi;
	};
};

struct GBA;
struct GBAMemory;
void GBAUnlCartInit(struct GBA*);
void GBAUnlCartReset(struct GBA*);
void GBAUnlCartUnload(struct GBA*);
void GBAUnlCartDetect(struct GBA*);
void GBAUnlCartWriteSRAM(struct GBA*, uint32_t address, uint8_t value);
void GBAUnlCartWriteROM(struct GBA*, uint32_t address, uint16_t value);

struct GBASerializedState;
void GBAUnlCartSerialize(const struct GBA* gba, struct GBASerializedState* state);
void GBAUnlCartDeserialize(struct GBA* gba, const struct GBASerializedState* state);

bool GBAVFameDetect(struct GBAVFameCart* cart, uint32_t* rom, size_t romSize, uint32_t crc32);
void GBAVFameSramWrite(struct GBAVFameCart* cart, uint32_t address, uint8_t value, uint8_t* sramData);
uint32_t GBAVFameModifyRomAddress(struct GBAVFameCart* cart, uint32_t address, size_t romSize);
uint32_t GBAVFameGetPatternValue(uint32_t address, int bits);

CXX_GUARD_END

#endif
