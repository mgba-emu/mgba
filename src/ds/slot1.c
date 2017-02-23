/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/slot1.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/ds/ds.h>
#include <mgba-util/math.h>
#include <mgba-util/vfs.h>

mLOG_DEFINE_CATEGORY(DS_SLOT1, "DS Slot-1");

static void _slot1SPI(struct mTiming*, void* context, uint32_t cyclesLate);
static bool _slot1GuaranteeSize(struct DSSlot1*);

void DSSlot1SPIInit(struct DS* ds, struct VFile* vf) {
	ds->memory.slot1.spiEvent.name = "DS Slot-1 SPI";
	ds->memory.slot1.spiEvent.priority = 0x70;
	ds->memory.slot1.spiEvent.context = NULL;
	ds->memory.slot1.spiEvent.callback = _slot1SPI;
	ds->memory.slot1.savedataType = DS_SAVEDATA_AUTODETECT;
	ds->memory.slot1.spiVf = vf;
	ds->memory.slot1.spiRealVf = vf;
	ds->memory.slot1.spiData = NULL;
}

void DSSlot1Reset(struct DS* ds) {
	ds->memory.slot1.statusReg = 0;
	ds->memory.slot1.spiCommand = 0;
	ds->memory.slot1.spiHoldEnabled = 0;
}

static void DSSlot1StepTransfer(struct DS* ds) {
	DSSlot1ROMCNT romcnt;
	// TODO: Big endian
	LOAD_32(romcnt, DS_REG_ROMCNT_LO, ds->memory.io7);
	if (ds->memory.slot1.transferRemaining) {
		ds->romVf->read(ds->romVf, ds->memory.slot1.readBuffer, 4);
		// TODO: Error check
		ds->memory.slot1.address += 4;
		ds->memory.slot1.transferRemaining -= 4;
		romcnt = DSSlot1ROMCNTFillWordReady(romcnt);
	} else {
		memset(ds->memory.slot1.readBuffer, 0, 4);
		romcnt = DSSlot1ROMCNTClearWordReady(romcnt);
		// TODO: IRQ
		romcnt = DSSlot1ROMCNTClearBlockBusy(romcnt);
	}
	STORE_32(romcnt, DS_REG_ROMCNT_LO, ds->memory.io7);
	STORE_32(romcnt, DS_REG_ROMCNT_LO, ds->memory.io9);
}

static void DSSlot1StartTransfer(struct DS* ds) {
	size_t i;
	for (i = 0; i < 8; i += 2) {
		uint16_t bytes;
		LOAD_16(bytes, DS_REG_ROMCMD_0 + i, ds->memory.io7);
		ds->memory.slot1.command[i] = bytes & 0xFF;
		ds->memory.slot1.command[i + 1] = bytes >> 8;
	}
	switch (ds->memory.slot1.command[0]) {
	case 0xB7:
		ds->memory.slot1.address = ds->memory.slot1.command[1] << 24;
		ds->memory.slot1.address |= ds->memory.slot1.command[2] << 16;
		ds->memory.slot1.address |= ds->memory.slot1.command[3] << 8;
		ds->memory.slot1.address |= ds->memory.slot1.command[4];
		if (ds->romVf) {
			ds->romVf->seek(ds->romVf, ds->memory.slot1.address, SEEK_SET);
		}
		ds->memory.slot1.transferRemaining = ds->memory.slot1.transferSize;
		DSSlot1StepTransfer(ds);
		break;
	case 0xB8:
		memcpy(ds->memory.slot1.readBuffer, DS_CHIP_ID, 4);
		ds->memory.slot1.transferRemaining = 0;
		break;
	default:
		mLOG(DS_SLOT1, STUB, "Unimplemented card command: %02X%02X%02X%02X%02X%02X%02X%02X",
		     ds->memory.slot1.command[0], ds->memory.slot1.command[1],
		     ds->memory.slot1.command[2], ds->memory.slot1.command[3],
		     ds->memory.slot1.command[4], ds->memory.slot1.command[5],
		     ds->memory.slot1.command[6], ds->memory.slot1.command[7]);
		break;
	}
}

DSSlot1AUXSPICNT DSSlot1Configure(struct DS* ds, DSSlot1AUXSPICNT config) {
	if (DSSlot1AUXSPICNTIsSPIMode(config)) {
		if (!ds->memory.slot1.spiHoldEnabled) {
			ds->memory.slot1.spiCommand = 0;
		}
		ds->memory.slot1.spiHoldEnabled = DSSlot1AUXSPICNTIsCSHold(config);
	}
	return config;
}

DSSlot1ROMCNT DSSlot1Control(struct DS* ds, DSSlot1ROMCNT control) {
	ds->memory.slot1.transferSize = DSSlot1ROMCNTGetBlockSize(control);
	if (ds->memory.slot1.transferSize != 0 && ds->memory.slot1.transferSize != 7) {
		ds->memory.slot1.transferSize = 0x100 << ds->memory.slot1.transferSize;
	}

	DSSlot1AUXSPICNT config = ds->memory.io7[DS_REG_AUXSPICNT >> 1];
	if (DSSlot1AUXSPICNTIsSPIMode(config)) {
		mLOG(DS_SLOT1, STUB, "Bad ROMCNT?");
		return control;
	}
	if (DSSlot1ROMCNTIsBlockBusy(control)) {
		DSSlot1StartTransfer(ds);
		// TODO: timing
		control = DSSlot1ROMCNTFillWordReady(control);
	}
	return control;
}

