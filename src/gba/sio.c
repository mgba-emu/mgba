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

static void _sioFinish(struct mTiming* timing, void* user, uint32_t cyclesLate);

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
		if (sio->mode != (enum GBASIOMode) -1) {
			mLOG(GBA_SIO, DEBUG, "Switching mode from %s to %s", _modeName(sio->mode), _modeName(newMode));
		}
		sio->mode = newMode;
		if (sio->driver && sio->driver->setMode) {
			sio->driver->setMode(sio->driver, newMode);
		}

		int id = 0;
		switch (newMode) {
		case GBA_SIO_MULTI:
			if (sio->driver && sio->driver->deviceId) {
				id = sio->driver->deviceId(sio->driver);
			}
			sio->rcnt = GBASIORegisterRCNTSetSi(sio->rcnt, !!id);
			break;
		default:
			// TODO
			break;
		}
	}
}

void GBASIOInit(struct GBASIO* sio) {
	sio->driver = NULL;

	sio->completeEvent.context = sio;
	sio->completeEvent.name = "GBA SIO Complete";
	sio->completeEvent.callback = _sioFinish;
	sio->completeEvent.priority = 0x80;

	sio->gbp.p = sio->p;
	GBASIOPlayerInit(&sio->gbp);

	GBASIOReset(sio);
}

void GBASIODeinit(struct GBASIO* sio) {
	if (sio->driver && sio->driver->deinit) {
		sio->driver->deinit(sio->driver);
	}
}

void GBASIOReset(struct GBASIO* sio) {
	if (sio->driver && sio->driver->reset) {
		sio->driver->reset(sio->driver);
	}
	sio->rcnt = RCNT_INITIAL;
	sio->siocnt = 0;
	sio->mode = -1;
	_switchMode(sio);

	GBASIOPlayerReset(&sio->gbp);
}

void GBASIOSetDriver(struct GBASIO* sio, struct GBASIODriver* driver) {
	if (sio->driver && sio->driver->deinit) {
		sio->driver->deinit(sio->driver);
	}
	sio->driver = driver;
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
}

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value) {
	sio->rcnt &= 0x1FF;
	sio->rcnt |= value & 0xC000;
	_switchMode(sio);
	if (sio->driver && sio->driver->writeRCNT) {
		switch (sio->mode) {
		case GBA_SIO_GPIO:
			sio->rcnt = (sio->driver->writeRCNT(sio->driver, value) & 0x01FF) | (sio->rcnt & 0xC000);
			break;
		default:
			sio->rcnt = (sio->driver->writeRCNT(sio->driver, value) & 0x01F0) | (sio->rcnt & 0xC00F);
		}
	} else if (sio->mode == GBA_SIO_GPIO) {
		sio->rcnt &= 0xC000;
		sio->rcnt |= value & 0x1FF;
	} else {
		sio->rcnt &= 0xC00F;
		sio->rcnt |= value & 0x1F0;
	}
}

static void _startTransfer(struct GBASIO* sio) {
	if (sio->driver && sio->driver->start) {
		if (!sio->driver->start(sio->driver)) {
			// Transfer completion is handled internally to the driver
			return;
		}
	}
	int connected = 0;
	if (sio->driver && sio->driver->connectedDevices) {
		connected = sio->driver->connectedDevices(sio->driver);
	}
	mTimingDeschedule(&sio->p->timing, &sio->completeEvent);
	mTimingSchedule(&sio->p->timing, &sio->completeEvent, GBASIOTransferCycles(sio->mode, sio->siocnt, connected));
}

