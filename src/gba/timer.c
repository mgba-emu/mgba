/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/timer.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

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

void GBATimerUpdateCountUp(struct mTiming* timing, struct GBATimer* nextTimer, uint16_t* io, uint32_t cyclesLate) {
	if (GBATimerFlagsIsCountUp(nextTimer->flags)) { // TODO: Does this increment while disabled?
		++*io;
		if (!*io && GBATimerFlagsIsEnable(nextTimer->flags)) {
			mTimingSchedule(timing, &nextTimer->event, -cyclesLate);
		}
	}
}

void GBATimerUpdate(struct mTiming* timing, struct GBATimer* timer, uint16_t* io, uint32_t cyclesLate) {
	*io = timer->reload;
	timer->oldReload = timer->reload;
	timer->lastEvent = timing->masterCycles - cyclesLate;

	if (!GBATimerFlagsIsCountUp(timer->flags)) {
		uint32_t nextEvent = timer->overflowInterval - cyclesLate;
		mTimingSchedule(timing, &timer->event, nextEvent);
	}
}

static void GBATimerUpdate0(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	struct GBATimer* timer = &gba->timers[0];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		GBARaiseIRQ(gba, IRQ_TIMER0);
	}
	GBATimerUpdateAudio(gba, 0, cyclesLate);
	GBATimerUpdate(timing, &gba->timers[0], &gba->memory.io[REG_TM0CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &gba->timers[1], &gba->memory.io[REG_TM1CNT_LO >> 1], cyclesLate);
}

static void GBATimerUpdate1(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	struct GBATimer* timer = &gba->timers[1];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		GBARaiseIRQ(gba, IRQ_TIMER1);
	}
	GBATimerUpdateAudio(gba, 1, cyclesLate);
	GBATimerUpdate(timing, &gba->timers[1], &gba->memory.io[REG_TM1CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &gba->timers[2], &gba->memory.io[REG_TM2CNT_LO >> 1], cyclesLate);
}

static void GBATimerUpdate2(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	struct GBATimer* timer = &gba->timers[2];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		GBARaiseIRQ(gba, IRQ_TIMER2);
	}
	GBATimerUpdate(timing, &gba->timers[2], &gba->memory.io[REG_TM2CNT_LO >> 1], cyclesLate);
	GBATimerUpdateCountUp(timing, &gba->timers[3], &gba->memory.io[REG_TM3CNT_LO >> 1], cyclesLate);
}

static void GBATimerUpdate3(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBA* gba = context;
	struct GBATimer* timer = &gba->timers[3];
	if (GBATimerFlagsIsDoIrq(timer->flags)) {
		GBARaiseIRQ(gba, IRQ_TIMER3);
	}
	GBATimerUpdate(timing, &gba->timers[3], &gba->memory.io[REG_TM3CNT_LO >> 1], cyclesLate);
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
		int32_t prefetchSkew = 0;
		if (gba->memory.lastPrefetchedPc >= (uint32_t) gba->cpu->gprs[ARM_PC]) {
			prefetchSkew = (gba->memory.lastPrefetchedPc - gba->cpu->gprs[ARM_PC]) * (gba->cpu->memory.activeSeqCycles16 + 1) / WORD_SIZE_THUMB;
		}
		GBATimerUpdateRegisterInternal(currentTimer, &gba->timing, gba->cpu, &gba->memory.io[(REG_TM0CNT_LO + (timer << 2)) >> 1], prefetchSkew);
	}
}

void GBATimerUpdateRegisterInternal(struct GBATimer* timer, struct mTiming* timing, struct ARMCore* cpu, uint16_t* io, int32_t skew) {
	// Reading this takes two cycles (1N+1I), so let's remove them preemptively
	int32_t diff = cpu->cycles - (timer->lastEvent - timing->masterCycles);
	*io = timer->oldReload + ((diff - 2 + skew) >> GBATimerFlagsGetPrescaleBits(timer->flags));
}

void GBATimerWriteTMCNT_LO(struct GBATimer* timer, uint16_t reload) {
	timer->reload = reload;
	timer->overflowInterval = (0x10000 - timer->reload) << GBATimerFlagsGetPrescaleBits(timer->flags);
}

void GBATimerWriteTMCNT_HI(struct GBATimer* timer, struct mTiming* timing, struct ARMCore* cpu, uint16_t* io, uint16_t control) {
	unsigned oldPrescale = GBATimerFlagsGetPrescaleBits(timer->flags);
	switch (control & 0x0003) {
	case 0x0000:
		timer->flags = GBATimerFlagsSetPrescaleBits(timer->flags, 0);
		break;
	case 0x0001:
		timer->flags = GBATimerFlagsSetPrescaleBits(timer->flags, 6);
		break;
	case 0x0002:
		timer->flags = GBATimerFlagsSetPrescaleBits(timer->flags, 8);
		break;
	case 0x0003:
		timer->flags = GBATimerFlagsSetPrescaleBits(timer->flags, 10);
		break;
	}
	timer->flags = GBATimerFlagsTestFillCountUp(timer->flags, timer > 0 && (control & 0x0004));
	timer->flags = GBATimerFlagsTestFillDoIrq(timer->flags, control & 0x0040);
	timer->overflowInterval = (0x10000 - timer->reload) << GBATimerFlagsGetPrescaleBits(timer->flags);
	bool wasEnabled = GBATimerFlagsIsEnable(timer->flags);
	timer->flags = GBATimerFlagsTestFillEnable(timer->flags, control & 0x0080);
	if (!wasEnabled && GBATimerFlagsIsEnable(timer->flags)) {
		mTimingDeschedule(timing, &timer->event);
		if (!GBATimerFlagsIsCountUp(timer->flags)) {
			mTimingSchedule(timing, &timer->event, timer->overflowInterval);
		}
		*io = timer->reload;
		timer->oldReload = timer->reload;
		timer->lastEvent = timing->masterCycles + cpu->cycles;
	} else if (wasEnabled && !GBATimerFlagsIsEnable(timer->flags)) {
		mTimingDeschedule(timing, &timer->event);
		if (!GBATimerFlagsIsCountUp(timer->flags)) {
			*io = timer->oldReload + ((cpu->cycles - timer->lastEvent) >> oldPrescale);
		}
	} else if (GBATimerFlagsIsEnable(timer->flags) && GBATimerFlagsGetPrescaleBits(timer->flags) != oldPrescale && !GBATimerFlagsIsCountUp(timer->flags)) {
		mTimingDeschedule(timing, &timer->event);
		mTimingSchedule(timing, &timer->event, timer->overflowInterval - timer->lastEvent);
	}
}
