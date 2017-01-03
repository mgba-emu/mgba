/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/timer.h>

#include <mgba/internal/arm/arm.h>
#include <mgba/internal/ds/ds.h>

static void DSTimerUpdate0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	struct GBATimer* timer = &dscore->timers[0];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(dscore->cpu, dscore->memory.io, DS_IRQ_TIMER0);
	}
	GBATimerUpdate(timing, &dscore->timers[0], &dscore->memory.io[DS_REG_TM0CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &dscore->timers[1], &dscore->memory.io[DS_REG_TM1CNT_LO >> 1], cyclesLate);
}

static void DSTimerUpdate1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	struct GBATimer* timer = &dscore->timers[1];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(dscore->cpu, dscore->memory.io, DS_IRQ_TIMER1);
	}
	GBATimerUpdate(timing, &dscore->timers[1], &dscore->memory.io[DS_REG_TM1CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &dscore->timers[2], &dscore->memory.io[DS_REG_TM2CNT_LO >> 1], cyclesLate);
}

static void DSTimerUpdate2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	struct GBATimer* timer = &dscore->timers[2];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(dscore->cpu, dscore->memory.io, DS_IRQ_TIMER2);
	}
	GBATimerUpdate(timing, &dscore->timers[2], &dscore->memory.io[DS_REG_TM2CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &dscore->timers[3], &dscore->memory.io[DS_REG_TM3CNT_LO >> 1], cyclesLate);
}

static void DSTimerUpdate3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	struct GBATimer* timer = &dscore->timers[3];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(dscore->cpu, dscore->memory.io, DS_IRQ_TIMER3);
	}
	GBATimerUpdate(timing, &dscore->timers[3], &dscore->memory.io[DS_REG_TM3CNT_LO >> 1], cyclesLate);
}

void DSTimerInit(struct DS* ds) {
	memset(ds->ds7.timers, 0, sizeof(ds->ds7.timers));
	ds->ds7.timers[0].event.name = "DS7 Timer 0";
	ds->ds7.timers[0].event.callback = DSTimerUpdate0;
	ds->ds7.timers[0].event.context = &ds->ds7;
	ds->ds7.timers[0].event.priority = 0x20;
	ds->ds7.timers[1].event.name = "DS7 Timer 1";
	ds->ds7.timers[0].event.callback = DSTimerUpdate1;
	ds->ds7.timers[0].event.context = &ds->ds7;
	ds->ds7.timers[1].event.priority = 0x21;
	ds->ds7.timers[2].event.name = "DS7 Timer 2";
	ds->ds7.timers[0].event.callback = DSTimerUpdate2;
	ds->ds7.timers[0].event.context = &ds->ds7;
	ds->ds7.timers[2].event.priority = 0x22;
	ds->ds7.timers[3].event.name = "DS7 Timer 3";
	ds->ds7.timers[0].event.callback = DSTimerUpdate3;
	ds->ds7.timers[0].event.context = &ds->ds7;
	ds->ds7.timers[3].event.priority = 0x23;

	memset(ds->ds9.timers, 0, sizeof(ds->ds9.timers));
	ds->ds9.timers[0].event.name = "DS9 Timer 0";
	ds->ds9.timers[0].event.callback = DSTimerUpdate0;
	ds->ds9.timers[0].event.context = ds;
	ds->ds9.timers[0].event.priority = 0x20;
	ds->ds9.timers[1].event.name = "DS9 Timer 1";
	ds->ds9.timers[1].event.callback = DSTimerUpdate1;
	ds->ds9.timers[1].event.context = ds;
	ds->ds9.timers[1].event.priority = 0x21;
	ds->ds9.timers[2].event.name = "DS9 Timer 2";
	ds->ds9.timers[2].event.callback = DSTimerUpdate2;
	ds->ds9.timers[2].event.context = ds;
	ds->ds9.timers[2].event.priority = 0x22;
	ds->ds9.timers[3].event.name = "DS9 Timer 3";
	ds->ds9.timers[3].event.callback = DSTimerUpdate3;
	ds->ds9.timers[3].event.context = ds;
	ds->ds9.timers[3].event.priority = 0x23;
}

void DSTimerWriteTMCNT_HI(struct GBATimer* timer, struct mTiming* timing, struct ARMCore* cpu, uint16_t* io, uint16_t value) {
	GBATimerUpdateRegisterInternal(timer, timing, cpu, io, 0);
	GBATimerWriteTMCNT_HI(timer, timing, cpu, io, value);
}
