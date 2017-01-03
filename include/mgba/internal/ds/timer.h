/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_TIMER_H
#define DS_TIMER_H

#include <mgba-util/common.h>

CXX_GUARD_START

DECL_BITFIELD(DSTimerFlags, uint32_t);
DECL_BITS(DSTimerFlags, PrescaleBits, 0, 4);
DECL_BIT(DSTimerFlags, CountUp, 4);
DECL_BIT(DSTimerFlags, DoIrq, 5);
DECL_BIT(DSTimerFlags, Enable, 6);

struct DSTimer {
	uint16_t reload;
	uint16_t oldReload;
	int32_t lastEvent;
	int32_t nextEvent;
	int32_t overflowInterval;
	DSTimerFlags flags;
};

// TODO: Merge back into GBATimer
struct ARMCore;
void DSTimerUpdateRegister(struct DSTimer* timer, struct ARMCore* cpu, uint16_t* io);
void DSTimerWriteTMCNT_LO(struct DSTimer* timer, uint16_t reload);
void DSTimerWriteTMCNT_HI(struct DSTimer* timer, struct ARMCore* cpu, uint16_t* io, uint16_t control);

struct DS;
int32_t DSTimersProcessEvents(struct DS* ds, int32_t cycles);

CXX_GUARD_END

#endif
