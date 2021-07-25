/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_EXTENSIONS_H
#define GBA_EXTENSIONS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>

#include <mgba/internal/gba/io.h>

#include <mgba-util/memory.h>

enum GBA_EXTENSIONS_IDS {
	GBAEX_ID_EXTRA_RAM = 0,
	GBAEX_EXTENSIONS_COUNT
};

#define REG_HWEX_VERSION_VALUE GBAEX_EXTENSIONS_COUNT
#define GBAEX_IO_SIZE (REG_HWEX_END - REG_HWEX0_ENABLE)

struct GBAExtensions {
	bool globalEnabled;
	bool extensionsEnabled[GBAEX_EXTENSIONS_COUNT];
	bool userGlobalEnabled;
	bool userExtensionsEnabled[GBAEX_EXTENSIONS_COUNT];

	// IO:
	uint16_t* io;
	
	// Other data
	uint8_t* extraRam;
	uint32_t extraRamSize;
	uint32_t extraRamRealSize;
};

struct GBAExtensionsStateBlockHeader {
	uint32_t id;
	uint32_t offset;
	uint32_t size;
};

struct GBAExtensionsState {
	uint32_t version;
	uint32_t extensionsBlockCount;

	struct GBAExtensionsStateBlockHeader ioBlockHeader;
	// More blocks can come after the IO one
};

struct GBA;
void GBAExtensionsInit(struct GBAExtensions* extensions);
void GBAExtensionsReset(struct GBAExtensions* extensions);
void GBAExtensionsDestroy(struct GBAExtensions* extensions);
uint16_t GBAExtensionsIORead(struct GBA* gba, uint32_t address);
uint32_t GBAExtensionsIORead32(struct GBA* gba, uint32_t address);
void GBAExtensionsIOWrite8(struct GBA* gba, uint32_t address, uint8_t value);
void GBAExtensionsIOWrite(struct GBA* gba, uint32_t address, uint16_t value);
size_t GBAExtensionsSerialize(struct GBA* gba, void** sram);
bool GBAExtensionsDeserialize(struct GBA* gba, const struct GBAExtensionsState* state, size_t size);

CXX_GUARD_END

#endif
