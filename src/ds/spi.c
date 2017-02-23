/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/spi.h>

#include <mgba/internal/ds/ds.h>

mLOG_DEFINE_CATEGORY(DS_SPI, "DS SPI");

static void _tscEvent(struct mTiming*, void* context, uint32_t cyclesLate);

void DSSPIReset(struct DS* ds) {
	memset(&ds->memory.spiBus, 0, sizeof(ds->memory.spiBus));
	ds->memory.spiBus.tscEvent.name = "DS SPI TSC";
	ds->memory.spiBus.tscEvent.context = ds;
	ds->memory.spiBus.tscEvent.callback = _tscEvent;
	ds->memory.spiBus.tscEvent.priority = 0x60;
}

DSSPICNT DSSPIWriteControl(struct DS* ds, uint16_t control) {
	// TODO
	if (!ds->memory.spiBus.holdEnabled) {
		ds->memory.spiBus.tscControlByte = 0;
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
		control = DSSPICNTFillBusy(control);
		mTimingDeschedule(&ds->ds7.timing, &ds->memory.spiBus.tscEvent);
		mTimingSchedule(&ds->ds7.timing, &ds->memory.spiBus.tscEvent, baud);
		break;
	case DS_SPI_DEV_POWERMAN:
	case DS_SPI_DEV_FIRMWARE:
	default:
		mLOG(DS_SPI, STUB, "Unimplemented data write: %04X:%02X", control, datum);
		break;
	}
	ds->memory.io7[DS7_REG_SPICNT >> 1] = control;
}

static void _tscEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(cyclesLate);
	struct DS* ds = context;
	uint8_t oldValue = ds->memory.io7[DS7_REG_SPIDATA >> 1];
	DSSPICNT control = ds->memory.io7[DS7_REG_SPICNT >> 1];
	uint8_t newValue = 0;

	// TODO: /PENIRQ
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
		case DS_TSC_CHANNEL_TS_X:
			mLOG(DS_SPI, STUB, "Unimplemented TSC channel X");
			ds->memory.spiBus.tscRegister = 0;
			break;
		case DS_TSC_CHANNEL_TS_Y:
			mLOG(DS_SPI, STUB, "Unimplemented TSC channel Y");
			ds->memory.spiBus.tscRegister = 0xFFF;
			break;
		case DS_TSC_CHANNEL_TEMP_0:
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
