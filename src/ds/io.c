/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/io.h>

#include <mgba/internal/ds/ds.h>
#include <mgba/internal/ds/ipc.h>

mLOG_DEFINE_CATEGORY(DS_IO, "DS I/O");

static bool DSIOWrite(struct DSCommon* dscore, uint32_t address, uint16_t value) {
	switch (address) {
	// Timers
	case DS_REG_TM0CNT_LO:
		GBATimerWriteTMCNT_LO(&dscore->timers[0], value);
		return true;
	case DS_REG_TM1CNT_LO:
		GBATimerWriteTMCNT_LO(&dscore->timers[1], value);
		return true;
	case DS_REG_TM2CNT_LO:
		GBATimerWriteTMCNT_LO(&dscore->timers[2], value);
		return true;
	case DS_REG_TM3CNT_LO:
		GBATimerWriteTMCNT_LO(&dscore->timers[3], value);
		return true;

	case DS_REG_TM0CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&dscore->timers[0], &dscore->timing, dscore->cpu, &dscore->memory.io[DS_REG_TM0CNT_LO >> 1], value);
		break;
	case DS_REG_TM1CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&dscore->timers[1], &dscore->timing, dscore->cpu, &dscore->memory.io[DS_REG_TM1CNT_LO >> 1], value);
		break;
	case DS_REG_TM2CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&dscore->timers[2], &dscore->timing, dscore->cpu, &dscore->memory.io[DS_REG_TM2CNT_LO >> 1], value);
		break;
	case DS_REG_TM3CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&dscore->timers[3], &dscore->timing, dscore->cpu, &dscore->memory.io[DS_REG_TM3CNT_LO >> 1], value);
		break;

	case DS_REG_IPCSYNC:
		value &= 0x6F00;
		value |= dscore->memory.io[address >> 1] & 0x000F;
		DSIPCWriteSYNC(dscore->ipc->cpu, dscore->ipc->memory.io, value);
		break;
	case DS_REG_IPCFIFOCNT:
		value = DSIPCWriteFIFOCNT(dscore, value);
		break;
	case DS_REG_IME:
		DSWriteIME(dscore->cpu, dscore->memory.io, value);
		break;
	case DS_REG_IF_LO:
	case DS_REG_IF_HI:
		value = dscore->memory.io[address >> 1] & ~value;
		break;
	default:
		return false;
	}
	return true;
}

static void DSIOUpdateTimer(struct DSCommon* dscore, uint32_t address) {
	switch (address) {
	case DS_REG_TM0CNT_LO:
		GBATimerUpdateRegisterInternal(&dscore->timers[0], &dscore->timing, dscore->cpu, &dscore->memory.io[address >> 1], 0);
		break;
	case DS_REG_TM1CNT_LO:
		GBATimerUpdateRegisterInternal(&dscore->timers[1], &dscore->timing, dscore->cpu, &dscore->memory.io[address >> 1], 0);
		break;
	case DS_REG_TM2CNT_LO:
		GBATimerUpdateRegisterInternal(&dscore->timers[2], &dscore->timing, dscore->cpu, &dscore->memory.io[address >> 1], 0);
		break;
	case DS_REG_TM3CNT_LO:
		GBATimerUpdateRegisterInternal(&dscore->timers[3], &dscore->timing, dscore->cpu, &dscore->memory.io[address >> 1], 0);
		break;
	}
}

void DS7IOInit(struct DS* ds) {
	memset(ds->memory.io7, 0, sizeof(ds->memory.io7));
}

void DS7IOWrite(struct DS* ds, uint32_t address, uint16_t value) {
	switch (address) {
	default:
		if (DSIOWrite(&ds->ds7, address, value)) {
			break;
		}
		mLOG(DS_IO, STUB, "Stub DS7 I/O register write: %06X:%04X", address, value);
		if (address >= DS7_REG_MAX) {
			mLOG(DS_IO, GAME_ERROR, "Write to unused DS7 I/O register: %06X:%04X", address, value);
			return;
		}
		break;
	}
	ds->memory.io7[address >> 1] = value;
}

void DS7IOWrite8(struct DS* ds, uint32_t address, uint8_t value) {
	if (address < DS7_REG_MAX) {
		uint16_t value16 = value << (8 * (address & 1));
		value16 |= (ds->ds7.memory.io[(address & 0xFFF) >> 1]) & ~(0xFF << (8 * (address & 1)));
		DS7IOWrite(ds, address & 0xFFFFFFFE, value16);
	} else {
		mLOG(DS, STUB, "Writing to unknown DS7 register: %08X:%02X", address, value);
	}
}

