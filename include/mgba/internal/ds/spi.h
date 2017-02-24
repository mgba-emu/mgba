/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef DS_SPI_IO_H
#define DS_SPI_IO_H

#include <mgba-util/common.h>

CXX_GUARD_START

#include <mgba/core/log.h>
#include <mgba/core/timing.h>

mLOG_DECLARE_CATEGORY(DS_SPI);

DECL_BITFIELD(DSSPICNT, uint16_t);
DECL_BITS(DSSPICNT, Baud, 0, 2);
DECL_BIT(DSSPICNT, Busy, 7);
DECL_BITS(DSSPICNT, ChipSelect, 8, 2);
DECL_BIT(DSSPICNT, TransferSize, 10);
DECL_BIT(DSSPICNT, CSHold, 11);
DECL_BIT(DSSPICNT, DoIRQ, 14);
DECL_BIT(DSSPICNT, Enable, 15);

DECL_BITFIELD(DSTSCControlByte, uint8_t);
// TODO
DECL_BITS(DSTSCControlByte, Channel, 4, 3);
DECL_BIT(DSTSCControlByte, Control, 7);

enum {
	DS_SPI_DEV_POWERMAN = 0,
	DS_SPI_DEV_FIRMWARE = 1,
	DS_SPI_DEV_TSC = 2
};

enum {
	DS_TSC_CHANNEL_TEMP_0 = 0,
	DS_TSC_CHANNEL_TS_Y = 1,
	DS_TSC_CHANNEL_BATTERY_V = 2,
	DS_TSC_CHANNEL_TS_Z1 = 3,
	DS_TSC_CHANNEL_TS_Z2 = 4,
	DS_TSC_CHANNEL_TS_X = 5,
	DS_TSC_CHANNEL_MIC = 6,
	DS_TSC_CHANNEL_TEMP_1 = 7,
};

struct DSSPIBus {
	bool holdEnabled;

	uint8_t firmCommand;
	uint8_t firmStatusReg;
	int firmAddressingRemaining;
	uint32_t firmAddress;

	uint8_t tscControlByte;
	uint16_t tscRegister;
	int tscOffset;

	uint8_t powmgrByte;

	struct mTimingEvent event;
};

struct DS;
void DSSPIReset(struct DS* ds);
DSSPICNT DSSPIWriteControl(struct DS* ds, uint16_t control);
void DSSPIWrite(struct DS* ds, uint8_t datum);

CXX_GUARD_END

#endif
