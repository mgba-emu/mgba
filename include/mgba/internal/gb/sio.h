/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GB_SIO_H
#define GB_SIO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>
#include <mgba/gb/interface.h>

#define MAX_GBS 2

extern const int GBSIOCyclesPerTransfer[2];

mLOG_DECLARE_CATEGORY(GB_SIO);

struct GB;
struct GBSIODriver;
struct GBSIO {
	struct GB* p;

	struct mTimingEvent event;
	struct GBSIODriver* driver;

	int32_t nextEvent;
	int32_t period;
	int remainingBits;

	uint8_t pendingSB;
};

DECL_BITFIELD(GBRegisterSC, uint8_t);
DECL_BIT(GBRegisterSC, ShiftClock, 0);
DECL_BIT(GBRegisterSC, ClockSpeed, 1);
DECL_BIT(GBRegisterSC, Enable, 7);

void GBSIOInit(struct GBSIO* sio);
void GBSIOReset(struct GBSIO* sio);
void GBSIODeinit(struct GBSIO* sio);
void GBSIOSetDriver(struct GBSIO* sio, struct GBSIODriver* driver);
void GBSIOWriteSC(struct GBSIO* sio, uint8_t sc);
void GBSIOWriteSB(struct GBSIO* sio, uint8_t sb);

CXX_GUARD_END

#endif