void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value) {
	if ((value ^ sio->siocnt) & 0x3000) {
		sio->siocnt = value & 0x3000;
		_switchMode(sio);
	}
	int id = 0;
	int connected = 0;
	bool handled = false;
	if (sio->driver) {
		handled = sio->driver->handlesMode(sio->driver, sio->mode);
		if (handled) {
			if (sio->driver->deviceId) {
				id = sio->driver->deviceId(sio->driver);
			}
			connected = sio->driver->connectedDevices(sio->driver);
			handled = !!sio->driver->writeSIOCNT;
		}
	}

	switch (sio->mode) {
	case GBA_SIO_MULTI:
		value &= 0xFF83;
		value = GBASIOMultiplayerSetSlave(value, id || !connected);
		value = GBASIOMultiplayerSetId(value, id);
		value |= sio->siocnt & 0x00FC;

		// SC appears to float in multi mode when not doing a transfer. While
		// it does spike at the end of a transfer, it appears to die down after
		// around 20-30 microseconds. However, the docs on akkit.org
		// (http://www.akkit.org/info/gba_comms.html) say this is high until
		// a transfer starts and low while active. Further, the Mario Bros.
		// multiplayer expects SC to be high in multi mode. This needs better
		// investigation than I managed, apparently.
		sio->rcnt = GBASIORegisterRCNTFillSc(sio->rcnt);

		if (GBASIOMultiplayerIsBusy(value) && !GBASIOMultiplayerIsBusy(sio->siocnt)) {
			if (!id) {
				sio->p->memory.io[GBA_REG(SIOMULTI0)] = 0xFFFF;
				sio->p->memory.io[GBA_REG(SIOMULTI1)] = 0xFFFF;
				sio->p->memory.io[GBA_REG(SIOMULTI2)] = 0xFFFF;
				sio->p->memory.io[GBA_REG(SIOMULTI3)] = 0xFFFF;
				sio->rcnt = GBASIORegisterRCNTClearSc(sio->rcnt);
				_startTransfer(sio);
			} else {
				// TODO
			}
		}
		break;
	case GBA_SIO_NORMAL_8:
	case GBA_SIO_NORMAL_32:
		// This line is pulled up by the clock owner while the clock is idle.
		// If there is no clock owner it's just hi-Z.
		if (GBASIONormalGetSc(value)) {
			sio->rcnt = GBASIORegisterRCNTFillSc(sio->rcnt);
		}
		if (GBASIONormalIsStart(value) && !GBASIONormalIsStart(sio->siocnt)) {
			_startTransfer(sio);
		}
		break;
	default:
		// TODO
		break;
	}
	if (handled) {
		value = sio->driver->writeSIOCNT(sio->driver, value);
	} else {
		// Dummy drivers
		switch (sio->mode) {
		case GBA_SIO_NORMAL_8:
		case GBA_SIO_NORMAL_32:
			value = GBASIONormalFillSi(value);
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
	if (sio->driver && sio->driver->deviceId) {
		id = sio->driver->deviceId(sio->driver);
	}

	bool handled = true;
	switch (sio->mode) {
	case GBA_SIO_JOYBUS:
		switch (address) {
		case GBA_REG_SIODATA8:
			mLOG(GBA_SIO, DEBUG, "JOY write: SIODATA8 (?) <- %04X", value);
			break;
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
			mLOG(GBA_SIO, GAME_ERROR, "JOY write: Unhandled %s <- %04X", GBAIORegisterNames[address >> 1], value);
			handled = false;
			break;
		}
		break;
	case GBA_SIO_NORMAL_8:
		switch (address) {
		case GBA_REG_SIODATA8:
			mLOG(GBA_SIO, DEBUG, "NORMAL8 %i write: SIODATA8 <- %04X", id, value);
			break;
		case GBA_REG_JOYCNT:
			mLOG(GBA_SIO, DEBUG, "NORMAL8 %i write: JOYCNT (?) <- %04X", id, value);
			value = (value & 0x0040) | (sio->p->memory.io[GBA_REG(JOYCNT)] & ~(value & 0x7) & ~0x0040);
			break;
		default:
			mLOG(GBA_SIO, GAME_ERROR, "NORMAL8 %i write: Unhandled %s <- %04X", id, GBAIORegisterNames[address >> 1], value);
			handled = false;
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
		case GBA_REG_SIODATA8:
			mLOG(GBA_SIO, DEBUG, "NORMAL32 %i write: SIODATA8 (?) <- %04X", id, value);
			break;
		case GBA_REG_JOYCNT:
			mLOG(GBA_SIO, DEBUG, "NORMAL32 %i write: JOYCNT (?) <- %04X", id, value);
			value = (value & 0x0040) | (sio->p->memory.io[GBA_REG(JOYCNT)] & ~(value & 0x7) & ~0x0040);
			break;
		default:
			mLOG(GBA_SIO, GAME_ERROR, "NORMAL32 %i write: Unhandled %s <- %04X", id, GBAIORegisterNames[address >> 1], value);
			handled = false;
			break;
		}
		break;
	case GBA_SIO_MULTI:
		switch (address) {
		case GBA_REG_SIOMLT_SEND:
			mLOG(GBA_SIO, DEBUG, "MULTI %i write: SIOMLT_SEND <- %04X", id, value);
			break;
		case GBA_REG_JOYCNT:
			mLOG(GBA_SIO, DEBUG, "MULTI %i write: JOYCNT (?) <- %04X", id, value);
			value = (value & 0x0040) | (sio->p->memory.io[GBA_REG(JOYCNT)] & ~(value & 0x7) & ~0x0040);
			break;
		default:
			mLOG(GBA_SIO, GAME_ERROR, "MULTI %i write: Unhandled %s <- %04X", id, GBAIORegisterNames[address >> 1], value);
			handled = false;
			break;
		}
		break;
	case GBA_SIO_UART:
		switch (address) {
		case GBA_REG_SIODATA8:
			mLOG(GBA_SIO, DEBUG, "UART write: SIODATA8 <- %04X", value);
			break;
		case GBA_REG_JOYCNT:
			mLOG(GBA_SIO, DEBUG, "UART write: JOYCNT (?) <- %04X", value);
			value = (value & 0x0040) | (sio->p->memory.io[GBA_REG(JOYCNT)] & ~(value & 0x7) & ~0x0040);
			break;
		default:
			mLOG(GBA_SIO, GAME_ERROR, "UART write: Unhandled %s <- %04X", GBAIORegisterNames[address >> 1], value);
			handled = false;
			break;
		}
		break;
	case GBA_SIO_GPIO:
		mLOG(GBA_SIO, STUB, "GPIO write: Unhandled %s <- %04X", GBAIORegisterNames[address >> 1], value);
		handled = false;
		break;
	}
	if (!handled) {
		value = sio->p->memory.io[address >> 1];
	}
	return value;
}

int32_t GBASIOTransferCycles(enum GBASIOMode mode, uint16_t siocnt, int connected) {
	if (connected < 0 || connected >= MAX_GBAS) {
		mLOG(GBA_SIO, ERROR, "Invalid device count %i", connected);
		return 0;
	}

	switch (mode) {
	case GBA_SIO_MULTI:
		return GBASIOCyclesPerTransfer[GBASIOMultiplayerGetBaud(siocnt)][connected];
	case GBA_SIO_NORMAL_8:
		return 8 * GBA_ARM7TDMI_FREQUENCY / ((GBASIONormalIsInternalSc(siocnt) ? 2048 : 256) * 1024);
	case GBA_SIO_NORMAL_32:
		return 32 * GBA_ARM7TDMI_FREQUENCY / ((GBASIONormalIsInternalSc(siocnt) ? 2048 : 256) * 1024);
	default:
		mLOG(GBA_SIO, STUB, "No cycle count implemented for mode %s", _modeName(mode));
		break;
	}
	return 0;
}

void GBASIOMultiplayerFinishTransfer(struct GBASIO* sio, uint16_t data[4], uint32_t cyclesLate) {
	int id = 0;
	if (sio->driver && sio->driver->deviceId) {
		id = sio->driver->deviceId(sio->driver);
	}
	sio->p->memory.io[GBA_REG(SIOMULTI0)] = data[0];
	sio->p->memory.io[GBA_REG(SIOMULTI1)] = data[1];
	sio->p->memory.io[GBA_REG(SIOMULTI2)] = data[2];
	sio->p->memory.io[GBA_REG(SIOMULTI3)] = data[3];

	sio->siocnt = GBASIOMultiplayerClearBusy(sio->siocnt);
	sio->siocnt = GBASIOMultiplayerSetId(sio->siocnt, id);

	sio->rcnt = GBASIORegisterRCNTFillSc(sio->rcnt);

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

static void _sioFinish(struct mTiming* timing, void* user, uint32_t cyclesLate) {
	UNUSED(timing);
	struct GBASIO* sio = user;
	union {
		uint16_t multi[4];
		uint8_t normal8;
		uint32_t normal32;
	} data = {0};
	switch (sio->mode) {
	case GBA_SIO_MULTI:
		if (sio->driver && sio->driver->finishMultiplayer) {
			sio->driver->finishMultiplayer(sio->driver, data.multi);
		}
		GBASIOMultiplayerFinishTransfer(sio, data.multi, cyclesLate);
		break;
	case GBA_SIO_NORMAL_8:
		if (sio->driver && sio->driver->finishNormal8) {
			data.normal8 = sio->driver->finishNormal8(sio->driver);
		}
		GBASIONormal8FinishTransfer(sio, data.normal8, cyclesLate);
		break;
	case GBA_SIO_NORMAL_32:
		if (sio->driver && sio->driver->finishNormal32) {
			data.normal32 = sio->driver->finishNormal32(sio->driver);
		}
		GBASIONormal32FinishTransfer(sio, data.normal32, cyclesLate);
		break;
	default:
		// TODO
		mLOG(GBA_SIO, STUB, "No dummy finish implemented for mode %s", _modeName(sio->mode));
		break;
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
