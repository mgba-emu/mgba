/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "timer.h"

#include "gb/gb.h"
#include "gb/io.h"
#include "gb/serialize.h"

void GBTimerReset(struct GBTimer* timer) {
	timer->nextDiv = GB_DMG_DIV_PERIOD; // TODO: GBC differences
	timer->nextEvent = GB_DMG_DIV_PERIOD;
	timer->eventDiff = 0;
	timer->timaPeriod = 1024 >> 4;
	timer->internalDiv = 0;
}

int32_t GBTimerProcessEvents(struct GBTimer* timer, int32_t cycles) {
	timer->eventDiff += cycles;
	timer->nextEvent -= cycles;
	if (timer->nextEvent <= 0) {
		timer->nextDiv -= timer->eventDiff;
		if (timer->irqPending) {
			timer->p->memory.io[REG_TIMA] = timer->p->memory.io[REG_TMA];
			timer->p->memory.io[REG_IF] |= (1 << GB_IRQ_TIMER);
			GBUpdateIRQs(timer->p);
			timer->irqPending = false;
			timer->nextEvent += timer->nextDiv;
		}
		if (timer->nextDiv <= 0) {
			if ((timer->internalDiv & 15) == 15) {
				++timer->p->memory.io[REG_DIV];
			}
			timer->nextDiv += GB_DMG_DIV_PERIOD;
			timer->nextEvent += GB_DMG_DIV_PERIOD;

			// Make sure to trigger when the correct bit is a falling edge
			if (timer->timaPeriod > 0 && (timer->internalDiv & (timer->timaPeriod - 1)) == timer->timaPeriod - 1) {
				++timer->p->memory.io[REG_TIMA];
				if (!timer->p->memory.io[REG_TIMA]) {
					timer->irqPending = true;
					timer->nextEvent += 4;
				}
			}
			++timer->internalDiv;
		}
		timer->eventDiff = 0;
	}
	return timer->nextEvent;
}

void GBTimerDivReset(struct GBTimer* timer) {
	timer->p->memory.io[REG_DIV] = 0;
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
	STORE_32LE(timer->nextEvent, 0, &state->timer.nextEvent);
	STORE_32LE(timer->eventDiff, 0, &state->timer.eventDiff);
	STORE_32LE(timer->nextDiv, 0, &state->timer.nextDiv);
	STORE_32LE(timer->internalDiv, 0, &state->timer.internalDiv);
	STORE_32LE(timer->timaPeriod, 0, &state->timer.timaPeriod);

	GBSerializedTimerFlags flags = 0;
	flags = GBSerializedTimerFlagsSetIrqPending(flags, state->timer.flags);
	state->timer.flags = flags;
}

void GBTimerDeserialize(struct GBTimer* timer, const struct GBSerializedState* state) {
	LOAD_32LE(timer->nextEvent, 0, &state->timer.nextEvent);
	LOAD_32LE(timer->eventDiff, 0, &state->timer.eventDiff);
	LOAD_32LE(timer->nextDiv, 0, &state->timer.nextDiv);
	LOAD_32LE(timer->internalDiv, 0, &state->timer.internalDiv);
	LOAD_32LE(timer->timaPeriod, 0, &state->timer.timaPeriod);

	GBSerializedTimerFlags flags = state->timer.flags ;
	timer->irqPending = GBSerializedTimerFlagsIsIrqPending(flags);
}
