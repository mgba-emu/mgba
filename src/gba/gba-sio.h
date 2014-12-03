/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef GBA_SIO_H
#define GBA_SIO_H

#include "util/common.h"

enum GBASIOMode {
	SIO_NORMAL_8 = 0,
	SIO_NORMAL_32 = 1,
	SIO_MULTI = 2,
	SIO_UART = 3,
	SIO_GPIO = 8,
	SIO_JOYBUS = 12
};

enum {
	RCNT_INITIAL = 0x8000
};

struct GBASIO;

struct GBASIODriver {
	struct GBASIO* p;

	int (*init)(struct GBASIODriver* driver);
	void (*deinit)(struct GBASIODriver* driver);
	int (*load)(struct GBASIODriver* driver);
	int (*unload)(struct GBASIODriver* driver);
	int (*writeRegister)(struct GBASIODriver* driver, uint32_t address, uint16_t value);
	int32_t (*processEvents)(struct GBASIODriver* driver, int32_t cycles);
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
	union {
		struct {
			unsigned sc : 1;
			unsigned internalSc : 1;
			unsigned si : 1;
			unsigned idleSo : 1;
			unsigned : 4;
			unsigned start : 1;
			unsigned : 3;
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

void GBASIOSetDriverSet(struct GBASIO* sio, struct GBASIODriverSet* drivers);
void GBASIOSetDriver(struct GBASIO* sio, struct GBASIODriver* driver, enum GBASIOMode mode);

void GBASIOWriteRCNT(struct GBASIO* sio, uint16_t value);
void GBASIOWriteSIOCNT(struct GBASIO* sio, uint16_t value);
void GBASIOWriteSIOMLT_SEND(struct GBASIO* sio, uint16_t value);

int32_t GBASIOProcessEvents(struct GBASIO* sio, int32_t cycles);

#endif
