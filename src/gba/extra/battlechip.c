/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/gba/interface.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/sio.h>

mLOG_DECLARE_CATEGORY(GBA_BATTLECHIP);
mLOG_DEFINE_CATEGORY(GBA_BATTLECHIP, "GBA BattleChip Gate", "gba.battlechip");

enum {
	BATTLECHIP_INDEX_HANDSHAKE_0 = 0,
	BATTLECHIP_INDEX_HANDSHAKE_1 = 1,
	BATTLECHIP_INDEX_ID = 2,
	BATTLECHIP_INDEX_END = 6
};

enum {
	BATTLECHIP_OK = 0xFFC6,
	BATTLECHIP_CONTINUE = 0xFFFF,
};

static bool GBASIOBattlechipGateLoad(struct GBASIODriver* driver);
static uint16_t GBASIOBattlechipGateWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value);

static void _battlechipTransfer(struct GBASIOBattlechipGate* gate);
static void _battlechipTransferEvent(struct mTiming* timing, void* user, uint32_t cyclesLate);

void GBASIOBattlechipGateCreate(struct GBASIOBattlechipGate* gate) {
	gate->d.init = NULL;
	gate->d.deinit = NULL;
	gate->d.load = GBASIOBattlechipGateLoad;
	gate->d.unload = NULL;
	gate->d.writeRegister = GBASIOBattlechipGateWriteRegister;

	gate->event.context = gate;
	gate->event.callback = _battlechipTransferEvent;
	gate->event.priority = 0x80;
}

bool GBASIOBattlechipGateLoad(struct GBASIODriver* driver) {
	struct GBASIOBattlechipGate* gate = (struct GBASIOBattlechipGate*) driver;
	gate->index = BATTLECHIP_INDEX_END;
	return true;
}

uint16_t GBASIOBattlechipGateWriteRegister(struct GBASIODriver* driver, uint32_t address, uint16_t value) {
	struct GBASIOBattlechipGate* gate = (struct GBASIOBattlechipGate*) driver;
	switch (address) {
	case REG_SIOCNT:
		value &= ~0xC;
		value |= 0x8;
		if (value & 0x80) {
			_battlechipTransfer(gate);
		}
		break;
	case REG_SIOMLT_SEND:
		break;
	case REG_RCNT:
		break;
	default:
		break;
	}
	return value;
}

void _battlechipTransfer(struct GBASIOBattlechipGate* gate) {
	int32_t cycles = GBASIOCyclesPerTransfer[gate->d.p->multiplayerControl.baud][1];
	mTimingSchedule(&gate->d.p->p->timing, &gate->event, cycles);
}

void _battlechipTransferEvent(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	struct GBASIOBattlechipGate* gate = user;

	uint16_t cmd = gate->d.p->p->memory.io[REG_SIOMLT_SEND >> 1];
	uint16_t reply = 0xFFFF;
	gate->d.p->p->memory.io[REG_SIOMULTI0 >> 1] = cmd;
	gate->d.p->p->memory.io[REG_SIOMULTI2 >> 1] = 0xFFFF;
	gate->d.p->p->memory.io[REG_SIOMULTI3 >> 1] = 0xFFFF;
	gate->d.p->multiplayerControl.busy = 0;
	gate->d.p->multiplayerControl.id = 0;

	mLOG(GBA_BATTLECHIP, DEBUG, "> %04x", cmd);

	switch (cmd) {
	case 0x4000:
		gate->index = 0;
	// Fall through
	case 0:
		switch (gate->index) {
		case BATTLECHIP_INDEX_HANDSHAKE_0:
			reply = 0x00FE;
			break;
		case BATTLECHIP_INDEX_HANDSHAKE_1:
			reply = 0xFFFE;
			break;
		case BATTLECHIP_INDEX_ID:
			reply = gate->chipId;
			break;
		default:
			if (gate->index >= BATTLECHIP_INDEX_END) {
				reply = BATTLECHIP_OK;
			} else if (gate->index < 0) {
				reply = BATTLECHIP_CONTINUE;
			} else {
				reply = 0;
			}
			break;
		}
		++gate->index;
		break;
	case 0x8FFF:
		gate->index = -2;
	// Fall through
	default:
	case 0xA3D0:
		reply = BATTLECHIP_OK;
		break;
	case 0x4234:
	case 0x574A:
		reply = BATTLECHIP_CONTINUE;
		break;
	}

	mLOG(GBA_BATTLECHIP, DEBUG, "< %04x", reply);

	gate->d.p->p->memory.io[REG_SIOMULTI1 >> 1] = reply;

	if (gate->d.p->multiplayerControl.irq) {
		GBARaiseIRQ(gate->d.p->p, IRQ_SIO);
	}
}
