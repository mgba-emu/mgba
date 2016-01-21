/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "timer.h"

#include "gb/gb.h"
#include "gb/io.h"

void GBTimerReset(struct GBTimer* timer) {
	timer->nextDiv = GB_DMG_DIV_PERIOD; // TODO: GBC differences
	timer->nextTima = INT_MAX;
	timer->nextEvent = INT_MAX;
}

int32_t GBTimerProcessEvents(struct GBTimer* timer, int32_t cycles) {
	if (timer->nextEvent == INT_MAX) {
		return INT_MAX;
	}
	timer->eventDiff += cycles;
	timer->nextEvent -= cycles;
	if (timer->nextEvent < 0) {
		timer->nextDiv -= timer->eventDiff;
		if (timer->nextDiv <= 0) {
			++timer->p->memory.io[REG_DIV];
			timer->nextDiv = GB_DMG_DIV_PERIOD;
		}
		timer->nextEvent = timer->nextDiv;

		if (timer->nextTima != INT_MAX) {
			timer->nextTima -= timer->eventDiff;
			if (timer->nextTima <= 0) {
				++timer->p->memory.io[REG_TIMA];
				if (!timer->p->memory.io[REG_TIMA]) {
					timer->p->memory.io[REG_TIMA] = timer->p->memory.io[REG_TMA];
					timer->p->memory.io[REG_IF] |= (1 << GB_IRQ_TIMER);
					GBUpdateIRQs(timer->p);
				}
				timer->nextTima = timer->timaPeriod;
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
	// TODO: Do we need to reset the event?
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
		timer->nextTima = timer->eventDiff + timer->timaPeriod;
		if (timer->nextTima < timer->nextEvent) {
			timer->nextEvent = timer->nextTima;
			if (timer->nextEvent < timer->p->cpu->nextEvent) {
				timer->p->cpu->nextEvent = timer->nextEvent;
			}
		}
	} else {
		timer->nextTima = INT_MAX;
	}
	return tac;
}
