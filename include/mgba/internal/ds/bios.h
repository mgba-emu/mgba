/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_BIOS_H
#define DS_BIOS_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(DS_BIOS);

struct ARMCore;
void DS7Swi16(struct ARMCore* cpu, int immediate);
void DS7Swi32(struct ARMCore* cpu, int immediate);
void DS9Swi16(struct ARMCore* cpu, int immediate);
void DS9Swi32(struct ARMCore* cpu, int immediate);

extern const uint32_t DS7_BIOS_CHECKSUM;
extern const uint32_t DS9_BIOS_CHECKSUM;

CXX_GUARD_END

#endif
