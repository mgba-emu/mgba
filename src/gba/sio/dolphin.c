/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "dolphin.h"

#include "gba/io.h"

#define CLOCK_GRAIN 2048
#define CYCLES_PER_BIT 75

const uint16_t DOLPHIN_CLOCK_PORT = 49420;
const uint16_t DOLPHIN_DATA_PORT = 54970;

enum {
	CMD_RESET = 0xFF,
	CMD_POLL = 0x00,
	CMD_TRANS = 0x14,
	CMD_RECV = 0x15,
};

// static bool GBASIODolphinInit(struct GBASIODriver* driver);
// static void GBASIODolphinDeinit(struct GBASIODriver* driver);
static bool GBASIODolphinLoad(struct GBASIODriver* driver);
// static bool GBASIODolphinUnload(struct GBASIODriver* driver);
static uint16_t GBASIODolphinWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);
static int32_t GBASIODolphinProcessEvents(struct GBASIODriver* driver, int32_t cycles);

void GBASIODolphinCreate(struct GBASIODolphin* dol) {
	dol->d.init = 0;
	dol->d.deinit = 0;
	dol->d.load = GBASIODolphinLoad;
	dol->d.unload = 0;
	dol->d.writeRegister = GBASIODolphinWriteRegister;
	dol->d.processEvents = GBASIODolphinProcessEvents;

	dol->data = INVALID_SOCKET;
	dol->clock = INVALID_SOCKET;
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
		SocketClose(dol->clock);
		return false;
	}

	SocketSetBlocking(dol->data, false);
	SocketSetBlocking(dol->clock, false);
	return true;
}

static bool GBASIODolphinLoad(struct GBASIODriver* driver) {
	struct GBASIODolphin* dol = (struct GBASIODolphin*) driver;
	dol->nextEvent = 0;
	return true;
}

uint16_t GBASIODolphinWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIODolphin* dol = (struct GBASIODolphin*) driver;
	switch (address) {
	case REG_JOYCNT:
		return (value & 0x0040) | (dol->d.p->p->memory.io[REG_JOYCNT >> 1] & ~(value & 0x7) & ~0x0040);
	case REG_JOYSTAT:
		return (value & 0x0030) | (dol->d.p->p->memory.io[REG_JOYSTAT >> 1] & ~0x30);
	case REG_JOY_TRANS_LO:
	case REG_JOY_TRANS_HI:
		dol->d.p->p->memory.io[REG_JOYSTAT >> 1] |= 8;
		break;
	}
	return value;
}

int32_t GBASIODolphinProcessEvents(struct GBASIODriver* driver, int32_t cycles) {
	struct GBASIODolphin* dol = (struct GBASIODolphin*) driver;
	dol->nextEvent -= cycles;
	if (dol->nextEvent <= 0) {
		uint8_t command;
		int32_t nextEvent = CLOCK_GRAIN;
		uint32_t clocks = 0;
		uint32_t clockSlice;
		while (SocketRecv(dol->clock, &clockSlice, 4) == 4) {
			clocks += ntohl(clockSlice);
		}
		while (SocketRecv(dol->data, &command, 1) == 1) {
			int bitsOnLine = 8 + 1;
			uint8_t buffer[5];

			switch (command) {
			case CMD_RESET:
				dol->d.p->p->memory.io[REG_JOYCNT >> 1] |= 1;
				if (dol->d.p->p->memory.io[REG_JOYCNT >> 1] & 0x40) {
					GBARaiseIRQ(dol->d.p->p, IRQ_SIO);
				}
				// Fall through
			case CMD_POLL:
				buffer[0] = 0x00;
				buffer[1] = 0x04;
				buffer[2] = dol->d.p->p->memory.io[REG_JOYSTAT >> 1];
				SocketSend(dol->data, buffer, 3);
				bitsOnLine += 24 + 1;
				break;
			case CMD_RECV:
				dol->d.p->p->memory.io[REG_JOYCNT >> 1] |= 2;
				dol->d.p->p->memory.io[REG_JOYSTAT >> 1] |= 2;
				SocketRecv(dol->data, &buffer, 4);
				dol->d.p->p->memory.io[REG_JOY_RECV_LO >> 1] = buffer[0] | (buffer[1] << 8);
				dol->d.p->p->memory.io[REG_JOY_RECV_HI >> 1] = buffer[2] | (buffer[3] << 8);
				buffer[0] = dol->d.p->p->memory.io[REG_JOYSTAT >> 1];
				SocketSend(dol->data, buffer, 1);
				if (dol->d.p->p->memory.io[REG_JOYCNT >> 1] & 0x40) {
					GBARaiseIRQ(dol->d.p->p, IRQ_SIO);
				}
				bitsOnLine += 40 + 1;
				break;
			case CMD_TRANS:
				dol->d.p->p->memory.io[REG_JOYCNT >> 1] |= 4;
				dol->d.p->p->memory.io[REG_JOYSTAT >> 1] &= ~8;
				buffer[0] = dol->d.p->p->memory.io[REG_JOY_TRANS_LO >> 1];
				buffer[1] = dol->d.p->p->memory.io[REG_JOY_TRANS_LO >> 1] >> 8;
				buffer[2] = dol->d.p->p->memory.io[REG_JOY_TRANS_HI >> 1];
				buffer[3] = dol->d.p->p->memory.io[REG_JOY_TRANS_HI >> 1] >> 8;
				buffer[4] = dol->d.p->p->memory.io[REG_JOYSTAT >> 1];
				SocketSend(dol->data, buffer, 5);
				if (dol->d.p->p->memory.io[REG_JOYCNT >> 1] & 0x40) {
					GBARaiseIRQ(dol->d.p->p, IRQ_SIO);
				}
				bitsOnLine += 40 + 1;
				break;
			}
			nextEvent = bitsOnLine * CYCLES_PER_BIT;
		}
		dol->nextEvent += nextEvent;
	}
	return dol->nextEvent;
}
