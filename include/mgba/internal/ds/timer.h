/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_TIMER_H
#define DS_TIMER_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/internal/gba/timer.h>

struct ARMCore;
struct DS;
void DS7TimerInit(struct DS* ds);
void DS9TimerInit(struct DS* ds);
void DSTimerWriteTMCNT_HI(struct GBATimer* timer, struct mTiming* timing, uint16_t* io, uint16_t control);

CXX_GUARD_END

#endif
