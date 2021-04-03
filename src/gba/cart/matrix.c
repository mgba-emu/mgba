/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/cart/matrix.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/memory.h>
#include <mgba/internal/gba/serialize.h>
#include <mgba-util/vfs.h>

#define MAPPING_MASK (GBA_MATRIX_MAPPINGS_MAX - 1)

static void _remapMatrix(struct GBA* gba) {
	if (gba->memory.matrix.vaddr & 0xFFFFE1FF) {
		mLOG(GBA_MEM, ERROR, "Invalid Matrix mapping: %08X", gba->memory.matrix.vaddr);
		return;
	}
	if (gba->memory.matrix.size & 0xFFFFE1FF) {
		mLOG(GBA_MEM, ERROR, "Invalid Matrix size: %08X", gba->memory.matrix.size);
		return;
	}
	if ((gba->memory.matrix.vaddr + gba->memory.matrix.size - 1) & 0xFFFFE000) {
		mLOG(GBA_MEM, ERROR, "Invalid Matrix mapping end: %08X", gba->memory.matrix.vaddr + gba->memory.matrix.size);
		return;
	}
	int start = gba->memory.matrix.vaddr >> 9;
	int size = (gba->memory.matrix.size >> 9) & MAPPING_MASK;
	int i;
	for (i = 0; i < size; ++i) {
		gba->memory.matrix.mappings[(start + i) & MAPPING_MASK] = gba->memory.matrix.paddr + (i << 9);
	}

	gba->romVf->seek(gba->romVf, gba->memory.matrix.paddr, SEEK_SET);
	gba->romVf->read(gba->romVf, &gba->memory.rom[gba->memory.matrix.vaddr >> 2], gba->memory.matrix.size);
}

void GBAMatrixReset(struct GBA* gba) {
	memset(gba->memory.matrix.mappings, 0, sizeof(gba->memory.matrix.mappings));
	gba->memory.matrix.size = 0x1000;

	gba->memory.matrix.paddr = 0;
	gba->memory.matrix.vaddr = 0;
	_remapMatrix(gba);
	gba->memory.matrix.paddr = 0x200;
	gba->memory.matrix.vaddr = 0x1000;
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
		if (value == 0) {
			mLOG(GBA_MEM, ERROR, "Rejecting Matrix write for size 0");
			return;
		}
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

void GBAMatrixSerialize(const struct GBA* gba, struct GBASerializedState* state) {
	STORE_32(gba->memory.matrix.cmd, 0, &state->matrix.cmd);
	STORE_32(gba->memory.matrix.paddr, 0, &state->matrix.paddr);
	STORE_32(gba->memory.matrix.vaddr, 0, &state->matrix.vaddr);
	STORE_32(gba->memory.matrix.size, 0, &state->matrix.size);

	if (GBA_MATRIX_MAPPINGS_MAX != 16) {
		mLOG(GBA_MEM, ERROR, "Matrix memory serialization is broken!");
	}

	int i;
	for (i = 0; i < 16; ++i) {
		STORE_32(gba->memory.matrix.mappings[i], i << 2, state->matrixMappings);
	}
}

void GBAMatrixDeserialize(struct GBA* gba, const struct GBASerializedState* state) {
	if (GBA_MATRIX_MAPPINGS_MAX != 16) {
		mLOG(GBA_MEM, ERROR, "Matrix memory deserialization is broken!");
	}
	gba->memory.matrix.size = 0x200;

	int i;
	for (i = 0; i < 16; ++i) {
		LOAD_32(gba->memory.matrix.mappings[i], i << 2, state->matrixMappings);
		gba->memory.matrix.paddr = gba->memory.matrix.mappings[i];
		gba->memory.matrix.vaddr = i << 9;
		_remapMatrix(gba);
	}

	LOAD_32(gba->memory.matrix.cmd, 0, &state->matrix.cmd);
	LOAD_32(gba->memory.matrix.paddr, 0, &state->matrix.paddr);
	LOAD_32(gba->memory.matrix.vaddr, 0, &state->matrix.vaddr);
	LOAD_32(gba->memory.matrix.size, 0, &state->matrix.size);
}
