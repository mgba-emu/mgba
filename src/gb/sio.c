/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sio.h"

#include "gb/gb.h"
#include "gb/io.h"
#include "gb/serialize.h"

void _GBSIOProcessEvents(struct mTiming* timing, void* context, uint32_t cyclesLate);

void GBSIOInit(struct GBSIO* sio) {
	sio->pendingSB = 0xFF;
	sio->event.context = sio;
	sio->event.name = "GB SIO";
	sio->event.callback = _GBSIOProcessEvents;
	sio->event.priority = 0x30;
}

void GBSIOReset(struct GBSIO* sio) {
	sio->nextEvent = INT_MAX;
	sio->remainingBits = 0;
}

void GBSIODeinit(struct GBSIO* sio) {
	UNUSED(sio);
	// Nothing to do yet
}

void _GBSIOProcessEvents(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(cyclesLate);
	struct GBSIO* sio = context;
	--sio->remainingBits;
	sio->p->memory.io[REG_SB] &= ~(8 >> sio->remainingBits);
	sio->p->memory.io[REG_SB] |= sio->pendingSB & ~(8 >> sio->remainingBits);
	if (!sio->remainingBits) {
		sio->p->memory.io[REG_IF] |= (1 << GB_IRQ_SIO);
		sio->p->memory.io[REG_SC] = GBRegisterSCClearEnable(sio->p->memory.io[REG_SC]);
		GBUpdateIRQs(sio->p);
	} else {
		mTimingSchedule(timing, &sio->event, sio->period);
	}
}

void GBSIOWriteSC(struct GBSIO* sio, uint8_t sc) {
	sio->period = 0x1000; // TODO Shift Clock
	if (GBRegisterSCIsEnable(sc)) {
		if (GBRegisterSCIsShiftClock(sc)) {
			mTimingSchedule(&sio->p->timing, &sio->event, sio->period);
		}
		sio->remainingBits = 8;
	}
}

