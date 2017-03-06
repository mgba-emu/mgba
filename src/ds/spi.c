/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/spi.h>

#include <mgba/internal/ds/ds.h>
#include <mgba-util/vfs.h>

mLOG_DEFINE_CATEGORY(DS_SPI, "DS SPI", "ds.spi");

static void _tscEvent(struct mTiming*, void* context, uint32_t cyclesLate);
static void _firmwareEvent(struct mTiming* timing, void* context, uint32_t cyclesLate);

void DSSPIReset(struct DS* ds) {
	memset(&ds->memory.spiBus, 0, sizeof(ds->memory.spiBus));
	ds->memory.spiBus.event.name = "DS SPI Event";
	ds->memory.spiBus.event.context = ds;
	ds->memory.spiBus.event.callback = _tscEvent;
	ds->memory.spiBus.event.priority = 0x60;
}

DSSPICNT DSSPIWriteControl(struct DS* ds, uint16_t control) {
	// TODO
	if (!ds->memory.spiBus.holdEnabled) {
		ds->memory.spiBus.tscControlByte = 0;
		ds->memory.spiBus.firmCommand = 0;
	}
	ds->memory.spiBus.holdEnabled = DSSPICNTIsCSHold(control);
	return control;
}

void DSSPIWrite(struct DS* ds, uint8_t datum) {
	DSSPICNT control = ds->memory.io7[DS7_REG_SPICNT >> 1];
	if (!DSSPICNTIsEnable(control)) {
		return;
	}
	uint32_t baud = 19 - DSSPICNTGetBaud(control);
	baud = DS_ARM7TDMI_FREQUENCY >> baud;
	switch (DSSPICNTGetChipSelect(control)) {
	case DS_SPI_DEV_TSC:
		ds->memory.spiBus.event.callback = _tscEvent;
		break;
	case DS_SPI_DEV_FIRMWARE:
		ds->memory.spiBus.event.callback = _firmwareEvent;
		break;
	case DS_SPI_DEV_POWERMAN:
	default:
		mLOG(DS_SPI, STUB, "Unimplemented data write: %04X:%02X", control, datum);
		break;
	}
	control = DSSPICNTFillBusy(control);
	mTimingDeschedule(&ds->ds7.timing, &ds->memory.spiBus.event);
	mTimingSchedule(&ds->ds7.timing, &ds->memory.spiBus.event, baud);
	ds->memory.io7[DS7_REG_SPICNT >> 1] = control;
}

