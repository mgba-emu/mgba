/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio/dolphin.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>

#define BITS_PER_SECOND 115200 // This is wrong, but we need to maintain compat for the time being
#define CYCLES_PER_BIT (GBA_ARM7TDMI_FREQUENCY / BITS_PER_SECOND)
#define CLOCK_GRAIN (CYCLES_PER_BIT * 8)
#define CLOCK_WAIT 500

const uint16_t DOLPHIN_CLOCK_PORT = 49420;
const uint16_t DOLPHIN_DATA_PORT = 54970;

enum {
	WAIT_FOR_FIRST_CLOCK = 0,
	WAIT_FOR_CLOCK,
	WAIT_FOR_COMMAND,
};

static bool GBASIODolphinInit(struct GBASIODriver* driver);
static bool GBASIODolphinLoad(struct GBASIODriver* driver);
static bool GBASIODolphinUnload(struct GBASIODriver* driver);
static void GBASIODolphinProcessEvents(struct mTiming* timing, void* context, uint32_t cyclesLate);

static int32_t _processCommand(struct GBASIODolphin* dol, uint32_t cyclesLate);
static void _flush(struct GBASIODolphin* dol);

void GBASIODolphinCreate(struct GBASIODolphin* dol) {
	GBASIOJOYCreate(&dol->d);
	dol->d.init = GBASIODolphinInit;
	dol->d.load = GBASIODolphinLoad;
	dol->d.unload = GBASIODolphinUnload;
	dol->event.context = dol;
	dol->event.name = "GB SIO Lockstep";
	dol->event.callback = GBASIODolphinProcessEvents;
	dol->event.priority = 0x80;

	dol->data = INVALID_SOCKET;
	dol->clock = INVALID_SOCKET;
	dol->active = false;
}

void GBASIODolphinDestroy(struct GBASIODolphin* dol) {
	if (!SOCKET_FAILED(dol->data)) {
		SocketClose(dol->data);
		dol->data = INVALID_SOCKET;
	}

	if (!SOCKET_FAILED(dol->clock)) {
		SocketClose(dol->clock);
		dol->clock = INVALID_SOCKET;
	}
}

bool GBASIODolphinConnect(struct GBASIODolphin* dol, const struct Address* address, short dataPort, short clockPort) {
	if (!SOCKET_FAILED(dol->data)) {
		SocketClose(dol->data);
		dol->data = INVALID_SOCKET;
	}
	if (!dataPort) {
		dataPort = DOLPHIN_DATA_PORT;
	}

	if (!SOCKET_FAILED(dol->clock)) {
		SocketClose(dol->clock);
		dol->clock = INVALID_SOCKET;
	}
	if (!clockPort) {
		clockPort = DOLPHIN_CLOCK_PORT;
	}

	dol->data = SocketConnectTCP(dataPort, address);
	if (SOCKET_FAILED(dol->data)) {
		return false;
	}

	dol->clock = SocketConnectTCP(clockPort, address);
	if (SOCKET_FAILED(dol->clock)) {
		SocketClose(dol->data);
		dol->data = INVALID_SOCKET;
		return false;
	}

	SocketSetBlocking(dol->data, false);
	SocketSetBlocking(dol->clock, false);
	SocketSetTCPPush(dol->data, true);
	return true;
}

static bool GBASIODolphinInit(struct GBASIODriver* driver) {
	struct GBASIODolphin* dol = (struct GBASIODolphin*) driver;
	dol->active = false;
	dol->clockSlice = 0;
	dol->state = WAIT_FOR_FIRST_CLOCK;
	_flush(dol);
	return true;
}

static bool GBASIODolphinLoad(struct GBASIODriver* driver) {
	struct GBASIODolphin* dol = (struct GBASIODolphin*) driver;
	dol->active = true;
	_flush(dol);
	mTimingDeschedule(&dol->d.p->p->timing, &dol->event);
	mTimingSchedule(&dol->d.p->p->timing, &dol->event, 0);
	return true;
}

static bool GBASIODolphinUnload(struct GBASIODriver* driver) {
	struct GBASIODolphin* dol = (struct GBASIODolphin*) driver;
	dol->active = false;
	return true;
}

void GBASIODolphinProcessEvents(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct GBASIODolphin* dol = context;
	if (SOCKET_FAILED(dol->data)) {
		return;
	}

	dol->clockSlice -= cyclesLate;

	int32_t clockSlice;

	int32_t nextEvent = CLOCK_GRAIN;
	switch (dol->state) {
	case WAIT_FOR_FIRST_CLOCK:
		dol->clockSlice = 0;
		// Fall through
	case WAIT_FOR_CLOCK:
		if (dol->clockSlice < 0) {
			Socket r = dol->clock;
			SocketPoll(1, &r, 0, 0, CLOCK_WAIT);
		}
		if (SocketRecv(dol->clock, &clockSlice, 4) == 4) {
			clockSlice = ntohl(clockSlice);
			dol->clockSlice += clockSlice;
			dol->state = WAIT_FOR_COMMAND;
			nextEvent = 0;
		}
		// Fall through
	case WAIT_FOR_COMMAND:
		if (dol->clockSlice < -VIDEO_TOTAL_LENGTH * 4) {
			Socket r = dol->data;
			SocketPoll(1, &r, 0, 0, CLOCK_WAIT);
		}
		if (_processCommand(dol, cyclesLate) >= 0) {
			dol->state = WAIT_FOR_CLOCK;
			nextEvent = CLOCK_GRAIN;
		}
		break;
	}

	dol->clockSlice -= nextEvent;
	mTimingSchedule(timing, &dol->event, nextEvent);
}

void _flush(struct GBASIODolphin* dol) {
	uint8_t buffer[32];
	while (SocketRecv(dol->clock, buffer, sizeof(buffer)) == sizeof(buffer));
	while (SocketRecv(dol->data, buffer, sizeof(buffer)) == sizeof(buffer));
}

int32_t _processCommand(struct GBASIODolphin* dol, uint32_t cyclesLate) {
	// This does not include the stop bits due to compatibility reasons
	int bitsOnLine = 8;
	uint8_t buffer[6];
	int gotten = SocketRecv(dol->data, buffer, 1);
	if (gotten < 1) {
		return -1;
	}

	switch (buffer[0]) {
	case JOY_RESET:
	case JOY_POLL:
		bitsOnLine += 24;
		break;
	case JOY_RECV:
		gotten = SocketRecv(dol->data, &buffer[1], 4);
		if (gotten < 4) {
			return -1;
		}
		mLOG(GBA_SIO, DEBUG, "DOL recv: %02X%02X%02X%02X", buffer[1], buffer[2], buffer[3], buffer[4]);
		// Fall through
	case JOY_TRANS:
		bitsOnLine += 40;
		break;
	}

	if (!dol->active) {
		return 0;
	}

	int sent = GBASIOJOYSendCommand(&dol->d, buffer[0], &buffer[1]);
	SocketSend(dol->data, &buffer[1], sent);

	return bitsOnLine * CYCLES_PER_BIT - cyclesLate;
}

bool GBASIODolphinIsConnected(struct GBASIODolphin* dol) {
	return dol->data != INVALID_SOCKET;
}
