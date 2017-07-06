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
	if (GBATimerFlagsIsIrqPending(timer->flags)) {
		timer->flags = GBATimerFlagsClearIrqPending(timer->flags);
		DSRaiseIRQ(dscore->cpu, dscore->memory.io, DS_IRQ_TIMER0 + timerId);
	}
}

static void DSTimerIrq0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	DSTimerIrq(context, 0);
}

static void DSTimerIrq1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	DSTimerIrq(context, 1);
}

static void DSTimerIrq2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	DSTimerIrq(context, 2);
}

static void DSTimerIrq3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	DSTimerIrq(context, 3);
}

static void DSTimerUpdate0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	GBATimerUpdate(timing, &dscore->timers[0], &dscore->memory.io[DS_REG_TM0CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &dscore->timers[1], &dscore->memory.io[DS_REG_TM1CNT_LO >> 1], cyclesLate);
}

static void DSTimerUpdate1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	GBATimerUpdate(timing, &dscore->timers[1], &dscore->memory.io[DS_REG_TM1CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &dscore->timers[2], &dscore->memory.io[DS_REG_TM2CNT_LO >> 1], cyclesLate);
}

static void DSTimerUpdate2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	GBATimerUpdate(timing, &dscore->timers[2], &dscore->memory.io[DS_REG_TM2CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &dscore->timers[3], &dscore->memory.io[DS_REG_TM3CNT_LO >> 1], cyclesLate);
}

static void DSTimerUpdate3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DSCommon* dscore = context;
	GBATimerUpdate(timing, &dscore->timers[3], &dscore->memory.io[DS_REG_TM3CNT_LO >> 1], cyclesLate);
}

void DSTimerInit(struct DS* ds) {
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
	ds->ds7.timers[0].irq.name = "DS7 Timer 0 IRQ";
	ds->ds7.timers[0].irq.callback = DSTimerIrq0;
	ds->ds7.timers[0].irq.context = &ds->ds7;
	ds->ds7.timers[0].irq.priority = 0x28;
	ds->ds7.timers[1].irq.name = "DS7 Timer 1 IRQ";
	ds->ds7.timers[1].irq.callback = DSTimerIrq1;
	ds->ds7.timers[1].irq.context = &ds->ds7;
	ds->ds7.timers[1].irq.priority = 0x29;
	ds->ds7.timers[2].irq.name = "DS7 Timer 2 IRQ";
	ds->ds7.timers[2].irq.callback = DSTimerIrq2;
	ds->ds7.timers[2].irq.context = &ds->ds7;
	ds->ds7.timers[2].irq.priority = 0x2A;
	ds->ds7.timers[3].irq.name = "DS7 Timer 3 IRQ";
	ds->ds7.timers[3].irq.callback = DSTimerIrq3;
	ds->ds7.timers[3].irq.context = &ds->ds7;
	ds->ds7.timers[3].irq.priority = 0x2B;

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
	ds->ds9.timers[0].irq.name = "DS9 Timer 0 IRQ";
	ds->ds9.timers[0].irq.callback = DSTimerIrq0;
	ds->ds9.timers[0].irq.context = &ds->ds9;
	ds->ds9.timers[0].irq.priority = 0x28;
	ds->ds9.timers[1].irq.name = "DS9 Timer 1 IRQ";
	ds->ds9.timers[1].irq.callback = DSTimerIrq1;
	ds->ds9.timers[1].irq.context = &ds->ds9;
	ds->ds9.timers[1].irq.priority = 0x29;
	ds->ds9.timers[2].irq.name = "DS9 Timer 2 IRQ";
	ds->ds9.timers[2].irq.callback = DSTimerIrq2;
	ds->ds9.timers[2].irq.context = &ds->ds9;
	ds->ds9.timers[2].irq.priority = 0x2A;
	ds->ds9.timers[3].irq.name = "DS9 Timer 3 IRQ";
	ds->ds9.timers[3].irq.callback = DSTimerIrq3;
	ds->ds9.timers[3].irq.context = &ds->ds9;
	ds->ds9.timers[3].irq.priority = 0x2B;
}

void DSTimerWriteTMCNT_HI(struct GBATimer* timer, struct mTiming* timing, uint16_t* io, uint16_t value) {
	GBATimerWriteTMCNT_HI(timer, timing, io, value);
}
