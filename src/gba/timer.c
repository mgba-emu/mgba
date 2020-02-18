/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/timer.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#define REG_TMCNT_LO(X) (REG_TM0CNT_LO + ((X) << 2))

static void GBATimerIrq(struct GBA* gba, int timerId, uint32_t cyclesLate) {
	struct GBATimer* timer = &gba->timers[timerId];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		GBARaiseIRQ(gba, IRQ_TIMER0 + timerId, cyclesLate);
	}
}

void GBATimerUpdate(struct mTiming* timing, struct GBATimer* timer, uint16_t* io, uint32_t cyclesLate) {
	if (GBATimerFlagsIsCountUp(timer->flags)) {
		*io = timer->reload;
	} else {
		GBATimerUpdateRegisterInternal(timer, timing, io, cyclesLate);
	}
}

static void GBATimerUpdateAudio(struct GBA* gba, int timerId, uint32_t cyclesLate) {
	if (!gba->audio.enable) {
		return;
	}
	if ((gba->audio.chALeft || gba->audio.chARight) && gba->audio.chATimer == timerId) {
		GBAAudioSampleFIFO(&gba->audio, 0, cyclesLate);
	}

	if ((gba->audio.chBLeft || gba->audio.chBRight) && gba->audio.chBTimer == timerId) {
		GBAAudioSampleFIFO(&gba->audio, 1, cyclesLate);
	}
}

bool GBATimerUpdateCountUp(struct mTiming* timing, struct GBATimer* nextTimer, uint16_t* io, uint32_t cyclesLate) {
	if (GBATimerFlagsIsCountUp(nextTimer->flags)) { // TODO: Does this increment while disabled?
		++*io;
		if (!*io && GBATimerFlagsIsEnable(nextTimer->flags)) {
			GBATimerUpdate(timing, nextTimer, io, cyclesLate);
			return true;
		}
	}
	return false;
}

static void GBATimerUpdate0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	GBATimerUpdateAudio(gba, 0, cyclesLate);
	GBATimerUpdate(timing, &gba->timers[0], &gba->memory.io[REG_TM0CNT_LO >> 1], cyclesLate);
	GBATimerIrq(gba, 0, cyclesLate);
	if (GBATimerUpdateCountUp(timing, &gba->timers[1], &gba->memory.io[REG_TM1CNT_LO >> 1], cyclesLate)) {
		GBATimerIrq(gba, 1, cyclesLate);
	}
}

static void GBATimerUpdate1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	GBATimerUpdateAudio(gba, 1, cyclesLate);
	GBATimerUpdate(timing, &gba->timers[1], &gba->memory.io[REG_TM1CNT_LO >> 1], cyclesLate);
	GBATimerIrq(gba, 1, cyclesLate);
	if (GBATimerUpdateCountUp(timing, &gba->timers[2], &gba->memory.io[REG_TM2CNT_LO >> 1], cyclesLate)) {
		GBATimerIrq(gba, 2, cyclesLate);
	}
}

static void GBATimerUpdate2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	GBATimerUpdate(timing, &gba->timers[2], &gba->memory.io[REG_TM2CNT_LO >> 1], cyclesLate);
	GBATimerIrq(gba, 2, cyclesLate);
	if (GBATimerUpdateCountUp(timing, &gba->timers[3], &gba->memory.io[REG_TM3CNT_LO >> 1], cyclesLate)) {
		GBATimerIrq(gba, 3, cyclesLate);
	}
}

static void GBATimerUpdate3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	GBATimerUpdate(timing, &gba->timers[3], &gba->memory.io[REG_TM3CNT_LO >> 1], cyclesLate);
	GBATimerIrq(gba, 3, cyclesLate);
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
	if (GBATimerFlagsIsEnable(currentTimer->flags) && !GBATimerFlagsIsCountUp(currentTimer->flags)) {
		int32_t prefetchSkew = cyclesLate;
		if (gba->memory.lastPrefetchedPc > (uint32_t) gba->cpu->gprs[ARM_PC]) {
			prefetchSkew += ((gba->memory.lastPrefetchedPc - gba->cpu->gprs[ARM_PC]) * gba->cpu->memory.activeSeqCycles16) / WORD_SIZE_THUMB;
		}
		GBATimerUpdateRegisterInternal(currentTimer, &gba->timing, &gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1], prefetchSkew);
	}
}

void GBATimerUpdateRegisterInternal(struct GBATimer* timer, struct mTiming* timing, uint16_t* io, int32_t skew) {
	if (!GBATimerFlagsIsEnable(timer->flags) || GBATimerFlagsIsCountUp(timer->flags)) {
		return;
	}

	// Align timer
	int prescaleBits = GBATimerFlagsGetPrescaleBits(timer->flags);
	int32_t currentTime = mTimingCurrentTime(timing) - skew;
	int32_t tickMask = (1 << prescaleBits) - 1;
	currentTime &= ~tickMask;

	// Update register
	int32_t tickIncrement = currentTime - timer->lastEvent;
	timer->lastEvent = currentTime;
	tickIncrement >>= prescaleBits;
	tickIncrement += *io;
	*io = tickIncrement;
	while (tickIncrement >= 0x10000) {
		tickIncrement -= 0x10000 - timer->reload;
	}
	*io = tickIncrement;

	// Schedule next update
	tickIncrement = (0x10000 - tickIncrement) << prescaleBits;
	currentTime += tickIncrement;
	currentTime &= ~tickMask;
	currentTime -= mTimingCurrentTime(timing);
	mTimingDeschedule(timing, &timer->event);
	mTimingSchedule(timing, &timer->event, currentTime);
}

void GBATimerWriteTMCNT_LO(struct GBATimer* timer, uint16_t reload) {
	timer->reload = reload;
}

void GBATimerWriteTMCNT_HI(struct GBATimer* timer, struct mTiming* timing, uint16_t* io, uint16_t control) {
	GBATimerUpdateRegisterInternal(timer, timing, io, 0);

	unsigned oldPrescale = GBATimerFlagsGetPrescaleBits(timer->flags);
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
	prescaleBits += timer->forcedPrescale;
	timer->flags = GBATimerFlagsSetPrescaleBits(timer->flags, prescaleBits);
	timer->flags = GBATimerFlagsTestFillCountUp(timer->flags, timer > 0 && (control & 0x0004));
	timer->flags = GBATimerFlagsTestFillDoIrq(timer->flags, control & 0x0040);
	bool wasEnabled = GBATimerFlagsIsEnable(timer->flags);
	timer->flags = GBATimerFlagsTestFillEnable(timer->flags, control & 0x0080);
	if (!wasEnabled && GBATimerFlagsIsEnable(timer->flags)) {
		mTimingDeschedule(timing, &timer->event);
		*io = timer->reload;
		int32_t tickMask = (1 << prescaleBits) - 1;
		timer->lastEvent = mTimingCurrentTime(timing) & ~tickMask;
		GBATimerUpdateRegisterInternal(timer, timing, io, 0);
	} else if (wasEnabled && !GBATimerFlagsIsEnable(timer->flags)) {
		mTimingDeschedule(timing, &timer->event);
	} else if (GBATimerFlagsIsEnable(timer->flags) && GBATimerFlagsGetPrescaleBits(timer->flags) != oldPrescale && !GBATimerFlagsIsCountUp(timer->flags)) {
		mTimingDeschedule(timing, &timer->event);
		int32_t tickMask = (1 << prescaleBits) - 1;
		timer->lastEvent = mTimingCurrentTime(timing) & ~tickMask;
		GBATimerUpdateRegisterInternal(timer, timing, io, 0);
	}
}
