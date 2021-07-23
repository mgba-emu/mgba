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
#include <mgba/internal/gba/extra/extensions-ids.h>

#define REG_HWEX_VERSION_VALUE GBAEX_EXTENSIONS_COUNT
#define HWEX_MORE_RAM_SIZE 0x100000 // 1 MB

struct GBAExtensions {
	bool globalEnabled;
	bool extensionsEnabled[GBAEX_EXTENSIONS_COUNT];
	bool userGlobalEnabled;
	bool userExtensionsEnabled[GBAEX_EXTENSIONS_COUNT];

	// IO:
	uint16_t io[(REG_HWEX_END - REG_HWEX0_ENABLE) / 2];
	
	// Other data
	uint32_t extraRam[HWEX_MORE_RAM_SIZE / sizeof(uint32_t)];
};

struct GBAExtensionsState {
	uint32_t globalEnabled; // boolean
	uint32_t version;

	// IO:
	uint32_t memory[128];
	
	// Other data
	uint32_t extraRam[HWEX_MORE_RAM_SIZE / sizeof(uint32_t)];
};

struct GBA;
void GBAExtensionsInit(struct GBAExtensions* hw);
uint16_t GBAExtensionsIORead(struct GBA* gba, uint32_t address);
uint32_t GBAExtensionsIORead32(struct GBA* gba, uint32_t address);
void GBAExtensionsIOWrite8(struct GBA* gba, uint32_t address, uint8_t value);
void GBAExtensionsIOWrite(struct GBA* gba, uint32_t address, uint16_t value);
bool GBAExtensionsSerialize(struct GBA* gba, struct GBAExtensionsState* state);
bool GBAExtensionsDeserialize(struct GBA* gba, const struct GBAExtensionsState* state, size_t size);

CXX_GUARD_END

#endif
