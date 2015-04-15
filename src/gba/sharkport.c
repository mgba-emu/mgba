/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sharkport.h"

#include "gba/gba.h"
#include "util/vfs.h"

static const char* const SHARKPORT_HEADER = "SharkPortSave";

bool GBASavedataImportSharkPort(struct GBA* gba, struct VFile* vf) {
	char buffer[0x1C];
	if (vf->read(vf, buffer, 4) < 4) {
		return false;
	}
	uint32_t size;
	LOAD_32(size, 0, buffer);
	if (size != strlen(SHARKPORT_HEADER)) {
		return false;
	}
	if (vf->read(vf, buffer, size) < size) {
		return false;
	}
	if (memcmp(SHARKPORT_HEADER, buffer, size) != 0) {
		return false;
	}
	if (vf->read(vf, buffer, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, buffer);
	if (size != 0x000F0000) {
		// What is this value?
		return false;
	}

	// Skip first three fields
	if (vf->read(vf, buffer, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, buffer);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return false;
	}

	if (vf->read(vf, buffer, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, buffer);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return false;
	}

	if (vf->read(vf, buffer, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, buffer);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return false;
	}

	// Read payload
	if (vf->read(vf, buffer, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, buffer);
	if (size < 0x1C || size > SIZE_CART_FLASH1M + 0x1C) {
		return false;
	}
	char* payload = malloc(size);
	if (vf->read(vf, payload, size) < size) {
		goto cleanup;
	}

	struct GBACartridge* cart = (struct GBACartridge*) gba->memory.rom;
	memcpy(buffer, cart->title, 16);
	buffer[0x10] = 0;
	buffer[0x11] = 0;
	buffer[0x12] = cart->checksum;
	buffer[0x13] = cart->maker;
	buffer[0x14] = 1;
	buffer[0x15] = 0;
	buffer[0x16] = 0;
	buffer[0x17] = 0;
	buffer[0x18] = 0;
	buffer[0x19] = 0;
	buffer[0x1A] = 0;
	buffer[0x1B] = 0;
	if (memcmp(buffer, payload, 0x1C) != 0) {
		goto cleanup;
	}

	uint32_t checksum;
	if (vf->read(vf, buffer, 4) < 4) {
		goto cleanup;
	}
	LOAD_32(checksum, 0, buffer);

	uint32_t calcChecksum = 0;
	uint32_t i;
	for (i = 0; i < size; ++i) {
		calcChecksum += payload[i] << (calcChecksum % 24);
	}

	if (calcChecksum != checksum) {
		goto cleanup;
	}

	uint32_t copySize = size - 0x1C;
	switch (gba->memory.savedata.type) {
	case SAVEDATA_SRAM:
		if (copySize > SIZE_CART_SRAM) {
			copySize = SIZE_CART_SRAM;
		}
		break;
	case SAVEDATA_FLASH512:
		if (copySize > SIZE_CART_FLASH512) {
			GBASavedataForceType(&gba->memory.savedata, SAVEDATA_FLASH1M, gba->memory.savedata.realisticTiming);
		}
		// Fall through
	case SAVEDATA_FLASH1M:
		if (copySize > SIZE_CART_FLASH1M) {
			copySize = SIZE_CART_FLASH1M;
		}
		break;
	case SAVEDATA_EEPROM:
		if (copySize > SIZE_CART_EEPROM) {
			copySize = SAVEDATA_EEPROM;
		}
		break;
	case SAVEDATA_FORCE_NONE:
	case SAVEDATA_AUTODETECT:
		goto cleanup;
	}

	memcpy(gba->memory.savedata.data, &payload[0x1C], copySize);

	free(payload);
	return true;

cleanup:
	free(payload);
	return false;
}
