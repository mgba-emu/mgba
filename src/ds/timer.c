/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/timer.h>

#include <mgba/internal/arm/arm.h>
#include <mgba/internal/ds/ds.h>

static void DS7TimerUpdate0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DS* ds = context;
	struct GBATimer* timer = &ds->timers7[0];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(ds->arm7, ds->memory.io7, DS_IRQ_TIMER0);
	}
	GBATimerUpdate(timing, &ds->timers7[0], &ds->memory.io7[DS7_REG_TM0CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &ds->timers7[1], &ds->memory.io7[DS7_REG_TM1CNT_LO >> 1], cyclesLate);
}

static void DS7TimerUpdate1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DS* ds = context;
	struct GBATimer* timer = &ds->timers7[1];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(ds->arm7, ds->memory.io7, DS_IRQ_TIMER1);
	}
	GBATimerUpdate(timing, &ds->timers7[1], &ds->memory.io7[DS7_REG_TM1CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &ds->timers7[2], &ds->memory.io7[DS7_REG_TM2CNT_LO >> 1], cyclesLate);
}

static void DS7TimerUpdate2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DS* ds = context;
	struct GBATimer* timer = &ds->timers7[2];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(ds->arm7, ds->memory.io7, DS_IRQ_TIMER2);
	}
	GBATimerUpdate(timing, &ds->timers7[2], &ds->memory.io7[DS7_REG_TM2CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &ds->timers7[3], &ds->memory.io7[DS7_REG_TM3CNT_LO >> 1], cyclesLate);
}

static void DS7TimerUpdate3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DS* ds = context;
	struct GBATimer* timer = &ds->timers7[3];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		DSRaiseIRQ(ds->arm7, ds->memory.io7, DS_IRQ_TIMER3);
	}
	GBATimerUpdate(timing, &ds->timers7[3], &ds->memory.io7[DS7_REG_TM3CNT_LO >> 1], cyclesLate);
}

void DSTimerInit(struct DS* ds) {
	memset(ds->timers7, 0, sizeof(ds->timers7));
	ds->timers7[0].event.name = "DS7 Timer 0";
	ds->timers7[0].event.callback = DS7TimerUpdate0;
	ds->timers7[0].event.context = ds;
	ds->timers7[0].event.priority = 0x20;
	ds->timers7[1].event.name = "DS7 Timer 1";
	ds->timers7[1].event.callback = DS7TimerUpdate1;
	ds->timers7[1].event.context = ds;
	ds->timers7[1].event.priority = 0x21;
	ds->timers7[2].event.name = "DS7 Timer 2";
	ds->timers7[2].event.callback = DS7TimerUpdate2;
	ds->timers7[2].event.context = ds;
	ds->timers7[2].event.priority = 0x22;
	ds->timers7[3].event.name = "DS7 Timer 3";
	ds->timers7[3].event.callback = DS7TimerUpdate3;
	ds->timers7[3].event.context = ds;
	ds->timers7[3].event.priority = 0x23;

	memset(ds->timers9, 0, sizeof(ds->timers9));
	ds->timers9[0].event.name = "DS9 Timer 0";
	ds->timers9[0].event.callback = NULL;
	ds->timers9[0].event.context = ds;
	ds->timers9[0].event.priority = 0x20;
	ds->timers9[1].event.name = "DS9 Timer 1";
	ds->timers9[1].event.callback = NULL;
	ds->timers9[1].event.context = ds;
	ds->timers9[1].event.priority = 0x21;
	ds->timers9[2].event.name = "DS9 Timer 2";
	ds->timers9[2].event.callback = NULL;
	ds->timers9[2].event.context = ds;
	ds->timers9[2].event.priority = 0x22;
	ds->timers9[3].event.name = "DS9 Timer 3";
	ds->timers9[3].event.callback = NULL;
	ds->timers9[3].event.context = ds;
	ds->timers9[3].event.priority = 0x23;
}

void DSTimerWriteTMCNT_HI(struct GBATimer* timer, struct mTiming* timing, struct ARMCore* cpu, uint16_t* io, uint16_t value) {
	GBATimerUpdateRegisterInternal(timer, timing, cpu, io, 0);
	GBATimerWriteTMCNT_HI(timer, timing, cpu, io, value);
}
