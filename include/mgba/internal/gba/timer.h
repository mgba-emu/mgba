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

struct GBATimer {
	uint16_t reload;
	uint16_t oldReload;
	uint32_t lastEvent;
	struct mTimingEvent event;
	int32_t overflowInterval;
	GBATimerFlags flags;
};

struct ARMCore;
struct GBA;
void GBATimerInit(struct GBA* gba);
void GBATimerWriteTMCNT_LO(struct GBATimer* timer, uint16_t reload);
void GBATimerWriteTMCNT_HI(struct GBATimer* timer, struct mTiming* timing, struct ARMCore* cpu, uint16_t* io, uint16_t control);

void GBATimerUpdateRegister(struct GBA* gba, int timer);
void GBATimerUpdateRegisterInternal(struct GBATimer* timer, struct mTiming* timing, struct ARMCore* cpu, uint16_t* io, int32_t skew);
void GBATimerUpdateCountUp(struct mTiming* timing, struct GBATimer* nextTimer, uint16_t* io, uint32_t cyclesLate);
void GBATimerUpdate(struct mTiming* timing, struct GBATimer* timer, uint16_t* io, uint32_t cyclesLate);

CXX_GUARD_END

#endif
