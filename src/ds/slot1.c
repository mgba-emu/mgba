/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/slot1.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/dma.h>
#include <mgba-util/math.h>
#include <mgba-util/vfs.h>

mLOG_DEFINE_CATEGORY(DS_SLOT1, "DS Slot-1", "ds.slot1");

static void _slot1SPI(struct mTiming*, void* context, uint32_t cyclesLate);
static void _transferEvent(struct mTiming* timing, void* context, uint32_t cyclesLate);
static bool _slot1GuaranteeSize(struct DSSlot1*);

void DSSlot1SPIInit(struct DS* ds, struct VFile* vf) {
	ds->memory.slot1.spiEvent.name = "DS Slot-1 SPI";
	ds->memory.slot1.spiEvent.priority = 0x70;
	ds->memory.slot1.spiEvent.context = NULL;
	ds->memory.slot1.spiEvent.callback = _slot1SPI;
	ds->memory.slot1.transferEvent.name = "DS Slot-1 Transfer";
	ds->memory.slot1.transferEvent.priority = 0x71;
	ds->memory.slot1.transferEvent.context = ds;
	ds->memory.slot1.transferEvent.callback = _transferEvent;
	ds->memory.slot1.savedataType = DS_SAVEDATA_AUTODETECT;
	ds->memory.slot1.spiVf = vf;
	ds->memory.slot1.spiRealVf = vf;
	ds->memory.slot1.spiData = NULL;
	ds->memory.slot1.spiSize = 0;
}

void DSSlot1Reset(struct DS* ds) {
	ds->memory.slot1.statusReg = 0;
	ds->memory.slot1.spiCommand = 0;
	ds->memory.slot1.spiHoldEnabled = 0;
	ds->memory.slot1.dmaSource = -1;
	ds->memory.slot1.spiAddressingBits = 16;
}

static void _scheduleTransfer(struct DS* ds, struct mTiming* timing, uint32_t bytes, uint32_t cyclesLate) {
	DSSlot1ROMCNT romcnt = ds->memory.io7[DS_REG_ROMCNT_HI >> 1] << 16;
	uint32_t cycles;
	if (DSSlot1ROMCNTIsTransferRate(romcnt)) {
		cycles = 8;
	} else {
		cycles = 5;
	}
	if (!ds->ds7.memory.slot1Access) {
		cycles <<= 1;
	}
	cycles *= bytes / 4;
	cycles -= cyclesLate;
	mTimingDeschedule(timing, &ds->memory.slot1.transferEvent);
	mTimingSchedule(timing, &ds->memory.slot1.transferEvent, cycles);
}

static void _transferEvent(struct mTiming* timing, void* context, uint32_t cyclesLate) {
	struct DS* ds = context;
	DSSlot1ROMCNT romcnt;
	// TODO: Big endian
	LOAD_32(romcnt, DS_REG_ROMCNT_LO, ds->memory.io7);

	struct DSCommon* dscore;
	if (ds->ds7.memory.slot1Access) {
		dscore = &ds->ds7;
	} else {
		dscore = &ds->ds9;
	}

	struct GBADMA* dma = NULL;
	if (ds->memory.slot1.dmaSource >= 0) {
		dma = &dscore->memory.dma[ds->memory.slot1.dmaSource];
	}
	bool hasDMA = false;
	if (dma) {
		if (ds->ds7.memory.slot1Access && GBADMARegisterGetTiming(dma->reg) == DS7_DMA_TIMING_SLOT1) {
			hasDMA = true;
		}
		if (ds->ds9.memory.slot1Access && GBADMARegisterGetTiming9(dma->reg) == DS9_DMA_TIMING_SLOT1) {
			hasDMA = true;
		}
		if (!GBADMARegisterIsEnable(dma->reg)) {
			hasDMA = false;
		}
	}
	if (!hasDMA) {
		ds->memory.slot1.dmaSource = -1;
	}

	if (ds->memory.slot1.transferRemaining) {
		ds->romVf->read(ds->romVf, ds->memory.slot1.readBuffer, 4);
		// TODO: Error check
		ds->memory.slot1.address += 4;
		ds->memory.slot1.transferRemaining -= 4;
		romcnt = DSSlot1ROMCNTFillWordReady(romcnt);

		if (hasDMA) {
			dma->when = mTimingCurrentTime(timing);
			dma->nextCount = 1;
			DSDMAUpdate(dscore);
		}
	} else {
		DSSlot1AUXSPICNT config = ds->memory.io7[DS_REG_AUXSPICNT >> 1];
		memset(ds->memory.slot1.readBuffer, 0, 4);
		romcnt = DSSlot1ROMCNTClearWordReady(romcnt);
		romcnt = DSSlot1ROMCNTClearBlockBusy(romcnt);
		if (DSSlot1AUXSPICNTIsDoIRQ(config)) {
			DSRaiseIRQ(dscore->cpu, dscore->memory.io, DS_IRQ_SLOT1_TRANS);
		}
		if (hasDMA && !GBADMARegisterIsRepeat(dma->reg)) {
			dma->reg = GBADMARegisterClearEnable(dma->reg);
			dscore->memory.io[(DS_REG_DMA0CNT_HI + ds->memory.slot1.dmaSource * (DS_REG_DMA1CNT_HI - DS_REG_DMA0CNT_HI)) >> 1] = dma->reg;
		}
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
		if (ds->memory.slot1.address < 0x8000) {
			mLOG(DS_SLOT1, GAME_ERROR, "Invalid read from secure area: %04X", ds->memory.slot1.address);
			ds->memory.slot1.address = 0x8000 + (ds->memory.slot1.address & 0x1FF);
		}
		if (ds->romVf) {
			ds->romVf->seek(ds->romVf, ds->memory.slot1.address, SEEK_SET);
		}
		ds->memory.slot1.transferRemaining = ds->memory.slot1.transferSize;
		if (ds->ds7.memory.slot1Access) {
			_scheduleTransfer(ds, &ds->ds7.timing, 12, 0);
		} else {
			_scheduleTransfer(ds, &ds->ds9.timing, 12, 0);
		}
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
		if (ds->memory.slot1.command[0] != 0xB7) {
			control = DSSlot1ROMCNTFillWordReady(control);
		}
	}
	return control;
}

