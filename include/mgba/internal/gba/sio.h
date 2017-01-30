/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SIO_H
#define GBA_SIO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/gba/interface.h>
#include <mgba/core/log.h>

#define MAX_GBAS 4

extern const int GBASIOCyclesPerTransfer[4][MAX_GBAS];

mLOG_DECLARE_CATEGORY(GBA_SIO);

enum {
	RCNT_INITIAL = 0x8000
};

enum {
	JOY_CMD_RESET = 0xFF,
	JOY_CMD_POLL = 0x00,
	JOY_CMD_TRANS = 0x14,
	JOY_CMD_RECV = 0x15,

	JOYSTAT_TRANS_BIT = 8,
	JOYSTAT_RECV_BIT = 2,
};

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
	// TODO: Convert to bitfields
	union {
		struct {
			unsigned sc : 1;
			unsigned internalSc : 1;
			unsigned si : 1;
			unsigned idleSo : 1;
			unsigned : 3;
			unsigned start : 1;
			unsigned : 4;
			unsigned length : 1;
			unsigned : 1;
			unsigned irq : 1;
			unsigned : 1;
		} normalControl;

		struct {
			unsigned baud : 2;
			unsigned slave : 1;
			unsigned ready : 1;
			unsigned id : 2;
			unsigned error : 1;
			unsigned busy : 1;
			unsigned : 6;
			unsigned irq : 1;
			unsigned : 1;
		} multiplayerControl;

		uint16_t siocnt;
	};
};

void GBASIOInit(struct GBASIO* sio);
void GBASIODeinit(struct GBASIO* sio);
void GBASIOReset(struct GBASIO* sio);

void GBASIOSetDriverSet(struct GBASIO* sio, struct GBASIODriverSet* drivers);
void GBASIOSetDriver(struct GBASIO* sio, struct GBASIODriver* driver, enum GBASIOMode mode);

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value);
void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value);
uint16_t GBASIOWriteRegister(struct GBASIO* sio, uint32_t address, uint16_t value);

CXX_GUARD_END

#endif
