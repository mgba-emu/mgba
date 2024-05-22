/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sio.h>

#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/io.h>
#include <mgba/internal/gba/sio/gbp.h>

mLOG_DEFINE_CATEGORY(GBA_SIO, "GBA Serial I/O", "gba.sio");

static const int GBASIOCyclesPerTransfer[4][MAX_GBAS] = {
	{ 31976, 63427, 94884, 125829 },
	{ 8378, 16241, 24104, 31457 },
	{ 5750, 10998, 16241, 20972 },
	{ 3140, 5755, 8376, 10486 }
};

static struct GBASIODriver* _lookupDriver(struct GBASIO* sio, enum GBASIOMode mode) {
	switch (mode) {
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
		return sio->drivers.normal;
	case GBA_SIO_MULTI:
		return sio->drivers.multiplayer;
	case GBA_SIO_JOYBUS:
		return sio->drivers.joybus;
	default:
		return 0;
	}
}

static const char* _modeName(enum GBASIOMode mode) {
	switch (mode) {
	case GBA_SIO_NORMAL_8:
		return "NORMAL8";
	case GBA_SIO_NORMAL_32:
		return "NORMAL32";
	case GBA_SIO_MULTI:
		return "MULTI";
	case GBA_SIO_JOYBUS:
		return "JOYBUS";
	case GBA_SIO_GPIO:
		return "GPIO";
	default:
		return "(unknown)";
	}
}

static void _switchMode(struct GBASIO* sio) {
	unsigned mode = ((sio->rcnt & 0xC000) | (sio->siocnt & 0x3000)) >> 12;
	enum GBASIOMode newMode;
	if (mode < 8) {
		newMode = (enum GBASIOMode) (mode & 0x3);
	} else {
		newMode = (enum GBASIOMode) (mode & 0xC);
	}
	if (newMode != sio->mode) {
		if (sio->activeDriver && sio->activeDriver->unload) {
			sio->activeDriver->unload(sio->activeDriver);
		}
		if (sio->mode != (enum GBASIOMode) -1) {
			mLOG(GBA_SIO, DEBUG, "Switching mode from %s to %s", _modeName(sio->mode), _modeName(newMode));
		}
		sio->mode = newMode;
		sio->activeDriver = _lookupDriver(sio, sio->mode);
		if (sio->activeDriver && sio->activeDriver->load) {
			sio->activeDriver->load(sio->activeDriver);
		}

		int id = 0;
		switch (newMode) {
		case GBA_SIO_MULTI:
			if (sio->activeDriver && sio->activeDriver->deviceId) {
				id = sio->activeDriver->deviceId(sio->activeDriver);
			}
			if (id) {
				sio->rcnt |= 4;
			} else {
				sio->rcnt &= ~4;
			}
			break;
		default:
			// TODO
			break;
		}
	}
}

void GBASIOInit(struct GBASIO* sio) {
	sio->drivers.normal = 0;
	sio->drivers.multiplayer = 0;
	sio->drivers.joybus = 0;
	sio->activeDriver = 0;

	sio->gbp.p = sio->p;
	GBASIOPlayerInit(&sio->gbp);

	GBASIOReset(sio);
}

void GBASIODeinit(struct GBASIO* sio) {
	if (sio->activeDriver && sio->activeDriver->unload) {
		sio->activeDriver->unload(sio->activeDriver);
	}
	if (sio->drivers.multiplayer && sio->drivers.multiplayer->deinit) {
		sio->drivers.multiplayer->deinit(sio->drivers.multiplayer);
	}
	if (sio->drivers.joybus && sio->drivers.joybus->deinit) {
		sio->drivers.joybus->deinit(sio->drivers.joybus);
	}
	if (sio->drivers.normal && sio->drivers.normal->deinit) {
		sio->drivers.normal->deinit(sio->drivers.normal);
	}
}

void GBASIOReset(struct GBASIO* sio) {
	if (sio->activeDriver && sio->activeDriver->unload) {
		sio->activeDriver->unload(sio->activeDriver);
	}
	if (sio->drivers.multiplayer && sio->drivers.multiplayer->reset) {
		sio->drivers.multiplayer->reset(sio->drivers.multiplayer);
	}
	if (sio->drivers.joybus && sio->drivers.joybus->reset) {
		sio->drivers.joybus->reset(sio->drivers.joybus);
	}
	if (sio->drivers.normal && sio->drivers.normal->reset) {
		sio->drivers.normal->reset(sio->drivers.normal);
	}
	sio->rcnt = RCNT_INITIAL;
	sio->siocnt = 0;
	sio->mode = -1;
	sio->activeDriver = NULL;
	_switchMode(sio);

	GBASIOPlayerReset(&sio->gbp);
}

