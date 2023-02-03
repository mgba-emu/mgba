/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/timer.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#define REG_TMCNT_LO(X) (REG_TM0CNT_LO + ((X) << 2))

static void GBATimerUpdate(struct GBA* gba, int timerId, uint32_t cyclesLate) {
	struct GBATimer* timer = &gba->timers[timerId];
	if (GBATimerFlagsIsCountUp(timer->flags)) {
		gba->memory.io[REG_TMCNT_LO(timerId) >> 1] = timer->reload;
	} else {
		GBATimerUpdateRegister(gba, timerId, cyclesLate);
	}

	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		GBARaiseIRQ(gba, GBA_IRQ_TIMER0 + timerId, cyclesLate);
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
		if (GBATimerFlagsIsCountUp(nextTimer->flags) && GBATimerFlagsIsEnable(nextTimer->flags)) {
			++gba->memory.io[REG_TMCNT_LO(timerId + 1) >> 1];
			if (!gba->memory.io[REG_TMCNT_LO(timerId + 1) >> 1] && GBATimerFlagsIsEnable(nextTimer->flags)) {
				GBATimerUpdate(gba, timerId + 1, cyclesLate);
			}
		}
	}
}

static void GBATimerUpdate0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	GBATimerUpdate(context, 0, cyclesLate);
}

static void GBATimerUpdate1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	GBATimerUpdate(context, 1, cyclesLate);
}

static void GBATimerUpdate2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	GBATimerUpdate(context, 2, cyclesLate);
}

static void GBATimerUpdate3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	GBATimerUpdate(context, 3, cyclesLate);
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

void GBATimerUpdateRegister(struct GBA* gba, int timer, int32_t cyclesLate) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	if (!GBATimerFlagsIsEnable(currentTimer->flags) || GBATimerFlagsIsCountUp(currentTimer->flags)) {
		return;
	}

	// Align timer
	int prescaleBits = GBATimerFlagsGetPrescaleBits(currentTimer->flags);
	int32_t currentTime = mTimingCurrentTime(&gba->timing) - cyclesLate;
	int32_t tickMask = (1 << prescaleBits) - 1;
	currentTime &= ~tickMask;

	// Update register
	int32_t tickIncrement = currentTime - currentTimer->lastEvent;
	currentTimer->lastEvent = currentTime;
	tickIncrement >>= prescaleBits;
	tickIncrement += gba->memory.io[REG_TMCNT_LO(timer) >> 1];
	while (tickIncrement >= 0x10000) {
		tickIncrement -= 0x10000 - currentTimer->reload;
	}
	gba->memory.io[REG_TMCNT_LO(timer) >> 1] = tickIncrement;

	// Schedule next update
	tickIncrement = (0x10000 - tickIncrement) << prescaleBits;
	currentTime += tickIncrement;
	currentTime &= ~tickMask;
	mTimingDeschedule(&gba->timing, &currentTimer->event);
	mTimingScheduleAbsolute(&gba->timing, &currentTimer->event, currentTime);
}

void GBATimerWriteTMCNT_LO(struct GBA* gba, int timer, uint16_t reload) {
	gba->timers[timer].reload = reload;
}

void GBATimerWriteTMCNT_HI(struct GBA* gba, int timer, uint16_t control) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	GBATimerUpdateRegister(gba, timer, 0);

	const unsigned prescaleTable[4] = { 0, 6, 8, 10 };
	unsigned prescaleBits = prescaleTable[control & 0x0003];

	GBATimerFlags oldFlags = currentTimer->flags;
	currentTimer->flags = GBATimerFlagsSetPrescaleBits(currentTimer->flags, prescaleBits);
	currentTimer->flags = GBATimerFlagsTestFillCountUp(currentTimer->flags, timer > 0 && (control & 0x0004));
	currentTimer->flags = GBATimerFlagsTestFillDoIrq(currentTimer->flags, control & 0x0040);
	currentTimer->flags = GBATimerFlagsTestFillEnable(currentTimer->flags, control & 0x0080);

	bool reschedule = false;
	if (GBATimerFlagsIsEnable(oldFlags) != GBATimerFlagsIsEnable(currentTimer->flags)) {
		reschedule = true;
		if (GBATimerFlagsIsEnable(currentTimer->flags)) {
			gba->memory.io[REG_TMCNT_LO(timer) >> 1] = currentTimer->reload;
		}
	} else if (GBATimerFlagsIsCountUp(oldFlags) != GBATimerFlagsIsCountUp(currentTimer->flags)) {
		reschedule = true;
	} else if (GBATimerFlagsGetPrescaleBits(currentTimer->flags) != GBATimerFlagsGetPrescaleBits(oldFlags)) {
		reschedule = true;
	}

	if (reschedule) {
		mTimingDeschedule(&gba->timing, &currentTimer->event);
		if (GBATimerFlagsIsEnable(currentTimer->flags) && !GBATimerFlagsIsCountUp(currentTimer->flags)) {
			int32_t tickMask = (1 << prescaleBits) - 1;
			currentTimer->lastEvent = mTimingCurrentTime(&gba->timing) & ~tickMask;
			GBATimerUpdateRegister(gba, timer, 0);
		}
	}
}