uint32_t DSSlot1Read(struct DS* ds) {
	uint32_t result;
	LOAD_32(result, 0, ds->memory.slot1.readBuffer);
	DSSlot1StepTransfer(ds);
	return result;
}

void DSSlot1WriteSPI(struct DSCommon* dscore, uint8_t datum) {
	UNUSED(datum);
	DSSlot1AUXSPICNT control = dscore->memory.io[DS_REG_AUXSPICNT >> 1];
	if (!DSSlot1AUXSPICNTIsSPIMode(control) || !DSSlot1AUXSPICNTIsEnable(control)) {
		return;
	}
	uint32_t baud = 19 - DSSlot1AUXSPICNTGetBaud(control);
	baud = DS_ARM7TDMI_FREQUENCY >> baud; // TODO: Right frequency for ARM9
	control = DSSlot1AUXSPICNTFillBusy(control);
	mTimingDeschedule(&dscore->timing, &dscore->p->memory.slot1.spiEvent);
	mTimingSchedule(&dscore->timing, &dscore->p->memory.slot1.spiEvent, baud);
	dscore->p->memory.slot1.spiEvent.context = dscore;
	dscore->memory.io[DS_REG_AUXSPICNT >> 1] = control;
	dscore->ipc->memory.io[DS_REG_AUXSPICNT >> 1] = control;
}

static uint8_t _slot1SPIAutodetect(struct DSCommon* dscore, uint8_t datum) {
	DSSlot1AUXSPICNT control = dscore->memory.io[DS_REG_AUXSPICNT >> 1];
	mLOG(DS_SLOT1, STUB, "Unimplemented SPI write: %04X:%02X:%02X", control, dscore->p->memory.slot1.spiCommand, datum);

	if (dscore->p->memory.slot1.spiAddressingRemaining) {
		dscore->p->memory.slot1.spiAddress <<= 8;
		dscore->p->memory.slot1.spiAddress |= datum;
		dscore->p->memory.slot1.spiAddressingRemaining -= 8;
		return 0xFF;
	} else {
		if (!_slot1GuaranteeSize(&dscore->p->memory.slot1)) {
			return 0xFF;
		}
	}

	switch (dscore->p->memory.slot1.spiCommand) {
	case 0x03: // RD
		return dscore->p->memory.slot1.spiData[dscore->p->memory.slot1.spiAddress++];
	case 0x02: // WR
		dscore->p->memory.slot1.spiData[dscore->p->memory.slot1.spiAddress] = datum;
		++dscore->p->memory.slot1.spiAddress;
		break;
	}
	return 0xFF;
}

static void _slot1SPI(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	UNUSED(timing);
	UNUSED(cyclesLate);
	struct DSCommon* dscore = context;
	DSSlot1AUXSPICNT control = dscore->memory.io[DS_REG_AUXSPICNT >> 1];
	uint8_t oldValue = dscore->memory.io[DS_REG_AUXSPIDATA >> 1];
	uint8_t newValue = 0xFF;

	if (!dscore->p->memory.slot1.spiCommand) {
		dscore->p->memory.slot1.spiCommand = oldValue;
		// Probably RDHI
		if (oldValue == 0x0B && dscore->p->memory.slot1.savedataType == DS_SAVEDATA_AUTODETECT) {
			dscore->p->memory.slot1.savedataType = DS_SAVEDATA_EEPROM512;
		}
		dscore->p->memory.slot1.spiAddress = 0;
		dscore->p->memory.slot1.spiAddressingRemaining = 16;
	} else {
		switch (dscore->p->memory.slot1.spiCommand) {
		case 0x04: // WRDI
			dscore->p->memory.slot1.statusReg &= ~2;
			break;
		case 0x05: // RDSR
			newValue = dscore->p->memory.slot1.statusReg;
			break;
		case 0x06: // WREN
			dscore->p->memory.slot1.statusReg |= 2;
			break;
		default:
			switch (dscore->p->memory.slot1.savedataType) {
			case DS_SAVEDATA_AUTODETECT:
				newValue = _slot1SPIAutodetect(dscore, oldValue);
				break;
			default:
				mLOG(DS_SLOT1, STUB, "Unimplemented SPI write: %04X:%02X", control, oldValue);
				break;
			}
		}
	}

	control = DSSlot1AUXSPICNTClearBusy(control);
	dscore->memory.io[DS_REG_AUXSPIDATA >> 1] = newValue;
	dscore->ipc->memory.io[DS_REG_AUXSPIDATA >> 1] = newValue;
	dscore->memory.io[DS_REG_AUXSPICNT >> 1] = control;
	dscore->ipc->memory.io[DS_REG_AUXSPICNT >> 1] = control;
}

static bool _slot1GuaranteeSize(struct DSSlot1* slot1) {
	if (!slot1->spiVf) {
		return false;
	}
	if (slot1->spiAddress >= slot1->spiVf->size(slot1->spiVf)) {
		size_t size = toPow2(slot1->spiAddress + 1);
		if (slot1->spiData) {
			slot1->spiVf->unmap(slot1->spiVf, slot1->spiData, slot1->spiVf->size(slot1->spiVf));
			slot1->spiData = NULL;
		}
		slot1->spiVf->truncate(slot1->spiVf, size);
		// TODO: Write FFs
	}
	if (!slot1->spiData) {
		slot1->spiData = slot1->spiVf->map(slot1->spiVf, slot1->spiVf->size(slot1->spiVf), MAP_WRITE);
	}
	return slot1->spiData;
}
