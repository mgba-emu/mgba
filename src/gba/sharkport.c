/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "sharkport.h"

#include "gba/gba.h"
#include "util/vfs.h"

static const char* const SHARKPORT_HEADER = "SharkPortSave";

bool GBASavedataImportSharkPort(struct GBA* gba, struct VFile* vf, bool testChecksum) {
	union {
		char c[0x1C];
		int32_t i;
	} buffer;
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return false;
	}
	int32_t size;
	LOAD_32(size, 0, &buffer.i);
	if (size != (int32_t) strlen(SHARKPORT_HEADER)) {
		return false;
	}
	if (vf->read(vf, buffer.c, size) < size) {
		return false;
	}
	if (memcmp(SHARKPORT_HEADER, buffer.c, size) != 0) {
		return false;
	}
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, &buffer.i);
	if (size != 0x000F0000) {
		// What is this value?
		return false;
	}

	// Skip first three fields
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, &buffer.i);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return false;
	}

	if (vf->read(vf, &buffer.i, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, &buffer.i);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return false;
	}

	if (vf->read(vf, &buffer.i, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, &buffer.i);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return false;
	}

	// Read payload
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return false;
	}
	LOAD_32(size, 0, &buffer.i);
	if (size < 0x1C || size > SIZE_CART_FLASH1M + 0x1C) {
		return false;
	}
	char* payload = malloc(size);
	if (vf->read(vf, payload, size) < size) {
		goto cleanup;
	}

	struct GBACartridge* cart = (struct GBACartridge*) gba->memory.rom;
	memcpy(buffer.c, &cart->title, 16);
	buffer.c[0x10] = 0;
	buffer.c[0x11] = 0;
	buffer.c[0x12] = cart->checksum;
	buffer.c[0x13] = cart->maker;
	buffer.c[0x14] = 1;
	buffer.c[0x15] = 0;
	buffer.c[0x16] = 0;
	buffer.c[0x17] = 0;
	buffer.c[0x18] = 0;
	buffer.c[0x19] = 0;
	buffer.c[0x1A] = 0;
	buffer.c[0x1B] = 0;
	if (memcmp(buffer.c, payload, 0x1C) != 0) {
		goto cleanup;
	}

	uint32_t checksum;
	if (vf->read(vf, &buffer.i, 4) < 4) {
		goto cleanup;
	}
	LOAD_32(checksum, 0, &buffer.i);

	if (testChecksum) {
		uint32_t calcChecksum = 0;
		int i;
		for (i = 0; i < size; ++i) {
			calcChecksum += ((int32_t) payload[i]) << (calcChecksum % 24);
		}

		if (calcChecksum != checksum) {
			goto cleanup;
		}
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

bool GBASavedataExportSharkPort(const struct GBA* gba, struct VFile* vf) {
	union {
		char c[0x1C];
		int32_t i;
	} buffer;
	int32_t size = strlen(SHARKPORT_HEADER);
	STORE_32(size, 0, &buffer.i);
	if (vf->write(vf, &buffer.i, 4) < 4) {
		return false;
	}
	if (vf->write(vf, SHARKPORT_HEADER, size) < size) {
		return false;
	}

	size = 0x000F0000;
	STORE_32(size, 0, &buffer.i);
	if (vf->write(vf, &buffer.i, 4) < 4) {
		return false;
	}

	const struct GBACartridge* cart = (const struct GBACartridge*) gba->memory.rom;
	size = sizeof(cart->title);
	STORE_32(size, 0, &buffer.i);
	if (vf->write(vf, &buffer.i, 4) < 4) {
		return false;
	}
	if (vf->write(vf, cart->title, size) < 4) {
		return false;
	}

	time_t t = time(0);
	struct tm* tm = localtime(&t);
	size = strftime(&buffer.c[4], sizeof(buffer.c) - 4, "%m/%d/%Y %I:%M:%S %p", tm);
	STORE_32(size, 0, &buffer.i);
	if (vf->write(vf, buffer.c, size + 4) < size + 4) {
		return false;
	}

	// Last field is blank
	size = 0;
	STORE_32(size, 0, &buffer.i);
	if (vf->write(vf, &buffer.i, 4) < 4) {
		return false;
	}

	// Write payload
	size = 0x1C;
	switch (gba->memory.savedata.type) {
	case SAVEDATA_SRAM:
		size += SIZE_CART_SRAM;
		break;
	case SAVEDATA_FLASH512:
		size += SIZE_CART_FLASH512;
		break;
	case SAVEDATA_FLASH1M:
		size += SIZE_CART_FLASH1M;
		break;
	case SAVEDATA_EEPROM:
		size += SIZE_CART_EEPROM;
		break;
	case SAVEDATA_FORCE_NONE:
	case SAVEDATA_AUTODETECT:
		return false;
	}
	STORE_32(size, 0, &buffer.i);
	if (vf->write(vf, &buffer.i, 4) < 4) {
		return false;
	}
	size -= 0x1C;

	memcpy(buffer.c, &cart->title, 16);
	buffer.c[0x10] = 0;
	buffer.c[0x11] = 0;
	buffer.c[0x12] = cart->checksum;
	buffer.c[0x13] = cart->maker;
	buffer.c[0x14] = 1;
	buffer.c[0x15] = 0;
	buffer.c[0x16] = 0;
	buffer.c[0x17] = 0;
	buffer.c[0x18] = 0;
	buffer.c[0x19] = 0;
	buffer.c[0x1A] = 0;
	buffer.c[0x1B] = 0;
	if (vf->write(vf, buffer.c, 0x1C) < 0x1C) {
		return false;
	}

	uint32_t checksum = 0;
	int i;
	for (i = 0; i < 0x1C; ++i) {
		checksum += buffer.c[i] << (checksum % 24);
	}

	if (vf->write(vf, gba->memory.savedata.data, size) < size) {
		return false;
	}

	for (i = 0; i < size; ++i) {
		checksum += ((char) gba->memory.savedata.data[i]) << (checksum % 24);
	}

	STORE_32(checksum, 0, &buffer.i);
	if (vf->write(vf, &buffer.i, 4) < 4) {
		return false;
	}

	return true;
}
