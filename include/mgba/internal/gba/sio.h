/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SIO_H
#define GBA_SIO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/sio/gbp.h>

#define MAX_GBAS 4

mLOG_DECLARE_CATEGORY(GBA_SIO);

enum {
	RCNT_INITIAL = -0x8000
};

enum {
	JOY_CMD_RESET = 0xFF,
	JOY_CMD_POLL = 0x00,
	JOY_CMD_TRANS = 0x14,
	JOY_CMD_RECV = 0x15,

	JOYSTAT_TRANS = 8,
	JOYSTAT_RECV = 2,

	JOYCNT_RESET = 1,
	JOYCNT_RECV = 2,
	JOYCNT_TRANS = 4,
};

DECL_BITFIELD(GBASIONormal, uint16_t);
DECL_BIT(GBASIONormal, Sc, 0);
DECL_BIT(GBASIONormal, InternalSc, 1);
DECL_BIT(GBASIONormal, Si, 2);
DECL_BIT(GBASIONormal, IdleSo, 3);
DECL_BIT(GBASIONormal, Start, 7);
DECL_BIT(GBASIONormal, Length, 12);
DECL_BIT(GBASIONormal, Irq, 14);
DECL_BITFIELD(GBASIOMultiplayer, uint16_t);
DECL_BITS(GBASIOMultiplayer, Baud, 0, 2);
DECL_BIT(GBASIOMultiplayer, Slave, 2);
DECL_BIT(GBASIOMultiplayer, Ready, 3);
DECL_BITS(GBASIOMultiplayer, Id, 4, 2);
DECL_BIT(GBASIOMultiplayer, Error, 6);
DECL_BIT(GBASIOMultiplayer, Busy, 7);
DECL_BIT(GBASIOMultiplayer, Irq, 14);
DECL_BITFIELD(GBASIORegisterRCNT, uint16_t);
DECL_BIT(GBASIORegisterRCNT, Sc, 0);
DECL_BIT(GBASIORegisterRCNT, Sd, 1);
DECL_BIT(GBASIORegisterRCNT, Si, 2);
DECL_BIT(GBASIORegisterRCNT, So, 3);
DECL_BIT(GBASIORegisterRCNT, ScDirection, 4);
DECL_BIT(GBASIORegisterRCNT, SdDirection, 5);
DECL_BIT(GBASIORegisterRCNT, SiDirection, 6);
DECL_BIT(GBASIORegisterRCNT, SoDirection, 7);

struct GBASIO {
	struct GBA* p;

	enum GBASIOMode mode;
	struct GBASIODriver* driver;

	uint16_t rcnt;
	uint16_t siocnt;

	struct GBASIOPlayer gbp;
	struct mTimingEvent completeEvent;
};

void GBASIOInit(struct GBASIO* sio);
void GBASIODeinit(struct GBASIO* sio);
void GBASIOReset(struct GBASIO* sio);

void GBASIOSetDriver(struct GBASIO* sio, struct GBASIODriver* driver);

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value);
void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value);
uint16_t GBASIOWriteRegister(struct GBASIO* sio, uint32_t address, uint16_t value);

int32_t GBASIOTransferCycles(enum GBASIOMode mode, uint16_t siocnt, int connected);

void GBASIOMultiplayerFinishTransfer(struct GBASIO* sio, uint16_t data[4], uint32_t cyclesLate);
void GBASIONormal8FinishTransfer(struct GBASIO* sio, uint8_t data, uint32_t cyclesLate);
void GBASIONormal32FinishTransfer(struct GBASIO* sio, uint32_t data, uint32_t cyclesLate);

int GBASIOJOYSendCommand(struct GBASIODriver* sio, enum GBASIOJOYCommand command, uint8_t* data);

CXX_GUARD_END

#endif