void GBASIOSetDriverSet(struct GBASIO* sio, struct GBASIODriverSet* drivers) {
	GBASIOSetDriver(sio, drivers->normal, GBA_SIO_NORMAL_8);
	GBASIOSetDriver(sio, drivers->multiplayer, GBA_SIO_MULTI);
	GBASIOSetDriver(sio, drivers->joybus, GBA_SIO_JOYBUS);
}

void GBASIOSetDriver(struct GBASIO* sio, struct GBASIODriver* driver, enum GBASIOMode mode) {
	struct GBASIODriver** driverLoc;
	switch (mode) {
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
		driverLoc = &sio->drivers.normal;
		break;
	case GBA_SIO_MULTI:
		driverLoc = &sio->drivers.multiplayer;
		break;
	case GBA_SIO_JOYBUS:
		driverLoc = &sio->drivers.joybus;
		break;
	default:
		mLOG(GBA_SIO, ERROR, "Setting an unsupported SIO driver: %x", mode);
		return;
	}
	if (*driverLoc) {
		if ((*driverLoc)->unload) {
			(*driverLoc)->unload(*driverLoc);
		}
		if ((*driverLoc)->deinit) {
			(*driverLoc)->deinit(*driverLoc);
		}
	}
	if (driver) {
		driver->p = sio;

		if (driver->init) {
			if (!driver->init(driver)) {
				driver->deinit(driver);
				mLOG(GBA_SIO, ERROR, "Could not initialize SIO driver");
				return;
			}
		}
	}
	if (sio->activeDriver == *driverLoc) {
		sio->activeDriver = driver;
		if (driver && driver->load) {
			driver->load(driver);
		}
	}
	*driverLoc = driver;
}

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value) {
	sio->rcnt &= 0xF;
	sio->rcnt |= value & ~0xF;
	_switchMode(sio);
	if (sio->activeDriver && sio->activeDriver->writeRegister) {
		sio->activeDriver->writeRegister(sio->activeDriver, GBA_REG_RCNT, value);
	}
}

void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value) {
	if ((value ^ sio->siocnt) & 0x3000) {
		sio->siocnt = value & 0x3000;
		_switchMode(sio);
	}
	int id = 0;
	int connected = 0;
	bool handled = false;
	if (sio->activeDriver) {
		handled = sio->activeDriver->handlesMode(sio->activeDriver, sio->mode);
		if (handled) {
			if (sio->activeDriver->deviceId) {
				id = sio->activeDriver->deviceId(sio->activeDriver);
			}
			connected = sio->activeDriver->connectedDevices(sio->activeDriver);
			handled = !!sio->activeDriver->writeRegister;
		}
	}

	switch (sio->mode) {
	case GBA_SIO_MULTI:
		value &= 0xFF83;
		value = GBASIOMultiplayerSetSlave(value, id || !connected);
		value = GBASIOMultiplayerSetId(value, id);
		value |= sio->siocnt & 0x00FC;
		break;
	default:
		// TODO
		break;
	}
	if (handled) {
		value = sio->activeDriver->writeRegister(sio->activeDriver, GBA_REG_SIOCNT, value);
	} else {
		// Dummy drivers
		switch (sio->mode) {
		case GBA_SIO_NORMAL_8:
		case GBA_SIO_NORMAL_32:
			value = GBASIONormalFillSi(value);
			if ((value & 0x0081) == 0x0081) {
				if (GBASIONormalIsIrq(value)) {
					// TODO: Test this on hardware to see if this is correct
					GBARaiseIRQ(sio->p, GBA_IRQ_SIO, 0);
				}
				value = GBASIONormalClearStart(value);
			}
			break;
		case GBA_SIO_MULTI:
			value = GBASIOMultiplayerFillReady(value);
			break;
		default:
			// TODO
			break;
		}
	}
	sio->siocnt = value;
}