static void _tscEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct DS* ds = context;
	uint8_t oldValue = ds->memory.io7[DS7_REG_SPIDATA >> 1];
	DSSPICNT control = ds->memory.io7[DS7_REG_SPICNT >> 1];
	uint8_t newValue = 0;

	if (ds->memory.spiBus.tscOffset > 0) {
		// TODO: Make generic?
		if (ds->memory.spiBus.tscOffset < 12) {
			newValue = (ds->memory.spiBus.tscRegister & 0x1F) << 3;
			ds->memory.spiBus.tscOffset = 12;
		} else {
			newValue = 0;
		}
	} else if (ds->memory.spiBus.tscControlByte) {
		switch (DSTSCControlByteGetChannel(ds->memory.spiBus.tscControlByte)) {
		// TODO: Calibrate from firmware
		case DS_TSC_CHANNEL_TS_X:
			if (*ds->touchSource) {
				ds->memory.spiBus.tscRegister = (*ds->cursorSourceX * 0xDD0 / DS_VIDEO_HORIZONTAL_PIXELS) + 0x100;
			} else {
				ds->memory.spiBus.tscRegister = 0;
			}
			break;
		case DS_TSC_CHANNEL_TS_Y:
			if (*ds->touchSource) {
				ds->memory.spiBus.tscRegister = (*ds->cursorSourceY * 0xE70 / DS_VIDEO_VERTICAL_PIXELS) + 0x0B0;
			} else {
				ds->memory.spiBus.tscRegister = 0xFFF;
			}
			break;
		case DS_TSC_CHANNEL_TEMP_0:
			if (*ds->touchSource) {
				ds->memory.io7[DS7_REG_EXTKEYIN >> 1] &= ~0x040;
			} else {
				ds->memory.io7[DS7_REG_EXTKEYIN >> 1] |= 0x040;
			}
			break;
		case DS_TSC_CHANNEL_BATTERY_V:
		case DS_TSC_CHANNEL_TS_Z1:
		case DS_TSC_CHANNEL_TS_Z2:
		case DS_TSC_CHANNEL_MIC:
		case DS_TSC_CHANNEL_TEMP_1:
			ds->memory.spiBus.tscRegister = 0;
			mLOG(DS_SPI, STUB, "Unimplemented TSC channel %i", DSTSCControlByteGetChannel(ds->memory.spiBus.tscControlByte));
		}
		newValue = (ds->memory.spiBus.tscRegister >> 5) & 0x7F;
		ds->memory.spiBus.tscOffset = 7;
	}

	if (DSTSCControlByteIsControl(oldValue)) {
		ds->memory.spiBus.tscControlByte = oldValue;
		ds->memory.spiBus.tscOffset = 0;
	}

	control = DSSPICNTClearBusy(control);
	ds->memory.io7[DS7_REG_SPIDATA >> 1] = newValue;
	ds->memory.io7[DS7_REG_SPICNT >> 1] = control;
	if (DSSPICNTIsDoIRQ(control)) {
		DSRaiseIRQ(ds->ds7.cpu, ds->ds7.memory.io, DS_IRQ_SPI);
	}
}

static void _firmwareEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct DS* ds = context;
	uint8_t oldValue = ds->memory.io7[DS7_REG_SPIDATA >> 1];
	DSSPICNT control = ds->memory.io7[DS7_REG_SPICNT >> 1];
	uint8_t newValue = 0;

	if (!ds->memory.spiBus.firmCommand) {
		ds->memory.spiBus.firmCommand = oldValue;
		ds->memory.spiBus.firmAddress = 0;
		ds->memory.spiBus.firmAddressingRemaining = 24;
	} else if (ds->memory.spiBus.firmAddressingRemaining) {
		ds->memory.spiBus.firmAddress <<= 8;
		ds->memory.spiBus.firmAddress |= oldValue;
		ds->memory.spiBus.firmAddressingRemaining -= 8;
		ds->firmwareVf->seek(ds->firmwareVf, ds->memory.spiBus.firmAddress, SEEK_SET);
	} else {
		switch (ds->memory.spiBus.firmCommand) {
		case 0x02: // WR
			ds->firmwareVf->write(ds->firmwareVf, &oldValue, 1);
			++ds->memory.spiBus.firmAddress;
			break;
		case 0x03: // RD
			ds->firmwareVf->read(ds->firmwareVf, &newValue, 1);
			++ds->memory.spiBus.firmAddress;
		case 0x04: // WRDI
			ds->memory.spiBus.firmStatusReg &= ~2;
			break;
		case 0x05: // RDSR
			newValue = ds->memory.spiBus.firmStatusReg;
			break;
		case 0x06: // WREN
			ds->memory.spiBus.firmStatusReg |= 2;
			break;
		default:
			mLOG(DS_SPI, STUB, "Unimplemented Firmware write: %04X:%02X:%02X", control, ds->memory.spiBus.firmCommand, newValue);
			break;
		}
	}

	control = DSSPICNTClearBusy(control);
	ds->memory.io7[DS7_REG_SPIDATA >> 1] = newValue;
	ds->memory.io7[DS7_REG_SPICNT >> 1] = control;
	if (DSSPICNTIsDoIRQ(control)) {
		DSRaiseIRQ(ds->ds7.cpu, ds->ds7.memory.io, DS_IRQ_SPI);
	}
}
