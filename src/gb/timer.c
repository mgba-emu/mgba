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
	timer->nextTima = INT_MAX;
	timer->nextEvent = GB_DMG_DIV_PERIOD;
	timer->eventDiff = 0;
	timer->timaPeriod = 1024;
}

int32_t GBTimerProcessEvents(struct GBTimer* timer, int32_t cycles) {
	timer->eventDiff += cycles;
	timer->nextEvent -= cycles;
	if (timer->nextEvent <= 0) {
		timer->nextDiv -= timer->eventDiff;
		if (timer->nextDiv <= 0) {
			++timer->p->memory.io[REG_DIV];
			timer->nextDiv = GB_DMG_DIV_PERIOD;
		}
		timer->nextEvent = timer->nextDiv;

		if (timer->nextTima != INT_MAX) {
			timer->nextTima -= timer->eventDiff;
			if (timer->nextTima <= 0) {
				if (!timer->p->memory.io[REG_TIMA]) {
					timer->p->memory.io[REG_TIMA] = timer->p->memory.io[REG_TMA];
					timer->p->memory.io[REG_IF] |= (1 << GB_IRQ_TIMER);
					GBUpdateIRQs(timer->p);
					timer->nextTima = timer->timaPeriod - 4;
				} else {
					++timer->p->memory.io[REG_TIMA];
					if (!timer->p->memory.io[REG_TIMA]) {
						timer->nextTima = 4;
					} else {
						timer->nextTima = timer->timaPeriod;
					}
				}
			}
			if (timer->nextTima < timer->nextEvent) {
				timer->nextEvent = timer->nextTima;
			}
		}

		timer->eventDiff = 0;
	}
	return timer->nextEvent;
}

void GBTimerDivReset(struct GBTimer* timer) {
	timer->p->memory.io[REG_DIV] = 0;
	timer->nextDiv = timer->eventDiff + timer->p->cpu->cycles + GB_DMG_DIV_PERIOD;
	if (timer->nextDiv - timer->eventDiff < timer->nextEvent) {
		timer->nextEvent = timer->nextDiv - timer->eventDiff;
		if (timer->nextEvent < timer->p->cpu->nextEvent) {
			timer->p->cpu->nextEvent = timer->nextEvent;
		}
	}
}

uint8_t GBTimerUpdateTAC(struct GBTimer* timer, GBRegisterTAC tac) {
	if (GBRegisterTACIsRun(tac)) {
		switch (GBRegisterTACGetClock(tac)) {
		case 0:
			timer->timaPeriod = 1024;
			break;
		case 1:
			timer->timaPeriod = 16;
			break;
		case 2:
			timer->timaPeriod = 64;
			break;
		case 3:
			timer->timaPeriod = 256;
			break;
		}
		GBTimerUpdateTIMA(timer);
	} else {
		timer->nextTima = INT_MAX;
	}
	return tac;
}

void GBTimerUpdateTIMA(struct GBTimer* timer) {
	timer->nextTima = timer->eventDiff + timer->p->cpu->cycles + timer->timaPeriod;
	if (timer->nextTima - timer->eventDiff < timer->nextEvent) {
		timer->nextEvent = timer->nextTima - timer->eventDiff;
		if (timer->nextEvent < timer->p->cpu->nextEvent) {
			timer->p->cpu->nextEvent = timer->nextEvent;
		}
	}
}

void GBTimerSerialize(const struct GBTimer* timer, struct GBSerializedState* state) {
	STORE_32LE(timer->nextEvent, 0, &state->timer.nextEvent);
	STORE_32LE(timer->eventDiff, 0, &state->timer.eventDiff);
	STORE_32LE(timer->nextDiv, 0, &state->timer.nextDiv);
	STORE_32LE(timer->nextTima, 0, &state->timer.nextTima);
	STORE_32LE(timer->timaPeriod, 0, &state->timer.timaPeriod);
}

void GBTimerDeserialize(struct GBTimer* timer, const struct GBSerializedState* state) {
	LOAD_32LE(timer->nextEvent, 0, &state->timer.nextEvent);
	LOAD_32LE(timer->eventDiff, 0, &state->timer.eventDiff);
	LOAD_32LE(timer->nextDiv, 0, &state->timer.nextDiv);
	LOAD_32LE(timer->nextTima, 0, &state->timer.nextTima);
	LOAD_32LE(timer->timaPeriod, 0, &state->timer.timaPeriod);
}
