/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/matrix.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/memory.h>
#include <mgba-util/vfs.h>

static void _remapMatrix(struct GBA* gba) {
	gba->romVf->seek(gba->romVf, gba->memory.matrix.paddr, SEEK_SET);
	gba->romVf->read(gba->romVf, &gba->memory.rom[gba->memory.matrix.vaddr >> 2], gba->memory.matrix.size);
}

void GBAMatrixReset(struct GBA* gba) {
	gba->memory.matrix.paddr = 0x200;
	gba->memory.matrix.size = 0x1000;

	gba->memory.matrix.vaddr = 0;
	_remapMatrix(gba);
	gba->memory.matrix.vaddr = 0x1000;
	_remapMatrix(gba);

	gba->memory.matrix.paddr = 0;
	gba->memory.matrix.vaddr = 0;
	gba->memory.matrix.size = 0x100;
	_remapMatrix(gba);
}

void GBAMatrixWrite(struct GBA* gba, uint32_t address, uint32_t value) {
	switch (address) {
	case 0x0:
		gba->memory.matrix.cmd = value;
		switch (value) {
		case 0x01:
		case 0x11:
			_remapMatrix(gba);
			break;
		default:
			mLOG(GBA_MEM, STUB, "Unknown Matrix command: %08X", value);
			break;
		}
		return;
	case 0x4:
		gba->memory.matrix.paddr = value & 0x03FFFFFF;
		return;
	case 0x8:
		gba->memory.matrix.vaddr = value & 0x007FFFFF;
		return;
	case 0xC:
		gba->memory.matrix.size = value << 9;
		return;
	}
	mLOG(GBA_MEM, STUB, "Unknown Matrix write: %08X:%04X", address, value);
}

void GBAMatrixWrite16(struct GBA* gba, uint32_t address, uint16_t value) {
	switch (address) {
	case 0x0:
		GBAMatrixWrite(gba, address, value | (gba->memory.matrix.cmd & 0xFFFF0000));
		break;
	case 0x4:
		GBAMatrixWrite(gba, address, value | (gba->memory.matrix.paddr & 0xFFFF0000));
		break;
	case 0x8:
		GBAMatrixWrite(gba, address, value | (gba->memory.matrix.vaddr & 0xFFFF0000));
		break;
	case 0xC:
		GBAMatrixWrite(gba, address, value | (gba->memory.matrix.size & 0xFFFF0000));
		break;
	}
}
