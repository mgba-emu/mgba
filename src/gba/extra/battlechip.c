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

static bool GBASIOBattlechipGateInit(struct GBASIODriver* driver);
static uint16_t GBASIOBattlechipGateWriteSIOCNT(struct GBASIODriver* driver, uint16_t value);
static bool GBASIOBattlechipGateHandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode);
static int GBASIOBattlechipGateConnectedDevices(struct GBASIODriver* driver);
static void GBASIOBattlechipGateFinishMultiplayer(struct GBASIODriver* driver, uint16_t data[4]);

void GBASIOBattlechipGateCreate(struct GBASIOBattlechipGate* gate) {
	memset(&gate->d, 0, sizeof(gate->d));
	gate->d.init = GBASIOBattlechipGateInit;
	gate->d.writeSIOCNT = GBASIOBattlechipGateWriteSIOCNT;
	gate->d.handlesMode = GBASIOBattlechipGateHandlesMode;
	gate->d.connectedDevices = GBASIOBattlechipGateConnectedDevices;
	gate->d.finishMultiplayer = GBASIOBattlechipGateFinishMultiplayer;

	gate->chipId = 0;
	gate->flavor = GBA_FLAVOR_BATTLECHIP_GATE;
}

bool GBASIOBattlechipGateInit(struct GBASIODriver* driver) {
	struct GBASIOBattlechipGate* gate = (struct GBASIOBattlechipGate*) driver;
	gate->state = BATTLECHIP_STATE_SYNC;
	gate->data[0] = 0x00FE;
	gate->data[1] = 0xFFFE;
	return true;
}

uint16_t GBASIOBattlechipGateWriteSIOCNT(struct GBASIODriver* driver, uint16_t value) {
	UNUSED(driver);
	value &= ~0xC;
	value |= 0x8;
	return value;
}

static bool GBASIOBattlechipGateHandlesMode(struct GBASIODriver* driver, enum GBASIOMode mode) {
	UNUSED(driver);
	switch (mode) {
	case GBA_SIO_NORMAL_32:
	case GBA_SIO_MULTI:
		return true;
	default:
		return false;
	}
}

static int GBASIOBattlechipGateConnectedDevices(struct GBASIODriver* driver) {
	UNUSED(driver);
	return 1;
}

static void GBASIOBattlechipGateFinishMultiplayer(struct GBASIODriver* driver, uint16_t data[4]) {
	struct GBASIOBattlechipGate* gate = (struct GBASIOBattlechipGate*) driver;

	uint16_t cmd = gate->d.p->p->memory.io[GBA_REG(SIOMLT_SEND)];
	uint16_t reply = 0xFFFF;

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

	data[0] = cmd;
	data[1] = reply;
	data[2] = 0xFFFF;
	data[3] = 0xFFFF;
}
