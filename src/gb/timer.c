/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/timer.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/serialize.h>

void _GBTimerIRQ(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct GBTimer* timer = context;
	timer->p->memory.io[REG_TIMA] = timer->p->memory.io[REG_TMA];
	timer->p->memory.io[REG_IF] |= (1 << GB_IRQ_TIMER);
	GBUpdateIRQs(timer->p);
}

void _GBTimerIncrement(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBTimer* timer = context;
	timer->nextDiv += cyclesLate;
	while (timer->nextDiv > 0) {
		timer->nextDiv -= GB_DMG_DIV_PERIOD;

		// Make sure to trigger when the correct bit is a falling edge
		if (timer->timaPeriod > 0 && (timer->internalDiv & (timer->timaPeriod - 1)) == timer->timaPeriod - 1) {
			++timer->p->memory.io[REG_TIMA];
			if (!timer->p->memory.io[REG_TIMA]) {
				mTimingSchedule(timing, &timer->irq, 4 - cyclesLate);
			}
		}
		++timer->internalDiv;
		timer->p->memory.io[REG_DIV] = timer->internalDiv >> 4;
	}
	// Batch div increments
	int divsToGo = 16 - (timer->internalDiv & 15);
	int timaToGo = INT_MAX;
	if (timer->timaPeriod) {
		timaToGo = timer->timaPeriod - (timer->internalDiv & (timer->timaPeriod - 1));
	}
	if (timaToGo < divsToGo) {
		divsToGo = timaToGo;
	}
	timer->nextDiv = GB_DMG_DIV_PERIOD * divsToGo;
	mTimingSchedule(timing, &timer->event, timer->nextDiv - cyclesLate);
}

void GBTimerReset(struct GBTimer* timer) {
	timer->event.context = timer;
	timer->event.name = "GB Timer";
	timer->event.callback = _GBTimerIncrement;
	timer->event.priority = 0x20;
	timer->irq.context = timer;
	timer->irq.name = "GB Timer IRQ";
	timer->irq.callback = _GBTimerIRQ;
	timer->event.priority = 0x21;

	timer->nextDiv = GB_DMG_DIV_PERIOD; // TODO: GBC differences
	timer->timaPeriod = 1024 >> 4;
	timer->internalDiv = 0;
}

void GBTimerDivReset(struct GBTimer* timer) {
	timer->p->memory.io[REG_DIV] = 0;
	timer->internalDiv = 0;
	timer->nextDiv = GB_DMG_DIV_PERIOD;
	mTimingDeschedule(&timer->p->timing, &timer->event);
	mTimingSchedule(&timer->p->timing, &timer->event, timer->nextDiv);
}

uint8_t GBTimerUpdateTAC(struct GBTimer* timer, GBRegisterTAC tac) {
	if (GBRegisterTACIsRun(tac)) {
		switch (GBRegisterTACGetClock(tac)) {
		case 0:
			timer->timaPeriod = 1024 >> 4;
			break;
		case 1:
			timer->timaPeriod = 16 >> 4;
			break;
		case 2:
			timer->timaPeriod = 64 >> 4;
			break;
		case 3:
			timer->timaPeriod = 256 >> 4;
			break;
		}
	} else {
		timer->timaPeriod = 0;
	}
	return tac;
}

void GBTimerSerialize(const struct GBTimer* timer, struct GBSerializedState* state) {
	STORE_32LE(timer->nextDiv, 0, &state->timer.nextDiv);
	STORE_32LE(timer->internalDiv, 0, &state->timer.internalDiv);
	STORE_32LE(timer->timaPeriod, 0, &state->timer.timaPeriod);
	STORE_32LE(timer->event.when - mTimingCurrentTime(&timer->p->timing), 0, &state->timer.nextEvent);
	STORE_32LE(timer->irq.when - mTimingCurrentTime(&timer->p->timing), 0, &state->timer.nextIRQ);
	GBSerializedTimerFlags flags = GBSerializedTimerFlagsSetIrqPending(0, mTimingIsScheduled(&timer->p->timing, &timer->irq));
	STORE_32LE(flags, 0, &state->timer.flags);
}

void GBTimerDeserialize(struct GBTimer* timer, const struct GBSerializedState* state) {
	LOAD_32LE(timer->nextDiv, 0, &state->timer.nextDiv);
	LOAD_32LE(timer->internalDiv, 0, &state->timer.internalDiv);
	LOAD_32LE(timer->timaPeriod, 0, &state->timer.timaPeriod);

	uint32_t when;
	LOAD_32LE(when, 0, &state->timer.nextEvent);
	mTimingSchedule(&timer->p->timing, &timer->event, when);

	GBSerializedTimerFlags flags;
	LOAD_32LE(flags, 0, &state->timer.flags);

	if (GBSerializedTimerFlagsIsIrqPending(flags)) {
		LOAD_32LE(when, 0, &state->timer.nextIRQ);
		mTimingSchedule(&timer->p->timing, &timer->irq, when);
	}
}
