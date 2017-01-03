/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/io.h>

#include <mgba/internal/ds/ds.h>

mLOG_DEFINE_CATEGORY(DS_IO, "DS I/O");

static void _writeIPCSync(struct ARMCore* remoteCpu, uint16_t* remoteIo, int16_t value) {
	remoteIo[DS7_REG_IPCSYNC >> 1] &= 0xFFF0;
	remoteIo[DS7_REG_IPCSYNC >> 1] |= (value >> 8) & 0x0F;
	if (value & 0x2000 && remoteIo[DS7_REG_IPCSYNC >> 1] & 0x4000) {
		mLOG(DS_IO, STUB, "Unimplemented IPC IRQ");
		UNUSED(remoteCpu);
	}
}

void DS7IOInit(struct DS* ds) {
	memset(ds->memory.io7, 0, sizeof(ds->memory.io7));
}

void DS7IOWrite(struct DS* ds, uint32_t address, uint16_t value) {
	switch (address) {
	// Timers
	case DS7_REG_TM0CNT_LO:
		DSTimerWriteTMCNT_LO(&ds->timers7[0], value);
		return;
	case DS7_REG_TM1CNT_LO:
		DSTimerWriteTMCNT_LO(&ds->timers7[1], value);
		return;
	case DS7_REG_TM2CNT_LO:
		DSTimerWriteTMCNT_LO(&ds->timers7[2], value);
		return;
	case DS7_REG_TM3CNT_LO:
		DSTimerWriteTMCNT_LO(&ds->timers7[3], value);
		return;

	case DS7_REG_TM0CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&ds->timers7[0], ds->arm7, &ds->memory.io7[(address - 2) >> 1], value);
		ds->timersEnabled7 &= ~1;
		ds->timersEnabled7 |= DSTimerFlagsGetEnable(ds->timers7[0].flags);
		break;
	case DS7_REG_TM1CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&ds->timers7[1], ds->arm7, &ds->memory.io7[(address - 2) >> 1], value);
		ds->timersEnabled7 &= ~2;
		ds->timersEnabled7 |= DSTimerFlagsGetEnable(ds->timers7[1].flags) << 1;
		break;
	case DS7_REG_TM2CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&ds->timers7[2], ds->arm7, &ds->memory.io7[(address - 2) >> 1], value);
		ds->timersEnabled7 &= ~4;
		ds->timersEnabled7 |= DSTimerFlagsGetEnable(ds->timers7[2].flags) << 2;
		break;
	case DS7_REG_TM3CNT_HI:
		value &= 0x00C7;
		DSTimerWriteTMCNT_HI(&ds->timers7[3], ds->arm7, &ds->memory.io7[(address - 2) >> 1], value);
		ds->timersEnabled7 &= ~8;
		ds->timersEnabled7 |= DSTimerFlagsGetEnable(ds->timers7[3].flags) << 3;
		break;

	case DS7_REG_IPCSYNC:
		value &= 0x6F00;
		value |= ds->memory.io7[address >> 1] & 0x000F;
		_writeIPCSync(ds->arm9, ds->memory.io9, value);
		break;
	case DS7_REG_IME:
		DSWriteIME(ds->arm7, ds->memory.io7, value);
		break;
	case DS7_REG_IF_LO:
	case DS7_REG_IF_HI:
		value = ds->memory.io7[address >> 1] & ~value;
		break;
	default:
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
		value16 |= (ds->memory.io7[(address & 0xFFF) >> 1]) & ~(0xFF << (8 * (address & 1)));
		DS7IOWrite(ds, address & 0xFFFFFFFE, value16);
	} else {
		mLOG(DS, STUB, "Writing to unknown DS7 register: %08X:%02X", address, value);
	}
}

void DS7IOWrite32(struct DS* ds, uint32_t address, uint32_t value) {
	switch (address) {
	case DS7_REG_IE_LO:
		DSWriteIE(ds->arm7, ds->memory.io7, value);
		break;
	default:
		DS7IOWrite(ds, address, value & 0xFFFF);
		DS7IOWrite(ds, address | 2, value >> 16);
		return;
	}
	ds->memory.io7[address >> 1] = value;
	ds->memory.io7[(address >> 1) + 1] = value >> 16;
}

uint16_t DS7IORead(struct DS* ds, uint32_t address) {
	switch (address) {
	case DS7_REG_TM0CNT_LO:
		DSTimerUpdateRegister(&ds->timers7[0], ds->arm7, &ds->memory.io7[address >> 1]);
		break;
	case DS7_REG_TM1CNT_LO:
		DSTimerUpdateRegister(&ds->timers7[1], ds->arm7, &ds->memory.io7[address >> 1]);
		break;
	case DS7_REG_TM2CNT_LO:
		DSTimerUpdateRegister(&ds->timers7[2], ds->arm7, &ds->memory.io7[address >> 1]);
		break;
	case DS7_REG_TM3CNT_LO:
		DSTimerUpdateRegister(&ds->timers7[3], ds->arm7, &ds->memory.io7[address >> 1]);
		break;

	case DS7_REG_TM0CNT_HI:
	case DS7_REG_TM1CNT_HI:
	case DS7_REG_TM2CNT_HI:
	case DS7_REG_TM3CNT_HI:
	case DS7_REG_IPCSYNC:
	case DS7_REG_IME:
	case DS7_REG_IE_LO:
	case DS7_REG_IE_HI:
	case DS7_REG_IF_LO:
	case DS7_REG_IF_HI:
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
	case DS9_REG_IPCSYNC:
		value &= 0x6F00;
		value |= ds->memory.io9[address >> 1] & 0x000F;
		_writeIPCSync(ds->arm7, ds->memory.io7, value);
		break;
	default:
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
	default:
		DS9IOWrite(ds, address, value & 0xFFFF);
		DS9IOWrite(ds, address | 2, value >> 16);
		return;
	}
	ds->memory.io9[address >> 1] = value;
	ds->memory.io9[(address >> 1) + 1] = value >> 16;
}

uint16_t DS9IORead(struct DS* ds, uint32_t address) {
	switch (address) {
	case DS9_REG_IPCSYNC:
		// Handled transparently by the registers
		break;
	default:
		mLOG(DS_IO, STUB, "Stub DS9 I/O register read: %06X", address);
	}
	if (address < DS9_REG_MAX) {
		return ds->memory.io9[address >> 1];
	}
	return 0;
}
