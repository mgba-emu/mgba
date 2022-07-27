/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_H
#define GB_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/cpu.h>
#include <mgba/core/interface.h>
#include <mgba/core/log.h>
#include <mgba/core/timing.h>

#include <mgba/internal/gb/audio.h>
#include <mgba/internal/gb/memory.h>
#include <mgba/internal/gb/sio.h>
#include <mgba/internal/gb/timer.h>
#include <mgba/internal/gb/video.h>

extern const uint32_t DMG_SM83_FREQUENCY;
extern const uint32_t CGB_SM83_FREQUENCY;
extern const uint32_t SGB_SM83_FREQUENCY;

mLOG_DECLARE_CATEGORY(GB);

enum GBIRQ {
	GB_IRQ_VBLANK = 0x0,
	GB_IRQ_LCDSTAT = 0x1,
	GB_IRQ_TIMER = 0x2,
	GB_IRQ_SIO = 0x3,
	GB_IRQ_KEYPAD = 0x4,
};

enum GBIRQVector {
	GB_VECTOR_VBLANK = 0x40,
	GB_VECTOR_LCDSTAT = 0x48,
	GB_VECTOR_TIMER = 0x50,
	GB_VECTOR_SIO = 0x58,
	GB_VECTOR_KEYPAD = 0x60,
};

enum GBSGBCommand {
	SGB_PAL01 = 0,
	SGB_PAL23,
	SGB_PAL03,
	SGB_PAL12,
	SGB_ATTR_BLK,
	SGB_ATTR_LIN,
	SGB_ATTR_DIV,
	SGB_ATTR_CHR,
	SGB_SOUND,
	SGB_SOU_TRN,
	SGB_PAL_SET,
	SGB_PAL_TRN,
	SGB_ATRC_EN,
	SGB_TEST_EN,
	SGB_PICON_EN,
	SGB_DATA_SND,
	SGB_DATA_TRN,
	SGB_MLT_REQ,
	SGB_JUMP,
	SGB_CHR_TRN,
	SGB_PCT_TRN,
	SGB_ATTR_TRN,
	SGB_ATTR_SET,
	SGB_MASK_EN,
	SGB_OBJ_TRN
};

struct GBXMetadata {
	enum GBMemoryBankControllerType mbc;
	bool battery;
	bool rumble;
	bool timer;
	uint32_t romSize;
	uint32_t ramSize;
	union {
		uint8_t u8[32];
		uint32_t u32[8];
	} mapperVars;
};

struct SM83Core;
struct mCoreSync;
struct mAVStream;
struct GB {
	struct mCPUComponent d;

	struct SM83Core* cpu;
	struct GBMemory memory;
	struct GBVideo video;
	struct GBTimer timer;
	struct GBAudio audio;
	struct GBSIO sio;
	enum GBModel model;
	struct GBXMetadata gbx;

	struct mCoreSync* sync;
	struct mTiming timing;

	uint8_t* keySource;

	bool isPristine;
	size_t pristineRomSize;
	size_t yankedRomSize;
	enum GBMemoryBankControllerType yankedMbc;
	uint32_t romCrc32;
	struct VFile* romVf;
	struct VFile* biosVf;
	struct VFile* sramVf;
	struct VFile* sramRealVf;
	uint32_t sramSize;
	int sramDirty;
	uint32_t sramDirtAge;
	bool sramMaskWriteback;

	int sgbBit;
	int currentSgbBits;
	uint8_t sgbPacket[16];
	uint8_t sgbControllers;
	uint8_t sgbCurrentController;
	bool sgbIncrement;

	struct mCoreCallbacksList coreCallbacks;
	struct mAVStream* stream;

	bool cpuBlocked;
	bool earlyExit;
	struct mTimingEvent eiPending;
	unsigned doubleSpeed;

	bool allowOpposingDirections;
};

struct GBCartridge {
	uint8_t entry[4];
	uint8_t logo[48];
	union {
		char titleLong[16];
		struct {
			char titleShort[11];
			char maker[4];
			uint8_t cgb;
		};
	};
	char licensee[2];
	uint8_t sgb;
	uint8_t type;
	uint8_t romSize;
	uint8_t ramSize;
	uint8_t region;
	uint8_t oldLicensee;
	uint8_t version;
	uint8_t headerChecksum;
	uint16_t globalChecksum;
	// And ROM data...
};

void GBCreate(struct GB* gb);
void GBDestroy(struct GB* gb);

void GBReset(struct SM83Core* cpu);
void GBSkipBIOS(struct GB* gb);
void GBMapBIOS(struct GB* gb);
void GBUnmapBIOS(struct GB* gb);
void GBDetectModel(struct GB* gb);

void GBUpdateIRQs(struct GB* gb);
void GBHalt(struct SM83Core* cpu);

struct VFile;
bool GBLoadROM(struct GB* gb, struct VFile* vf);
bool GBLoadSave(struct GB* gb, struct VFile* vf);
void GBUnloadROM(struct GB* gb);
void GBSynthesizeROM(struct VFile* vf);
void GBYankROM(struct GB* gb);
bool GBLoadGBX(struct GBXMetadata* metadata, struct VFile* vf);

void GBLoadBIOS(struct GB* gb, struct VFile* vf);

void GBSramClean(struct GB* gb, uint32_t frameCount);
void GBResizeSram(struct GB* gb, size_t size);
void GBSavedataMask(struct GB* gb, struct VFile* vf, bool writeback);
void GBSavedataUnmask(struct GB* gb);

struct Patch;
void GBApplyPatch(struct GB* gb, struct Patch* patch);

void GBGetGameTitle(const struct GB* gba, char* out);
void GBGetGameCode(const struct GB* gba, char* out);

void GBTestKeypadIRQ(struct GB* gb);

void GBFrameStarted(struct GB* gb);
void GBFrameEnded(struct GB* gb);

CXX_GUARD_END

#endif
