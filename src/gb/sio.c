/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sio.h"

#include "gb/gb.h"
#include "gb/io.h"
#include "gb/serialize.h"

void GBSIOInit(struct GBSIO* sio) {
	sio->pendingSB = 0xFF;
}

void GBSIOReset(struct GBSIO* sio) {
	sio->nextEvent = INT_MAX;
	sio->remainingBits = 0;
}

void GBSIODeinit(struct GBSIO* sio) {
	UNUSED(sio);
	// Nothing to do yet
}

int32_t GBSIOProcessEvents(struct GBSIO* sio, int32_t cycles) {
	if (sio->nextEvent != INT_MAX) {
		sio->nextEvent -= cycles;
	}
	if (sio->nextEvent <= 0) {
		--sio->remainingBits;
		sio->p->memory.io[REG_SB] &= ~(8 >> sio->remainingBits);
		sio->p->memory.io[REG_SB] |= sio->pendingSB & ~(8 >> sio->remainingBits);
		if (!sio->remainingBits) {
			sio->p->memory.io[REG_IF] |= (1 << GB_IRQ_SIO);
			sio->p->memory.io[REG_SC] = GBRegisterSCClearEnable(sio->p->memory.io[REG_SC]);
			GBUpdateIRQs(sio->p);
			sio->nextEvent = INT_MAX;
		} else {
			sio->nextEvent += sio->period;
		}
	}
	return sio->nextEvent;
}

void GBSIOWriteSC(struct GBSIO* sio, uint8_t sc) {
	sio->period = 0x1000; // TODO Shift Clock
	if (GBRegisterSCIsEnable(sc)) {
		sio->nextEvent = sio->p->cpu->cycles + sio->period;
		sio->remainingBits = 8;
	}
}