void DS7IOWrite32(struct DS* ds, uint32_t address, uint32_t value) {
	switch (address) {
	case DS_REG_IPCFIFOSEND_LO:
		DSIPCWriteFIFO(&ds->ds9, value);
		break;
	case DS_REG_IE_LO:
		DSWriteIE(ds->ds7.cpu, ds->ds7.memory.io, value);
		break;
	default:
		DS7IOWrite(ds, address, value & 0xFFFF);
		DS7IOWrite(ds, address | 2, value >> 16);
		return;
	}
	ds->ds7.memory.io[address >> 1] = value;
	ds->ds7.memory.io[(address >> 1) + 1] = value >> 16;
}

uint16_t DS7IORead(struct DS* ds, uint32_t address) {
	switch (address) {
	case DS_REG_TM0CNT_LO:
	case DS_REG_TM1CNT_LO:
	case DS_REG_TM2CNT_LO:
	case DS_REG_TM3CNT_LO:
		DSIOUpdateTimer(&ds->ds7, address);
		break;
	case DS_REG_TM0CNT_HI:
	case DS_REG_TM1CNT_HI:
	case DS_REG_TM2CNT_HI:
	case DS_REG_TM3CNT_HI:
	case DS_REG_IPCSYNC:
	case DS_REG_IME:
	case DS_REG_IE_LO:
	case DS_REG_IE_HI:
	case DS_REG_IF_LO:
	case DS_REG_IF_HI:
		// Handled transparently by the registers
		break;
	default:
		mLOG(DS_IO, STUB, "Stub DS7 I/O register read: %06X", address);
	}
	if (address < DS7_REG_MAX) {
		return ds->memory.io7[address >> 1];
	}
	return 0;
}

void DS9IOInit(struct DS* ds) {
	memset(ds->memory.io9, 0, sizeof(ds->memory.io9));
}

void DS9IOWrite(struct DS* ds, uint32_t address, uint16_t value) {
	switch (address) {
	default:
		if (DSIOWrite(&ds->ds9, address, value)) {
			break;
		}
		mLOG(DS_IO, STUB, "Stub DS9 I/O register write: %06X:%04X", address, value);
		if (address >= DS7_REG_MAX) {
			mLOG(DS_IO, GAME_ERROR, "Write to unused DS9 I/O register: %06X:%04X", address, value);
			return;
		}
		break;
	}
	ds->memory.io9[address >> 1] = value;
}

void DS9IOWrite8(struct DS* ds, uint32_t address, uint8_t value) {
	if (address < DS9_REG_MAX) {
		uint16_t value16 = value << (8 * (address & 1));
		value16 |= (ds->memory.io9[(address & 0x1FFF) >> 1]) & ~(0xFF << (8 * (address & 1)));
		DS9IOWrite(ds, address & 0xFFFFFFFE, value16);
	} else {
		mLOG(DS, STUB, "Writing to unknown DS9 register: %08X:%02X", address, value);
	}
}

void DS9IOWrite32(struct DS* ds, uint32_t address, uint32_t value) {
	switch (address) {
	case DS_REG_IPCFIFOSEND_LO:
		DSIPCWriteFIFO(&ds->ds9, value);
		break;
	case DS_REG_IE_LO:
		DSWriteIE(ds->ds9.cpu, ds->ds9.memory.io, value);
		break;
	default:
		DS9IOWrite(ds, address, value & 0xFFFF);
		DS9IOWrite(ds, address | 2, value >> 16);
		return;
	}
	ds->ds9.memory.io[address >> 1] = value;
	ds->ds9.memory.io[(address >> 1) + 1] = value >> 16;
}

uint16_t DS9IORead(struct DS* ds, uint32_t address) {
	switch (address) {
	case DS_REG_TM0CNT_LO:
	case DS_REG_TM1CNT_LO:
	case DS_REG_TM2CNT_LO:
	case DS_REG_TM3CNT_LO:
		DSIOUpdateTimer(&ds->ds9, address);
		break;
	case DS_REG_TM0CNT_HI:
	case DS_REG_TM1CNT_HI:
	case DS_REG_TM2CNT_HI:
	case DS_REG_TM3CNT_HI:
	case DS_REG_IPCSYNC:
	case DS_REG_IME:
	case DS_REG_IE_LO:
	case DS_REG_IE_HI:
	case DS_REG_IF_LO:
	case DS_REG_IF_HI:
		// Handled transparently by the registers
		break;
	default:
		mLOG(DS_IO, STUB, "Stub DS9 I/O register read: %06X", address);
	}
	if (address < DS9_REG_MAX) {
		return ds->ds9.memory.io[address >> 1];
	}
	return 0;
}