uint16_t GBASIOWriteRegister(struct GBASIO* sio, uint32_t address, uint16_t value) {
	int id = 0;
	if (sio->activeDriver && sio->activeDriver->deviceId) {
		id = sio->activeDriver->deviceId(sio->activeDriver);
	}

	switch (sio->mode) {
	case GBA_SIO_JOYBUS:
		switch (address) {
		case GBA_REG_JOYCNT:
			mLOG(GBA_SIO, DEBUG, "JOY write: CNT <- %04X", value);
			value = (value & 0x0040) | (sio->p->memory.io[GBA_REG(JOYCNT)] & ~(value & 0x7) & ~0x0040);
			break;
		case GBA_REG_JOYSTAT:
			mLOG(GBA_SIO, DEBUG, "JOY write: STAT <- %04X", value);
			value = (value & 0x0030) | (sio->p->memory.io[GBA_REG(JOYSTAT)] & ~0x30);
			break;
		case GBA_REG_JOY_TRANS_LO:
			mLOG(GBA_SIO, DEBUG, "JOY write: TRANS_LO <- %04X", value);
			break;
		case GBA_REG_JOY_TRANS_HI:
			mLOG(GBA_SIO, DEBUG, "JOY write: TRANS_HI <- %04X", value);
			break;
		default:
			mLOG(GBA_SIO, DEBUG, "JOY write: Unknown reg %03X <- %04X", address, value);
			break;
		case GBA_REG_RCNT:
			break;
		}
		break;
	case GBA_SIO_NORMAL_8:
		switch (address) {
		case GBA_REG_SIODATA8:
			mLOG(GBA_SIO, DEBUG, "NORMAL8 %i write: SIODATA8 <- %02X", id, value);
			break;
		default:
			mLOG(GBA_SIO, DEBUG, "NORMAL8 %i write: Unknown reg %03X <- %04X", id, address, value);
			break;
		case GBA_REG_RCNT:
			break;
		}
		break;
	case GBA_SIO_NORMAL_32:
		switch (address) {
		case GBA_REG_SIODATA32_LO:
			mLOG(GBA_SIO, DEBUG, "NORMAL32 %i write: SIODATA32_LO <- %04X", id, value);
			break;
		case GBA_REG_SIODATA32_HI:
			mLOG(GBA_SIO, DEBUG, "NORMAL32 %i write: SIODATA32_HI <- %04X", id, value);
			break;
		default:
			mLOG(GBA_SIO, DEBUG, "NORMAL32 %i write: Unknown reg %03X <- %04X", id, address, value);
			break;
		case GBA_REG_RCNT:
			break;
		}
		break;
	case GBA_SIO_MULTI:
		switch (address) {
		case GBA_REG_SIOMLT_SEND:
			mLOG(GBA_SIO, DEBUG, "MULTI %i write: SIOMLT_SEND <- %04X", id, value);
			break;
		default:
			mLOG(GBA_SIO, DEBUG, "MULTI %i write: Unknown reg %03X <- %04X", id, address, value);
			break;
		case GBA_REG_RCNT:
			break;
		}
		break;
	default:
		// TODO
		break;
	}

	if (sio->activeDriver && sio->activeDriver->writeRegister && sio->activeDriver->handlesMode(sio->activeDriver, sio->mode)) {
		sio->activeDriver->writeRegister(sio->activeDriver, address, value);
	}
	return value;
}

int32_t GBASIOTransferCycles(struct GBASIO* sio) {
	int connected = 0;
	if (sio->activeDriver) {
		connected = sio->activeDriver->connectedDevices(sio->activeDriver);
	}

	if (connected < 0 || connected >= MAX_GBAS) {
		mLOG(GBA_SIO, ERROR, "SIO driver returned invalid device count %i", connected);
		return 0;
	}

	switch (sio->mode) {
	case GBA_SIO_MULTI:
		return GBASIOCyclesPerTransfer[GBASIOMultiplayerGetBaud(sio->siocnt)][connected];
	case GBA_SIO_NORMAL_8:
		return 8 * GBA_ARM7TDMI_FREQUENCY / ((GBASIONormalIsInternalSc(sio->siocnt) ? 2048 : 256) * 1024);
	case GBA_SIO_NORMAL_32:
		return 32 * GBA_ARM7TDMI_FREQUENCY / ((GBASIONormalIsInternalSc(sio->siocnt) ? 2048 : 256) * 1024);
	default:
		mLOG(GBA_SIO, STUB, "No cycle count implemented for mode %s", _modeName(sio->mode));
		break;
	}
	return 0;
}

void GBASIOMultiplayerFinishTransfer(struct GBASIO* sio, uint16_t data[4], uint32_t cyclesLate) {
	int id = 0;
	if (sio->activeDriver && sio->activeDriver->deviceId) {
		id = sio->activeDriver->deviceId(sio->activeDriver);
	}
	sio->p->memory.io[GBA_REG(SIOMULTI0)] = data[0];
	sio->p->memory.io[GBA_REG(SIOMULTI1)] = data[1];
	sio->p->memory.io[GBA_REG(SIOMULTI2)] = data[2];
	sio->p->memory.io[GBA_REG(SIOMULTI3)] = data[3];
	sio->rcnt |= 1;
	sio->siocnt = GBASIOMultiplayerClearBusy(sio->siocnt);
	sio->siocnt = GBASIOMultiplayerSetId(sio->siocnt, id);
	if (GBASIOMultiplayerIsIrq(sio->siocnt)) {
		GBARaiseIRQ(sio->p, GBA_IRQ_SIO, cyclesLate);
	}
}

