/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/sio.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/serialize.h>

mLOG_DEFINE_CATEGORY(GB_SIO, "GB Serial I/O", "gb.sio");

const int GBSIOCyclesPerTransfer[2] = {
	512,
	16
};

void _GBSIOProcessEvents(struct mTiming* timing, void* context, uint32_t cyclesLate);

void GBSIOInit(struct GBSIO* sio) {
	sio->pendingSB = 0xFF;
	sio->event.context = sio;
	sio->event.name = "GB SIO";
	sio->event.callback = _GBSIOProcessEvents;
	sio->event.priority = 0x30;

	sio->driver = NULL;
}

void GBSIOReset(struct GBSIO* sio) {
	sio->nextEvent = INT_MAX;
	sio->remainingBits = 0;
	GBSIOSetDriver(sio, sio->driver);
}

void GBSIODeinit(struct GBSIO* sio) {
	UNUSED(sio);
	// Nothing to do yet
}

void GBSIOSetDriver(struct GBSIO* sio, struct GBSIODriver* driver) {
	if (sio->driver) {
		if (sio->driver->deinit) {
			sio->driver->deinit(sio->driver);
		}
	}
	if (driver) {
		driver->p = sio;

		if (driver->init) {
			if (!driver->init(driver)) {
				driver->deinit(driver);
				mLOG(GB_SIO, ERROR, "Could not initialize SIO driver");
				return;
			}
		}
	}
	sio->driver = driver;
}

void _GBSIOProcessEvents(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(cyclesLate);
	struct GBSIO* sio = context;
	bool doIRQ = false;
	if (sio->remainingBits) {
		doIRQ = true;
		--sio->remainingBits;
		sio->p->memory.io[GB_REG_SB] &= ~(128 >> sio->remainingBits);
		sio->p->memory.io[GB_REG_SB] |= sio->pendingSB & (128 >> sio->remainingBits);
	}
	if (!sio->remainingBits) {
		sio->p->memory.io[GB_REG_SC] = GBRegisterSCClearEnable(sio->p->memory.io[GB_REG_SC]);
		if (doIRQ) {
			sio->p->memory.io[GB_REG_IF] |= (1 << GB_IRQ_SIO);
			GBUpdateIRQs(sio->p);
			sio->pendingSB = 0xFF;
		}
	} else {
		mTimingSchedule(timing, &sio->event, sio->period * (2 - sio->p->doubleSpeed));
	}
}

void GBSIOWriteSB(struct GBSIO* sio, uint8_t sb) {
	if (!sio->driver) {
		return;
	}
	sio->driver->writeSB(sio->driver, sb);
}

void GBSIOWriteSC(struct GBSIO* sio, uint8_t sc) {
	sio->period = GBSIOCyclesPerTransfer[GBRegisterSCGetClockSpeed(sc)]; // TODO Shift Clock
	if (GBRegisterSCIsEnable(sc)) {
		mTimingDeschedule(&sio->p->timing, &sio->event);
		if (GBRegisterSCIsShiftClock(sc)) {
			mTimingSchedule(&sio->p->timing, &sio->event, sio->period * (2 - sio->p->doubleSpeed));
			sio->remainingBits = 8;
		}
	}
	if (sio->driver) {
		sio->driver->writeSC(sio->driver, sc);
	}
}

