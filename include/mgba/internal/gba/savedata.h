/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SAVEDATA_H
#define GBA_SAVEDATA_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>

mLOG_DECLARE_CATEGORY(GBA_SAVE);

struct VFile;

enum SavedataType {
	SAVEDATA_AUTODETECT = -1,
	SAVEDATA_FORCE_NONE = 0,
	SAVEDATA_SRAM = 1,
	SAVEDATA_FLASH512 = 2,
	SAVEDATA_FLASH1M = 3,
	SAVEDATA_EEPROM = 4,
	SAVEDATA_EEPROM512 = 5,
	SAVEDATA_SRAM512 = 6,
};

enum SavedataCommand {
	EEPROM_COMMAND_NULL = 0,
	EEPROM_COMMAND_PENDING = 1,
	EEPROM_COMMAND_WRITE = 2,
	EEPROM_COMMAND_READ_PENDING = 3,
	EEPROM_COMMAND_READ = 4,

	FLASH_COMMAND_START = 0xAA,
	FLASH_COMMAND_CONTINUE = 0x55,

	FLASH_COMMAND_ERASE_CHIP = 0x10,
	FLASH_COMMAND_ERASE_SECTOR = 0x30,

	FLASH_COMMAND_NONE = 0,
	FLASH_COMMAND_ERASE = 0x80,
	FLASH_COMMAND_ID = 0x90,
	FLASH_COMMAND_PROGRAM = 0xA0,
	FLASH_COMMAND_SWITCH_BANK = 0xB0,
	FLASH_COMMAND_TERMINATE = 0xF0
};

enum FlashStateMachine {
	FLASH_STATE_RAW = 0,
	FLASH_STATE_START = 1,
	FLASH_STATE_CONTINUE = 2,
};

enum FlashManufacturer {
	FLASH_MFG_PANASONIC = 0x1B32,
	FLASH_MFG_SANYO = 0x1362
};

enum {
	SAVEDATA_FLASH_BASE = 0x0E005555,

	FLASH_BASE_HI = 0x5555,
	FLASH_BASE_LO = 0x2AAA
};

struct GBASavedata {
	enum SavedataType type;
	uint8_t* data;
	enum SavedataCommand command;
	struct VFile* vf;
	struct GBACartridgeHardware* gpio;

	int mapMode;
	bool maskWriteback;
	struct VFile* realVf;

	int8_t readBitsRemaining;
	uint32_t readAddress;
	uint32_t writeAddress;

	uint8_t* currentBank;

	struct mTiming* timing;
	unsigned settling;
	struct mTimingEvent dust;

	int dirty;
	uint32_t dirtAge;

	enum FlashStateMachine flashState;
};

struct GBASavedataRTCBuffer {
	uint8_t time[7];
	uint8_t control;
	uint64_t lastLatch;
};

void GBASavedataInit(struct GBASavedata* savedata, struct VFile* vf);
void GBASavedataDeinit(struct GBASavedata* savedata);

void GBASavedataMask(struct GBASavedata* savedata, struct VFile* vf, bool writeback);
void GBASavedataUnmask(struct GBASavedata* savedata);
size_t GBASavedataSize(const struct GBASavedata* savedata);
bool GBASavedataClone(struct GBASavedata* savedata, struct VFile* out);
bool GBASavedataLoad(struct GBASavedata* savedata, struct VFile* in);
void GBASavedataForceType(struct GBASavedata* savedata, enum SavedataType type);

void GBASavedataInitFlash(struct GBASavedata* savedata);
void GBASavedataInitEEPROM(struct GBASavedata* savedata);
void GBASavedataInitSRAM(struct GBASavedata* savedata);
void GBASavedataInitSRAM512(struct GBASavedata* savedata);

uint8_t GBASavedataReadFlash(struct GBASavedata* savedata, uint16_t address);
void GBASavedataWriteFlash(struct GBASavedata* savedata, uint16_t address, uint8_t value);

uint16_t GBASavedataReadEEPROM(struct GBASavedata* savedata);
void GBASavedataWriteEEPROM(struct GBASavedata* savedata, uint16_t value, uint32_t writeSize);

void GBASavedataClean(struct GBASavedata* savedata, uint32_t frameCount);

void GBASavedataRTCRead(struct GBASavedata* savedata);
void GBASavedataRTCWrite(struct GBASavedata* savedata);

struct GBASerializedState;
void GBASavedataSerialize(const struct GBASavedata* savedata, struct GBASerializedState* state);
void GBASavedataDeserialize(struct GBASavedata* savedata, const struct GBASerializedState* state);

CXX_GUARD_END

#endif
