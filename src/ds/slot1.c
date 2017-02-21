/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/ds/slot1.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/ds/ds.h>
#include <mgba-util/vfs.h>

mLOG_DEFINE_CATEGORY(DS_SLOT1, "DS Slot-1");

static void DSSlot1StepTransfer(struct DS* ds) {
	DSSlot1ROMCNT romcnt;
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
	mLOG(DS_SLOT1, STUB, "Unimplemented SPI AUX config: %04X", config);
	return config;
}

DSSlot1ROMCNT DSSlot1Control(struct DS* ds, DSSlot1ROMCNT control) {
	ds->memory.slot1.transferSize = DSSlot1ROMCNTGetBlockSize(control);
	if (ds->memory.slot1.transferSize != 0 && ds->memory.slot1.transferSize != 7) {
		ds->memory.slot1.transferSize = 0x100 << ds->memory.slot1.transferSize;
	}
	if (DSSlot1ROMCNTIsBlockBusy(control)) {
		DSSlot1StartTransfer(ds);
		// TODO timing
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
