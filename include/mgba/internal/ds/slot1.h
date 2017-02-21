/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_SLOT1_H
#define DS_SLOT1_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(DS_SLOT1);

DECL_BITFIELD(DSSlot1AUXSPICNT, uint16_t);

DECL_BITFIELD(DSSlot1ROMCNT, uint32_t);
DECL_BIT(DSSlot1ROMCNT, WordReady, 23);
DECL_BITS(DSSlot1ROMCNT, BlockSize, 24, 3);
DECL_BIT(DSSlot1ROMCNT, BlockBusy, 31);

struct DSSlot1 {
	uint8_t command[8];
	uint32_t address;
	uint32_t transferSize;
	uint32_t transferRemaining;
	uint8_t readBuffer[4];
};

struct DS;
DSSlot1AUXSPICNT DSSlot1Configure(struct DS* ds, DSSlot1AUXSPICNT config);
DSSlot1ROMCNT DSSlot1Control(struct DS* ds, DSSlot1ROMCNT control);
uint32_t DSSlot1Read(struct DS* ds);

CXX_GUARD_END

#endif
