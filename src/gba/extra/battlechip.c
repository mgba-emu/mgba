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
	BATTLECHIP_STATE_SYNC = -1,
	BATTLECHIP_STATE_COMMAND = 0,
	BATTLECHIP_STATE_UNK_0 = 1,
	BATTLECHIP_STATE_UNK_1 = 2,
	BATTLECHIP_STATE_DATA_0 = 3,
	BATTLECHIP_STATE_DATA_1 = 4,
	BATTLECHIP_STATE_ID = 5,
	BATTLECHIP_STATE_UNK_2 = 6,
	BATTLECHIP_STATE_UNK_3 = 7,
	BATTLECHIP_STATE_END = 8
};

enum {
	BATTLECHIP_OK = 0xFFC6,
	PROGRESS_GATE_OK = 0xFFC7,
	BEAST_LINK_GATE_OK = 0xFFC4,
	BEAST_LINK_GATE_US_OK = 0xFF00,
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

	gate->chipId = 0;
	gate->flavor = GBA_FLAVOR_BATTLECHIP_GATE;
}

bool GBASIOBattlechipGateLoad(struct GBASIODriver* driver) {
	struct GBASIOBattlechipGate* gate = (struct GBASIOBattlechipGate*) driver;
	gate->state = BATTLECHIP_STATE_SYNC;
	gate->data[0] = 0x00FE;
	gate->data[1] = 0xFFFE;
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
	int32_t cycles;
	if (gate->d.p->mode == SIO_NORMAL_32) {
		cycles = GBA_ARM7TDMI_FREQUENCY / 0x40000;
	} else {
		cycles = GBASIOCyclesPerTransfer[GBASIOMultiplayerGetBaud(gate->d.p->siocnt)][1];
	}
	mTimingDeschedule(&gate->d.p->p->timing, &gate->event);
	mTimingSchedule(&gate->d.p->p->timing, &gate->event, cycles);
}

void _battlechipTransferEvent(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	UNUSED(timing);
	struct GBASIOBattlechipGate* gate = user;

	if (gate->d.p->mode == SIO_NORMAL_32) {
		gate->d.p->p->memory.io[REG_SIODATA32_LO >> 1] = 0;
		gate->d.p->p->memory.io[REG_SIODATA32_HI >> 1] = 0;
		gate->d.p->siocnt = GBASIONormalClearStart(gate->d.p->siocnt);
		if (GBASIONormalIsIrq(gate->d.p->siocnt)) {
			GBARaiseIRQ(gate->d.p->p, GBA_IRQ_SIO, cyclesLate);
		}
		return;
	}

	uint16_t cmd = gate->d.p->p->memory.io[REG_SIOMLT_SEND >> 1];
	uint16_t reply = 0xFFFF;
	gate->d.p->p->memory.io[REG_SIOMULTI0 >> 1] = cmd;
	gate->d.p->p->memory.io[REG_SIOMULTI2 >> 1] = 0xFFFF;
	gate->d.p->p->memory.io[REG_SIOMULTI3 >> 1] = 0xFFFF;
	gate->d.p->siocnt = GBASIOMultiplayerClearBusy(gate->d.p->siocnt);
	gate->d.p->siocnt = GBASIOMultiplayerSetId(gate->d.p->siocnt, 0);

	mLOG(GBA_BATTLECHIP, DEBUG, "Game: %04X (%i)", cmd, gate->state);

	uint16_t ok;
	switch (gate->flavor) {
	case GBA_FLAVOR_BATTLECHIP_GATE:
	default:
		ok = BATTLECHIP_OK;
		break;
	case GBA_FLAVOR_PROGRESS_GATE:
		ok = PROGRESS_GATE_OK;
		break;
	case GBA_FLAVOR_BEAST_LINK_GATE:
		ok = BEAST_LINK_GATE_OK;
		break;
	case GBA_FLAVOR_BEAST_LINK_GATE_US:
		ok = BEAST_LINK_GATE_US_OK;
		break;
	}

	if (gate->state != BATTLECHIP_STATE_COMMAND) {
		// Resync if needed
		switch (cmd) {
		// EXE 5, 6
		case 0xA380:
		case 0xA390:
		case 0xA3A0:
		case 0xA3B0:
		case 0xA3C0:
		case 0xA3D0:
		// EXE 4
		case 0xA6C0:
		mLOG(GBA_BATTLECHIP, DEBUG, "Resync detected");
			gate->state = BATTLECHIP_STATE_SYNC;
			break;
		}
	}

	switch (gate->state) {
	case BATTLECHIP_STATE_SYNC:
		if (cmd != 0x8FFF) {
			--gate->state;
		}
		// Fall through
	case BATTLECHIP_STATE_COMMAND:
		reply = ok;
		break;
	case BATTLECHIP_STATE_UNK_0:
	case BATTLECHIP_STATE_UNK_1:
		reply = 0xFFFF;
		break;
	case BATTLECHIP_STATE_DATA_0:
		reply = gate->data[0];
		gate->data[0] += 3;
		gate->data[0] &= 0x00FF;
		break;
	case BATTLECHIP_STATE_DATA_1:
		reply = gate->data[1];
		gate->data[1] -= 3;
		gate->data[1] |= 0xFC00;
		break;
	case BATTLECHIP_STATE_ID:
		reply = gate->chipId;
		break;
	case BATTLECHIP_STATE_UNK_2:
	case BATTLECHIP_STATE_UNK_3:
		reply = 0;
		break;
	case BATTLECHIP_STATE_END:
		reply = ok;
		gate->state = BATTLECHIP_STATE_SYNC;
		break;
	}

	mLOG(GBA_BATTLECHIP, DEBUG, "Gate: %04X (%i)", reply, gate->state);
	++gate->state;

	gate->d.p->p->memory.io[REG_SIOMULTI1 >> 1] = reply;

	if (GBASIOMultiplayerIsIrq(gate->d.p->siocnt)) {
		GBARaiseIRQ(gate->d.p->p, GBA_IRQ_SIO, cyclesLate);
	}
}
