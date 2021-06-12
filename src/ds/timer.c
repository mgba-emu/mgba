/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/timer.h>

#include <mgba/internal/arm/arm.h>
#include <mgba/internal/ds/ds.h>

static void DSTimerIrq(struct DSCommon* dscore, int timerId) {
	struct GBATimer* timer = &dscore->timers[timerId];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(dscore->cpu, dscore->memory.io, DS_IRQ_TIMER0 + timerId);
	}
}

static void DSTimerUpdate0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	GBATimerUpdate(timing, &dscore->timers[0], &dscore->memory.io[DS_REG_TM0CNT_LO >> 1], cyclesLate);
	DSTimerIrq(dscore, 0);
	if (GBATimerUpdateCountUp(timing, &dscore->timers[1], &dscore->memory.io[DS_REG_TM1CNT_LO >> 1], cyclesLate)) {
		DSTimerIrq(dscore, 1);
	}
}

static void DSTimerUpdate1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	GBATimerUpdate(timing, &dscore->timers[1], &dscore->memory.io[DS_REG_TM1CNT_LO >> 1], cyclesLate);
	DSTimerIrq(dscore, 1);
	if (GBATimerUpdateCountUp(timing, &dscore->timers[2], &dscore->memory.io[DS_REG_TM2CNT_LO >> 1], cyclesLate)) {
		DSTimerIrq(dscore, 2);
	}
}

static void DSTimerUpdate2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	GBATimerUpdate(timing, &dscore->timers[2], &dscore->memory.io[DS_REG_TM2CNT_LO >> 1], cyclesLate);
	DSTimerIrq(dscore, 2);
	if (GBATimerUpdateCountUp(timing, &dscore->timers[3], &dscore->memory.io[DS_REG_TM3CNT_LO >> 1], cyclesLate)) {
		DSTimerIrq(dscore, 3);
	}
}

static void DSTimerUpdate3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	GBATimerUpdate(timing, &dscore->timers[3], &dscore->memory.io[DS_REG_TM3CNT_LO >> 1], cyclesLate);
	DSTimerIrq(dscore, 3);
}

void DS7TimerInit(struct DS* ds) {
	memset(ds->ds7.timers, 0, sizeof(ds->ds7.timers));
	ds->ds7.timers[0].event.name = "DS7 Timer 0";
	ds->ds7.timers[0].event.callback = DSTimerUpdate0;
	ds->ds7.timers[0].event.context = &ds->ds7;
	ds->ds7.timers[0].event.priority = 0x20;
	ds->ds7.timers[1].event.name = "DS7 Timer 1";
	ds->ds7.timers[1].event.callback = DSTimerUpdate1;
	ds->ds7.timers[1].event.context = &ds->ds7;
	ds->ds7.timers[1].event.priority = 0x21;
	ds->ds7.timers[2].event.name = "DS7 Timer 2";
	ds->ds7.timers[2].event.callback = DSTimerUpdate2;
	ds->ds7.timers[2].event.context = &ds->ds7;
	ds->ds7.timers[2].event.priority = 0x22;
	ds->ds7.timers[3].event.name = "DS7 Timer 3";
	ds->ds7.timers[3].event.callback = DSTimerUpdate3;
	ds->ds7.timers[3].event.context = &ds->ds7;
	ds->ds7.timers[3].event.priority = 0x23;
}

void DS9TimerInit(struct DS* ds) {
	memset(ds->ds9.timers, 0, sizeof(ds->ds9.timers));
	ds->ds9.timers[0].event.name = "DS9 Timer 0";
	ds->ds9.timers[0].event.callback = DSTimerUpdate0;
	ds->ds9.timers[0].event.context = &ds->ds9;
	ds->ds9.timers[0].event.priority = 0x20;
	ds->ds9.timers[0].forcedPrescale = 1;
	ds->ds9.timers[1].event.name = "DS9 Timer 1";
	ds->ds9.timers[1].event.callback = DSTimerUpdate1;
	ds->ds9.timers[1].event.context = &ds->ds9;
	ds->ds9.timers[1].event.priority = 0x21;
	ds->ds9.timers[1].forcedPrescale = 1;
	ds->ds9.timers[2].event.name = "DS9 Timer 2";
	ds->ds9.timers[2].event.callback = DSTimerUpdate2;
	ds->ds9.timers[2].event.context = &ds->ds9;
	ds->ds9.timers[2].event.priority = 0x22;
	ds->ds9.timers[2].forcedPrescale = 1;
	ds->ds9.timers[3].event.name = "DS9 Timer 3";
	ds->ds9.timers[3].event.callback = DSTimerUpdate3;
	ds->ds9.timers[3].event.context = &ds->ds9;
	ds->ds9.timers[3].event.priority = 0x23;
	ds->ds9.timers[3].forcedPrescale = 1;
}

void DSTimerWriteTMCNT_HI(struct GBATimer* timer, struct mTiming* timing, uint16_t* io, uint16_t value) {
	GBATimerWriteTMCNT_HI(timer, timing, io, value);
}
