/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_TIMER_H
#define GBA_TIMER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/timing.h>

DECL_BITFIELD(GBATimerFlags, uint32_t);
DECL_BITS(GBATimerFlags, PrescaleBits, 0, 4);
DECL_BIT(GBATimerFlags, CountUp, 4);
DECL_BIT(GBATimerFlags, DoIrq, 5);
DECL_BIT(GBATimerFlags, Enable, 6);

struct GBA;
struct GBATimer {
	uint16_t reload;
	int32_t lastEvent;
	struct mTimingEvent event;
	GBATimerFlags flags;
};

void GBATimerInit(struct GBA* gba);
void GBATimerUpdateRegister(struct GBA* gba, int timer, int32_t cyclesLate);
void GBATimerWriteTMCNT_LO(struct GBA* gba, int timer, uint16_t value);
void GBATimerWriteTMCNT_HI(struct GBA* gba, int timer, uint16_t value);

CXX_GUARD_END

#endif
