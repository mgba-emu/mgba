/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/timer.h>

#include <mgba/internal/arm/arm.h>
#include <mgba/internal/ds/ds.h>

void DSTimerUpdateRegister(struct DSTimer* timer, struct ARMCore* cpu, uint16_t* io) {
	if (DSTimerFlagsIsEnable(timer->flags) && !DSTimerFlagsIsCountUp(timer->flags)) {
		// Reading this takes two cycles (1N+1I), so let's remove them preemptively
		*io = timer->oldReload + ((cpu->cycles - timer->lastEvent - 2) >> DSTimerFlagsGetPrescaleBits(timer->flags));
	}
}

void DSTimerWriteTMCNT_LO(struct DSTimer* timer, uint16_t reload) {
	timer->reload = reload;
	timer->overflowInterval = (0x10000 - timer->reload) << DSTimerFlagsGetPrescaleBits(timer->flags);
}

void DSTimerWriteTMCNT_HI(struct DSTimer* timer, struct ARMCore* cpu, uint16_t* io, uint16_t control) {
	DSTimerUpdateRegister(timer, cpu, io);

	unsigned oldPrescale = DSTimerFlagsGetPrescaleBits(timer->flags);
	switch (control & 0x0003) {
	case 0x0000:
		timer->flags = DSTimerFlagsSetPrescaleBits(timer->flags, 0);
		break;
	case 0x0001:
		timer->flags = DSTimerFlagsSetPrescaleBits(timer->flags, 6);
		break;
	case 0x0002:
		timer->flags = DSTimerFlagsSetPrescaleBits(timer->flags, 8);
		break;
	case 0x0003:
		timer->flags = DSTimerFlagsSetPrescaleBits(timer->flags, 10);
		break;
	}
	timer->flags = DSTimerFlagsTestFillCountUp(timer->flags, control & 0x0004);
	timer->flags = DSTimerFlagsTestFillDoIrq(timer->flags, control & 0x0040);
	timer->overflowInterval = (0x10000 - timer->reload) << DSTimerFlagsGetPrescaleBits(timer->flags);
	bool wasEnabled = DSTimerFlagsIsEnable(timer->flags);
	timer->flags = DSTimerFlagsTestFillEnable(timer->flags, control & 0x0080);
	if (!wasEnabled && DSTimerFlagsIsEnable(timer->flags)) {
		if (!DSTimerFlagsIsCountUp(timer->flags)) {
			timer->nextEvent = cpu->cycles + timer->overflowInterval;
		} else {
			timer->nextEvent = INT_MAX;
		}
		*io = timer->reload;
		timer->oldReload = timer->reload;
		timer->lastEvent = cpu->cycles;
	} else if (wasEnabled && !DSTimerFlagsIsEnable(timer->flags)) {
		if (!DSTimerFlagsIsCountUp(timer->flags)) {
			*io = timer->oldReload + ((cpu->cycles - timer->lastEvent) >> oldPrescale);
		}
	} else if (DSTimerFlagsGetPrescaleBits(timer->flags) != oldPrescale && !DSTimerFlagsIsCountUp(timer->flags)) {
		// FIXME: this might be before present
		timer->nextEvent = timer->lastEvent + timer->overflowInterval;
	}

	if (timer->nextEvent < cpu->nextEvent) {
		cpu->nextEvent = timer->nextEvent;
	}
}

int32_t DSTimersProcessEvents(struct DS* ds, int32_t cycles) {
	int32_t nextEvent = INT_MAX;
	if (!ds->timersEnabled7) {
		return nextEvent;
	}

	struct DSTimer* timer;
	struct DSTimer* nextTimer;

	int t;
	for (t = 0; t < 4; ++t) {
		timer = &ds->timers7[t];
		if (DSTimerFlagsIsEnable(timer->flags)) {
			timer->nextEvent -= cycles;
			timer->lastEvent -= cycles;
			while (timer->nextEvent <= 0) {
				timer->lastEvent = timer->nextEvent;
				timer->nextEvent += timer->overflowInterval;
				ds->memory.io7[(DS7_REG_TM0CNT_LO + (t << 2)) >> 1] = timer->reload;
				timer->oldReload = timer->reload;

				if (DSTimerFlagsIsDoIrq(timer->flags)) {
					DSRaiseIRQ(ds->arm7, ds->memory.io7, DS_IRQ_TIMER0);
				}

				if (t == 3) {
					break;
				}

				nextTimer = &ds->timers7[t + 1];
				if (DSTimerFlagsIsCountUp(nextTimer->flags)) {
					++ds->memory.io7[(DS7_REG_TM1CNT_LO + (t << 2)) >> 1];
					if (!ds->memory.io7[(DS7_REG_TM1CNT_LO + (t << 2)) >> 1]) {
						nextTimer->nextEvent = 0;
					}
				}
			}
			nextEvent = timer->nextEvent;
		}
	}
	return nextEvent;
}