void GBASIONormal8FinishTransfer(struct GBASIO* sio, uint8_t data, uint32_t cyclesLate) {
	sio->siocnt = GBASIONormalClearStart(sio->siocnt);
	sio->p->memory.io[GBA_REG(SIODATA8)] = data;
	if (GBASIONormalIsIrq(sio->siocnt)) {
		GBARaiseIRQ(sio->p, GBA_IRQ_SIO, cyclesLate);
	}
}

void GBASIONormal32FinishTransfer(struct GBASIO* sio, uint32_t data, uint32_t cyclesLate) {
	sio->siocnt = GBASIONormalClearStart(sio->siocnt);
	sio->p->memory.io[GBA_REG(SIODATA32_LO)] = data;
	sio->p->memory.io[GBA_REG(SIODATA32_HI)] = data >> 16;
	if (GBASIONormalIsIrq(sio->siocnt)) {
		GBARaiseIRQ(sio->p, GBA_IRQ_SIO, cyclesLate);
	}
}

int GBASIOJOYSendCommand(struct GBASIODriver* sio, enum GBASIOJOYCommand command, uint8_t* data) {
	switch (command) {
	case JOY_RESET:
		sio->p->p->memory.io[GBA_REG(JOYCNT)] |= JOYCNT_RESET;
		if (sio->p->p->memory.io[GBA_REG(JOYCNT)] & 0x40) {
			GBARaiseIRQ(sio->p->p, GBA_IRQ_SIO, 0);
		}
		// Fall through
	case JOY_POLL:
		data[0] = 0x00;
		data[1] = 0x04;
		data[2] = sio->p->p->memory.io[GBA_REG(JOYSTAT)];

		mLOG(GBA_SIO, DEBUG, "JOY %s: %02X (%02X)", command == JOY_POLL ? "poll" : "reset", data[2], sio->p->p->memory.io[GBA_REG(JOYCNT)]);
		return 3;
	case JOY_RECV:
		sio->p->p->memory.io[GBA_REG(JOYCNT)] |= JOYCNT_RECV;
		sio->p->p->memory.io[GBA_REG(JOYSTAT)] |= JOYSTAT_RECV;

		sio->p->p->memory.io[GBA_REG(JOY_RECV_LO)] = data[0] | (data[1] << 8);
		sio->p->p->memory.io[GBA_REG(JOY_RECV_HI)] = data[2] | (data[3] << 8);

		data[0] = sio->p->p->memory.io[GBA_REG(JOYSTAT)];

		mLOG(GBA_SIO, DEBUG, "JOY recv: %02X (%02X)", data[0], sio->p->p->memory.io[GBA_REG(JOYCNT)]);

		if (sio->p->p->memory.io[GBA_REG(JOYCNT)] & 0x40) {
			GBARaiseIRQ(sio->p->p, GBA_IRQ_SIO, 0);
		}
		return 1;
	case JOY_TRANS:
		data[0] = sio->p->p->memory.io[GBA_REG(JOY_TRANS_LO)];
		data[1] = sio->p->p->memory.io[GBA_REG(JOY_TRANS_LO)] >> 8;
		data[2] = sio->p->p->memory.io[GBA_REG(JOY_TRANS_HI)];
		data[3] = sio->p->p->memory.io[GBA_REG(JOY_TRANS_HI)] >> 8;
		data[4] = sio->p->p->memory.io[GBA_REG(JOYSTAT)];

		sio->p->p->memory.io[GBA_REG(JOYCNT)] |= JOYCNT_TRANS;
		sio->p->p->memory.io[GBA_REG(JOYSTAT)] &= ~JOYSTAT_TRANS;

		mLOG(GBA_SIO, DEBUG, "JOY trans: %02X%02X%02X%02X:%02X (%02X)", data[0], data[1], data[2], data[3], data[4], sio->p->p->memory.io[GBA_REG(JOYCNT)]);

		if (sio->p->p->memory.io[GBA_REG(JOYCNT)] & 0x40) {
			GBARaiseIRQ(sio->p->p, GBA_IRQ_SIO, 0);
		}
		return 5;
	}
	return 0;
}