uint32_t DSSlot1Read(struct DS* ds) {
	uint32_t result;
	LOAD_32(result, 0, ds->memory.slot1.readBuffer);
	if (ds->ds7.memory.slot1Access) {
		_scheduleTransfer(ds, &ds->ds7.timing, 4, 0);
	} else {
		_scheduleTransfer(ds, &ds->ds9.timing, 4, 0);
	}
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
	} else if (dscore->p->isHomebrew) {
		if (!_slot1GuaranteeSize(&dscore->p->memory.slot1)) {
			return 0xFF;
		}
	}
	if (!dscore->p->memory.slot1.spiData) {
		return 0xFF;
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

static uint8_t _slot1SPIEEPROM(struct DSCommon* dscore, uint8_t datum) {
	DSSlot1AUXSPICNT control = dscore->memory.io[DS_REG_AUXSPICNT >> 1];

	if (dscore->p->memory.slot1.spiAddressingRemaining) {
		dscore->p->memory.slot1.spiAddress <<= 8;
		dscore->p->memory.slot1.spiAddress |= datum;
		dscore->p->memory.slot1.spiAddressingRemaining -= 8;
		return 0xFF;
	} else if (dscore->p->isHomebrew) {
		if (!_slot1GuaranteeSize(&dscore->p->memory.slot1)) {
			return 0xFF;
		}
	}
	if (!dscore->p->memory.slot1.spiData) {
		return 0xFF;
	}

	uint8_t oldValue;
	switch (dscore->p->memory.slot1.spiCommand) {
	case 0x03: // RDLO
	case 0x0B: // RDHI
		dscore->p->memory.slot1.spiAddress &= dscore->p->memory.slot1.spiSize - 1;
		oldValue = dscore->p->memory.slot1.spiData[dscore->p->memory.slot1.spiAddress];
		++dscore->p->memory.slot1.spiAddress;
		return oldValue;
	case 0x02: // WRLO
	case 0x0A: // WRHI
		dscore->p->memory.slot1.spiAddress &= dscore->p->memory.slot1.spiSize - 1;
		dscore->p->memory.slot1.spiData[dscore->p->memory.slot1.spiAddress] = datum;
		++dscore->p->memory.slot1.spiAddress;
		break;
	}
	return 0xFF;
}

static uint8_t _slot1SPIFlash(struct DSCommon* dscore, uint8_t datum) {
	DSSlot1AUXSPICNT control = dscore->memory.io[DS_REG_AUXSPICNT >> 1];

	if (dscore->p->memory.slot1.spiAddressingRemaining) {
		dscore->p->memory.slot1.spiAddress <<= 8;
		dscore->p->memory.slot1.spiAddress |= datum;
		dscore->p->memory.slot1.spiAddressingRemaining -= 8;
		return 0xFF;
	} else if (dscore->p->isHomebrew) {
		if (!_slot1GuaranteeSize(&dscore->p->memory.slot1)) {
			return 0xFF;
		}
	}
	if (!dscore->p->memory.slot1.spiData) {
		return 0xFF;
	}

	uint8_t oldValue;
	switch (dscore->p->memory.slot1.spiCommand) {
	case 0x03: // RD
		dscore->p->memory.slot1.spiAddress &= dscore->p->memory.slot1.spiSize - 1;
		oldValue = dscore->p->memory.slot1.spiData[dscore->p->memory.slot1.spiAddress];
		++dscore->p->memory.slot1.spiAddress;
		return oldValue;
	case 0x02: // PP
		dscore->p->memory.slot1.spiAddress &= dscore->p->memory.slot1.spiSize - 1;
		dscore->p->memory.slot1.spiData[dscore->p->memory.slot1.spiAddress] = datum;
		++dscore->p->memory.slot1.spiAddress;
		break;
	case 0x0A: // PW
		dscore->p->memory.slot1.spiAddress &= dscore->p->memory.slot1.spiSize - 1;
		oldValue = dscore->p->memory.slot1.spiData[dscore->p->memory.slot1.spiAddress];
		dscore->p->memory.slot1.spiData[dscore->p->memory.slot1.spiAddress] = datum;
		++dscore->p->memory.slot1.spiAddress;
		return oldValue;
	default:
		mLOG(DS_SLOT1, STUB, "Unimplemented SPI Flash write: %04X:%02X:%02X", control, dscore->p->memory.slot1.spiCommand, datum);
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
		if ((oldValue & 0x08) && dscore->p->memory.slot1.savedataType == DS_SAVEDATA_EEPROM512) {
			dscore->p->memory.slot1.spiAddress = 1;
		} else {
			dscore->p->memory.slot1.spiAddress = 0;
		}
		dscore->p->memory.slot1.spiAddressingRemaining = dscore->p->memory.slot1.spiAddressingBits;
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
			case DS_SAVEDATA_FLASH:
				newValue = _slot1SPIFlash(dscore, oldValue);
				break;
			case DS_SAVEDATA_EEPROM:
			case DS_SAVEDATA_EEPROM512:
				newValue = _slot1SPIEEPROM(dscore, oldValue);
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
		slot1->spiSize = size;
		size_t oldSize = slot1->spiVf->size(slot1->spiVf);
		if (slot1->spiData) {
			slot1->spiVf->unmap(slot1->spiVf, slot1->spiData, oldSize);
			slot1->spiData = NULL;
		}
		slot1->spiVf->truncate(slot1->spiVf, size);
		slot1->spiVf->seek(slot1->spiVf, oldSize, SEEK_SET);
		while (oldSize < size) {
			static char buffer[1024];
			memset(buffer, 0xFF, sizeof(buffer));
			ssize_t written;
			if (oldSize + sizeof(buffer) <= size) {
				written = slot1->spiVf->write(slot1->spiVf, buffer, sizeof(buffer));
			} else {
				written = slot1->spiVf->write(slot1->spiVf, buffer, size - oldSize);
			}
			if (written >= 0) {
				oldSize += written;
			} else {
				break;
			}
		}
	}
	if (!slot1->spiData) {
		slot1->spiData = slot1->spiVf->map(slot1->spiVf, slot1->spiVf->size(slot1->spiVf), MAP_WRITE);
	}
	return slot1->spiData;
}

void DSSlot1ConfigureSPI(struct DS* ds, uint32_t paramPtr) {
	struct ARMCore* cpu = ds->ds7.cpu;
	uint32_t saveParams = cpu->memory.load32(cpu, paramPtr + 4, NULL);
	uint32_t size = 1 << ((saveParams & 0xFF00) >> 8);
	if ((saveParams & 0xFF) == 2) {
		ds->memory.slot1.savedataType = DS_SAVEDATA_FLASH;
	} else {
		ds->memory.slot1.savedataType = DS_SAVEDATA_EEPROM;
	}
	if (size > 0x10000) {
		ds->memory.slot1.spiAddressingBits = 24;
	} else if (size <= 0x200) {
		ds->memory.slot1.spiAddressingBits = 8;
		ds->memory.slot1.savedataType = DS_SAVEDATA_EEPROM512;
	} else {
		ds->memory.slot1.spiAddressingBits = 16;
	}
	ds->memory.slot1.spiAddress = size - 1;
	ds->memory.slot1.spiSize = size;
	_slot1GuaranteeSize(&ds->memory.slot1);
}

void DSSlot1ScheduleDMA(struct DSCommon* dscore, int number, struct GBADMA* info) {
	UNUSED(info);
	dscore->p->memory.slot1.dmaSource = number;
}
