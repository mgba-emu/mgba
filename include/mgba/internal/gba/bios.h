/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_BIOS_H
#define GBA_BIOS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(GBA_BIOS);

struct ARMCore;
void GBASwi16(struct ARMCore* cpu, int immediate);
void GBASwi32(struct ARMCore* cpu, int immediate);

uint32_t GBAChecksum(uint32_t* memory, size_t size);
extern const uint32_t GBA_BIOS_CHECKSUM;
extern const uint32_t GBA_DS_BIOS_CHECKSUM;

CXX_GUARD_END

#endif
