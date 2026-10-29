/* Copyright (c) 2013-2026 Jeffrey Pfau
 * Copyright (c) 2026 Felix Jones
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_CART_EVERDRIVE_SD_H
#define GBA_CART_EVERDRIVE_SD_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct GBA;
struct VFile;

enum {
	GBA_EVERDRIVE_SD_BASE = 0x09FC0000,
	GBA_EVERDRIVE_SD_EEP_BASE = 0x09FE0000,
	GBA_EVERDRIVE_SD_REG_SIZE = 0x200,
};

struct GBAEverdriveSD {
	struct VFile* image;

	bool readOnly;
	bool cardIdle;
	bool appCmd;
	bool readMulti;
	bool writeMulti;
	bool writeCapturing;

	uint8_t regs[0x100];
	uint8_t status;
	uint32_t currentReadSector;
	uint32_t currentWriteSector;

	uint8_t cmdPacket[6];
	int cmdPacketLen;
	uint8_t cmdResponse[32];
	int cmdResponseLen;
	int cmdResponsePos;

	uint8_t dataBlock[512];
	int dataBlockPos;
	uint8_t dataScript[32];
	int dataScriptLen;
	int dataScriptPos;
};

void GBAEverdriveSDInit(struct GBAEverdriveSD* sd);
void GBAEverdriveSDDeinit(struct GBAEverdriveSD* sd);
void GBAEverdriveSDReset(struct GBAEverdriveSD* sd);
void GBAEverdriveSDConfigure(struct GBAEverdriveSD* sd, bool enabled, const char* path);

bool GBAEverdriveSDIsActive(const struct GBAEverdriveSD* sd);
bool GBAEverdriveSDHandlesAddress(uint32_t address);

uint16_t GBAEverdriveSDRead16(struct GBAEverdriveSD* sd, uint32_t address);
void GBAEverdriveSDWrite16(struct GBAEverdriveSD* sd, uint32_t address, uint16_t value);

CXX_GUARD_END

#endif
