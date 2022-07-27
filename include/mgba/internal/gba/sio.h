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

extern const int GBASIOCyclesPerTransfer[4][MAX_GBAS];

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

struct GBASIODriverSet {
	struct GBASIODriver* normal;
	struct GBASIODriver* multiplayer;
	struct GBASIODriver* joybus;
};

struct GBASIO {
	struct GBA* p;

	enum GBASIOMode mode;
	struct GBASIODriverSet drivers;
	struct GBASIODriver* activeDriver;

	uint16_t rcnt;
	uint16_t siocnt;

	struct GBASIOPlayer gbp;
};

void GBASIOInit(struct GBASIO* sio);
void GBASIODeinit(struct GBASIO* sio);
void GBASIOReset(struct GBASIO* sio);

void GBASIOSetDriverSet(struct GBASIO* sio, struct GBASIODriverSet* drivers);
void GBASIOSetDriver(struct GBASIO* sio, struct GBASIODriver* driver, enum GBASIOMode mode);

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value);
void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value);
uint16_t GBASIOWriteRegister(struct GBASIO* sio, uint32_t address, uint16_t value);

int GBASIOJOYSendCommand(struct GBASIODriver* sio, enum GBASIOJOYCommand command, uint8_t* data);

CXX_GUARD_END

#endif
