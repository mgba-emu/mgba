/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/timer.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#define TIMER_MASTER 1024

static void GBATimerUpdate(struct GBA* gba, int timerId, uint32_t cyclesLate) {
	struct GBATimer* timer = &gba->timers[timerId];
	gba->memory.io[(REG_TM0CNT_LO >> 1) + (timerId << 1)] = timer->reload;
	int32_t currentTime = mTimingCurrentTime(&gba->timing) - cyclesLate;
	int32_t tickMask = (1 << GBATimerFlagsGetPrescaleBits(timer->flags)) - 1;
	currentTime &= ~tickMask;
	timer->lastEvent = currentTime;
	GBATimerUpdateRegister(gba, timerId, 0);

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

	if (timerId < 4) {
		struct GBATimer* nextTimer = &gba->timers[timerId + 1];
		if (GBATimerFlagsIsCountUp(nextTimer->flags)) { // TODO: Does this increment while disabled?
			++gba->memory.io[(REG_TM1CNT_LO >> 1) + (timerId << 1)];
			if (!gba->memory.io[(REG_TM1CNT_LO >> 1) + (timerId << 1)] && GBATimerFlagsIsEnable(nextTimer->flags)) {
				GBATimerUpdate(gba, timerId + 1, cyclesLate);
			}
		}
	}
}

static void GBATimerMasterUpdate(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	mTimingSchedule(timing, &gba->timerMaster, TIMER_MASTER - cyclesLate);
	GBATimerUpdateRegister(gba, 0, cyclesLate);
	GBATimerUpdateRegister(gba, 1, cyclesLate);
	GBATimerUpdateRegister(gba, 2, cyclesLate);
	GBATimerUpdateRegister(gba, 3, cyclesLate);
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
	gba->timerMaster.name = "GBA Timer Master";
	gba->timerMaster.callback = GBATimerMasterUpdate;
	gba->timerMaster.context = gba;
	gba->timerMaster.priority = 0x20;
	mTimingSchedule(&gba->timing, &gba->timerMaster, TIMER_MASTER);
	memset(gba->timers, 0, sizeof(gba->timers));
	gba->timers[0].event.name = "GBA Timer 0";
	gba->timers[0].event.callback = GBATimerUpdate0;
	gba->timers[0].event.context = gba;
	gba->timers[0].event.priority = 0x21;
	gba->timers[1].event.name = "GBA Timer 1";
	gba->timers[1].event.callback = GBATimerUpdate1;
	gba->timers[1].event.context = gba;
	gba->timers[1].event.priority = 0x22;
	gba->timers[2].event.name = "GBA Timer 2";
	gba->timers[2].event.callback = GBATimerUpdate2;
	gba->timers[2].event.context = gba;
	gba->timers[2].event.priority = 0x23;
	gba->timers[3].event.name = "GBA Timer 3";
	gba->timers[3].event.callback = GBATimerUpdate3;
	gba->timers[3].event.context = gba;
	gba->timers[3].event.priority = 0x24;
}

void GBATimerUpdateRegister(struct GBA* gba, int timer, int32_t cyclesLate) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	if (!GBATimerFlagsIsEnable(currentTimer->flags) || GBATimerFlagsIsCountUp(currentTimer->flags)) {
		return;
	}

	if (gba->memory.lastPrefetchedPc > (uint32_t) gba->cpu->gprs[ARM_PC]) {
		cyclesLate -= ((gba->memory.lastPrefetchedPc - gba->cpu->gprs[ARM_PC]) * gba->cpu->memory.activeSeqCycles16) / WORD_SIZE_THUMB;
	}

	int prescaleBits = GBATimerFlagsGetPrescaleBits(currentTimer->flags);
	int32_t currentTime = mTimingCurrentTime(&gba->timing) - cyclesLate;
	int32_t tickMask = (1 << prescaleBits) - 1;
	currentTime &= ~tickMask;
	int32_t tickIncrement = currentTime - currentTimer->lastEvent;
	currentTimer->lastEvent = currentTime;
	tickIncrement >>= prescaleBits;
	tickIncrement += gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1];
	gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = tickIncrement;
	tickIncrement -= 0x10000;
	mTimingDeschedule(&gba->timing, &currentTimer->event);
	if (tickIncrement >= 0) {
		mTimingSchedule(&gba->timing, &currentTimer->event, 7 - (tickIncrement << prescaleBits));
	} else {
		int32_t nextIncrement = mTimingUntil(&gba->timing, &gba->timerMaster);
		tickIncrement = -tickIncrement;
		tickIncrement <<= prescaleBits;
		if (nextIncrement - tickIncrement > 0) {
			mTimingSchedule(&gba->timing, &currentTimer->event, 7 + tickIncrement);
		}
	}
}

void GBATimerWriteTMCNT_LO(struct GBA* gba, int timer, uint16_t reload) {
	gba->timers[timer].reload = reload;
}

void GBATimerWriteTMCNT_HI(struct GBA* gba, int timer, uint16_t control) {
	struct GBATimer* currentTimer = &gba->timers[timer];
	GBATimerUpdateRegister(gba, timer, 0);

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
	bool wasEnabled = GBATimerFlagsIsEnable(currentTimer->flags);
	currentTimer->flags = GBATimerFlagsTestFillEnable(currentTimer->flags, control & 0x0080);
	if (!wasEnabled && GBATimerFlagsIsEnable(currentTimer->flags)) {
		mTimingDeschedule(&gba->timing, &currentTimer->event);
		gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1] = currentTimer->reload;
		int32_t tickMask = (1 << prescaleBits) - 1;
		currentTimer->lastEvent = (mTimingCurrentTime(&gba->timing)) & ~tickMask;
		GBATimerUpdateRegister(gba, timer, 0);
	} else if (wasEnabled && !GBATimerFlagsIsEnable(currentTimer->flags)) {
		mTimingDeschedule(&gba->timing, &currentTimer->event);
	} else if (GBATimerFlagsIsEnable(currentTimer->flags) && GBATimerFlagsGetPrescaleBits(currentTimer->flags) != oldPrescale && !GBATimerFlagsIsCountUp(currentTimer->flags)) {
		mTimingDeschedule(&gba->timing, &currentTimer->event);
		int32_t tickMask = (1 << prescaleBits) - 1;
		currentTimer->lastEvent = (mTimingCurrentTime(&gba->timing)) & ~tickMask;
		GBATimerUpdateRegister(gba, timer, 0);
	}
}
