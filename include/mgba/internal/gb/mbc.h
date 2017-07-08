/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_MBC_H
#define GB_MBC_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>

mLOG_DECLARE_CATEGORY(GB_MBC);

struct GB;
struct GBMemory;
void GBMBCInit(struct GB* gb);
void GBMBCSwitchBank(struct GB* gb, int bank);
void GBMBCSwitchBank0(struct GB* gb, int bank);
void GBMBCSwitchSramBank(struct GB* gb, int bank);

struct GBMBCRTCSaveBuffer {
	uint32_t sec;
	uint32_t min;
	uint32_t hour;
	uint32_t days;
	uint32_t daysHi;
	uint32_t latchedSec;
	uint32_t latchedMin;
	uint32_t latchedHour;
	uint32_t latchedDays;
	uint32_t latchedDaysHi;
	uint64_t unixTime;
};
void GBMBCRTCRead(struct GB* gb);
void GBMBCRTCWrite(struct GB* gb);

void GBMBC7Write(struct GBMemory*, uint16_t address, uint8_t value);

CXX_GUARD_END

#endif
