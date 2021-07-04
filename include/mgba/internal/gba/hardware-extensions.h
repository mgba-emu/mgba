/* Copyright (c) 2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef HARDWARE_EXTENSIONS_H
#define HARDWARE_EXTENSIONS_H

#include <mgba-util/common.h>

#include <mgba/core/log.h>
#include <mgba/core/timing.h>

#include <mgba/internal/gba/io.h>


#define HWEX_EXTENSIONS_COUNT  1 
#define REG_HWEX_VERSION_VALUE HWEX_EXTENSIONS_COUNT
#define HWEX_MORE_RAM_SIZE 0x100000 // 1 MB

struct GBAHardwareExtensions {
    bool enabled;
    bool userEnabled;
    uint16_t userEnabledFlags[5];

    // IO:
    uint32_t memory[(REG_HWEX_END - REG_HWEX_ENABLE) / sizeof(uint32_t)];
    
    // Other data
    uint32_t moreRam[HWEX_MORE_RAM_SIZE / sizeof(uint32_t)];
};

struct GBA;
void GBAHardwareExtensionsInit(struct GBAHardwareExtensions* hw);
uint16_t GBAHardwareExtensionsIORead(struct GBA* gba, uint32_t address);
uint32_t GBAHardwareExtensionsIORead32(struct GBA* gba, uint32_t address);
void GBAHardwareExtensionsIOWrite8(struct GBA* gba, uint32_t address, uint8_t value);
void GBAHardwareExtensionsIOWrite(struct GBA* gba, uint32_t address, uint16_t value);
void GBAHardwareExtensionsIOWrite32(struct GBA* gba, uint32_t address, uint32_t value);

#endif
