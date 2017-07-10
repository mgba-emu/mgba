/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/timer.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

static void GBATimerUpdate(struct mTiming* timing, struct GBA* gba, int timerId, uint32_t cyclesLate) {
	struct GBATimer* timer = &gba->timers[timerId];
	gba->memory.io[(REG_TM0CNT_LO >> 1) + (timerId << 1)] = timer->reload;
	timer->oldReload = timer->reload;
	timer->lastEvent = timing->masterCycles - cyclesLate;

	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		GBARaiseIRQ(gba, IRQ_TIMER0 + timerId);
	}

	if (gba->audio.enable && timerId < 2) {
		if ((gba->audio.chALeft || gba->audio.chARight) && gba->audio.chATimer == timerId) {
			GBAAudioSampleFIFO(&gba->audio, 0, cyclesLate);
		}

		if ((gba->audio.chBLeft || gba->audio.chBRight) && gba->audio.chBTimer == timerId) {
			GBAAudioSampleFIFO(&gba->audio, 1, cyclesLate);
		}
	}

	if (timerId < 3) {
		struct GBATimer* nextTimer = &gba->timers[timerId + 1];
		if (GBATimerFlagsIsCountUp(nextTimer->flags)) { // TODO: Does this increment while disabled?
			++gba->memory.io[(REG_TM1CNT_LO >> 1) + (timerId << 1)];
			if (!gba->memory.io[(REG_TM1CNT_LO >> 1) + (timerId << 1)] && GBATimerFlagsIsEnable(nextTimer->flags)) {
				mTimingSchedule(timing, &nextTimer->event, -cyclesLate);
			}
		}
	}

	if (!GBATimerFlagsIsCountUp(timer->flags)) {
		uint32_t nextEvent = timer->overflowInterval - cyclesLate;
		mTimingSchedule(timing, &timer->event, nextEvent);
	}
}

static void GBATimerUpdate0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	GBATimerUpdate(timing, context, 0, cyclesLate);
}

static void GBATimerUpdate1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	GBATimerUpdate(timing, context, 1, cyclesLate);
}

static void GBATimerUpdate2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	GBATimerUpdate(timing, context, 2, cyclesLate);
}

static void GBATimerUpdate3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	GBATimerUpdate(timing, context, 3, cyclesLate);
}

void GBATimerInit(struct GBA* gba) {
	memset(gba->timers, 0, sizeof(gba->timers));
	gba->timers[0].event.name = "GBA Timer 0";
	gba->timers[0].event.callback = GBATimerUpdate0;
	gba->timers[0].event.context = gba;
	gba->timers[0].event.priority = 0x20;
	gba->timers[1].event.name = "GBA Timer 1";
	gba->timers[1].event.callback = GBATimerUpdate1;
	gba->timers[1].event.context = gba;
	gba->timers[1].event.priority = 0x21;
	gba->timers[2].event.name = "GBA Timer 2";
	gba->timers[2].event.callback = GBATimerUpdate2;
	gba->timers[2].event.context = gba;
	gba->timers[2].event.priority = 0x22;
	gba->timers[3].event.name = "GBA Timer 3";
	gba->timers[3].event.callback = GBATimerUpdate3;
	gba->timers[3].event.context = gba;
	gba->timers[3].event.priority = 0x23;
}

void GBATimerUpdateRegister(struct GBA* gba, int timer) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	if (GBATimerFlagsIsEnable(currentTimer->flags) && !GBATimerFlagsIsCountUp(currentTimer->flags)) {
		int32_t prefetchSkew = -2;
		if (gba->memory.lastPrefetchedPc > (uint32_t) gba->cpu->gprs[ARM_PC]) {
			prefetchSkew += ((gba->memory.lastPrefetchedPc - gba->cpu->gprs[ARM_PC]) * gba->cpu->memory.activeSeqCycles16) / WORD_SIZE_THUMB;
		}
		// Reading this takes two cycles (1N+1I), so let's remove them preemptively
		int32_t diff = gba->cpu->cycles - (currentTimer->lastEvent - gba->timing.masterCycles);
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->oldReload + ((diff + prefetchSkew) >> GBATimerFlagsGetPrescaleBits(currentTimer->flags));
	}
}

void GBATimerWriteTMCNT_LO(struct GBA* gba, int timer, uint16_t reload) {
	gba->timers[timer].reload = reload;
	gba->timers[timer].overflowInterval = (0x10000 - gba->timers[timer].reload) << GBATimerFlagsGetPrescaleBits(gba->timers[timer].flags);
}

void GBATimerWriteTMCNT_HI(struct GBA* gba, int timer, uint16_t control) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	GBATimerUpdateRegister(gba, timer);

	unsigned oldPrescale = GBATimerFlagsGetPrescaleBits(currentTimer->flags);
	unsigned prescaleBits;
	switch (control & 0x0003) {
	case 0x0000:
		prescaleBits = 0;
		break;
	case 0x0001:
		prescaleBits = 6;
		break;
	case 0x0002:
		prescaleBits = 8;
		break;
	case 0x0003:
		prescaleBits = 10;
		break;
	}
	currentTimer->flags = GBATimerFlagsSetPrescaleBits(currentTimer->flags, prescaleBits);
	currentTimer->flags = GBATimerFlagsTestFillCountUp(currentTimer->flags, timer > 0 && (control & 0x0004));
	currentTimer->flags = GBATimerFlagsTestFillDoIrq(currentTimer->flags, control & 0x0040);
	currentTimer->overflowInterval = (0x10000 - currentTimer->reload) << GBATimerFlagsGetPrescaleBits(currentTimer->flags);
	bool wasEnabled = GBATimerFlagsIsEnable(currentTimer->flags);
	currentTimer->flags = GBATimerFlagsTestFillEnable(currentTimer->flags, control & 0x0080);
	if (!wasEnabled && GBATimerFlagsIsEnable(currentTimer->flags)) {
		mTimingDeschedule(&gba->timing, &currentTimer->event);
		if (!GBATimerFlagsIsCountUp(currentTimer->flags)) {
			mTimingSchedule(&gba->timing, &currentTimer->event, currentTimer->overflowInterval + 7 - 6 * prescaleBits);
		}
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->reload;
		currentTimer->oldReload = currentTimer->reload;
		currentTimer->lastEvent = gba->timing.masterCycles + gba->cpu->cycles;
	} else if (wasEnabled && !GBATimerFlagsIsEnable(currentTimer->flags)) {
		mTimingDeschedule(&gba->timing, &currentTimer->event);
	} else if (GBATimerFlagsIsEnable(currentTimer->flags) && GBATimerFlagsGetPrescaleBits(currentTimer->flags) != oldPrescale && !GBATimerFlagsIsCountUp(currentTimer->flags)) {
		mTimingDeschedule(&gba->timing, &currentTimer->event);
		mTimingSchedule(&gba->timing, &currentTimer->event, currentTimer->overflowInterval - currentTimer->lastEvent);
	}
}
