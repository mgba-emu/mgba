/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_EREADER_H
#define GBA_EREADER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba-util/vector.h>

struct GBACartridgeHardware;

#define EREADER_DOTCODE_STRIDE 1420
#define EREADER_DOTCODE_SIZE (EREADER_DOTCODE_STRIDE * 40)
#define EREADER_CARDS_MAX 16

DECL_BITFIELD(EReaderControl0, uint8_t);
DECL_BIT(EReaderControl0, Data, 0);
DECL_BIT(EReaderControl0, Clock, 1);
DECL_BIT(EReaderControl0, Direction, 2);
DECL_BIT(EReaderControl0, LedEnable, 3);
DECL_BIT(EReaderControl0, Scan, 4);
DECL_BIT(EReaderControl0, Phi, 5);
DECL_BIT(EReaderControl0, PowerEnable, 6);
DECL_BITFIELD(EReaderControl1, uint8_t);
DECL_BIT(EReaderControl1, Scanline, 1);
DECL_BIT(EReaderControl1, Unk1, 4);
DECL_BIT(EReaderControl1, Voltage, 5);

enum EReaderStateMachine {
	EREADER_SERIAL_INACTIVE = 0,
	EREADER_SERIAL_STARTING,
	EREADER_SERIAL_BIT_0,
	EREADER_SERIAL_BIT_1,
	EREADER_SERIAL_BIT_2,
	EREADER_SERIAL_BIT_3,
	EREADER_SERIAL_BIT_4,
	EREADER_SERIAL_BIT_5,
	EREADER_SERIAL_BIT_6,
	EREADER_SERIAL_BIT_7,
	EREADER_SERIAL_END_BIT,
};

enum EReaderCommand {
	EREADER_COMMAND_IDLE = 0, // TODO: Verify on hardware
	EREADER_COMMAND_WRITE_DATA = 1,
	EREADER_COMMAND_SET_INDEX = 0x22,
	EREADER_COMMAND_READ_DATA = 0x23,
};

struct EReaderCard {
	void* data;
	size_t size;
};

struct GBA;
struct GBACartEReader {
	struct GBA* p;
	uint16_t data[44];
	uint8_t serial[92];
	uint16_t registerUnk;
	uint16_t registerReset;
	EReaderControl0 registerControl0;
	EReaderControl1 registerControl1;
	uint16_t registerLed;

	// TODO: Serialize these
	enum EReaderStateMachine state;
	enum EReaderCommand command;
	uint8_t activeRegister;
	uint8_t byte;
	int scanX;
	int scanY;
	uint8_t* dots;
	struct EReaderCard cards[EREADER_CARDS_MAX];
};

struct EReaderAnchor;
struct EReaderBlock;
DECLARE_VECTOR(EReaderAnchorList, struct EReaderAnchor);
DECLARE_VECTOR(EReaderBlockList, struct EReaderBlock);

struct EReaderScan {
	uint8_t* buffer;
	unsigned width;
	unsigned height;

	uint8_t* srcBuffer;
	size_t srcWidth;
	size_t srcHeight;

	unsigned scale;

	uint8_t min;
	uint8_t max;
	uint8_t mean;
	uint8_t anchorThreshold;

	struct EReaderAnchorList anchors;
	struct EReaderBlockList blocks;
};

void GBACartEReaderInit(struct GBACartEReader* ereader);
void GBACartEReaderDeinit(struct GBACartEReader* ereader);
void GBACartEReaderWrite(struct GBACartEReader* ereader, uint32_t address, uint16_t value);
void GBACartEReaderWriteFlash(struct GBACartEReader* ereader, uint32_t address, uint8_t value);
uint16_t GBACartEReaderRead(struct GBACartEReader* ereader, uint32_t address);
uint8_t GBACartEReaderReadFlash(struct GBACartEReader* ereader, uint32_t address);
void GBACartEReaderScan(struct GBACartEReader* ereader, const void* data, size_t size);

struct EReaderScan* EReaderScanCreate(unsigned width, unsigned height);
void EReaderScanDetectParams(struct EReaderScan*);
void EReaderScanDetectAnchors(struct EReaderScan*);
void EReaderScanFilterAnchors(struct EReaderScan*);
void EReaderScanConnectAnchors(struct EReaderScan*);
void EReaderScanCreateBlocks(struct EReaderScan*);
void EReaderScanDetectBlockThreshold(struct EReaderScan*, int block);
bool EReaderScanRecalibrateBlock(struct EReaderScan*, int block);
bool EReaderScanScanBlock(struct EReaderScan*, int block, bool strict);

CXX_GUARD_END

#endif
