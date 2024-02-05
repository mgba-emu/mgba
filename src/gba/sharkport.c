/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gba/sharkport.h>

#include <mgba/internal/arm/macros.h>
#include <mgba/internal/gba/gba.h>
#include <mgba-util/vfs.h>

static const char* const SHARKPORT_HEADER = "SharkPortSave";
static const char* const GSV_HEADER = "ADVSAVEG";
static const char* const GSV_FOOTER = "xV4\x12";
static const int GSV_IDENT_OFFSET = 0xC;
static const int GSV_PAYLOAD_OFFSET = 0x430;

static bool _importSavedata(struct GBA* gba, void* payload, size_t size) {
	bool success = false;
	switch (gba->memory.savedata.type) {
	case SAVEDATA_FLASH512:
		if (size > GBA_SIZE_FLASH512) {
			GBASavedataForceType(&gba->memory.savedata, SAVEDATA_FLASH1M);
		}
	// Fall through
	default:
		if (size > GBASavedataSize(&gba->memory.savedata)) {
			size = GBASavedataSize(&gba->memory.savedata);
		}
		break;
	case SAVEDATA_FORCE_NONE:
	case SAVEDATA_AUTODETECT:
		goto cleanup;
	}

	if (size == GBA_SIZE_EEPROM || size == GBA_SIZE_EEPROM512) {
		size_t i;
		for (i = 0; i < size; i += 8) {
			uint32_t lo, hi;
			LOAD_32BE(lo, i, payload);
			LOAD_32BE(hi, i + 4, payload);
			STORE_32LE(hi, i, gba->memory.savedata.data);
			STORE_32LE(lo, i + 4, gba->memory.savedata.data);
		}
	} else {
		memcpy(gba->memory.savedata.data, payload, size);
	}
	if (gba->memory.savedata.vf) {
		gba->memory.savedata.vf->sync(gba->memory.savedata.vf, gba->memory.savedata.data, size);
	}
	success = true;

cleanup:
	free(payload);
	return success;
}

int GBASavedataSharkPortPayloadSize(struct VFile* vf) {
	union {
		char c[0x1C];
		int32_t i;
	} buffer;
	vf->seek(vf, 0, SEEK_SET);
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return 0;
	}
	int32_t size;
	LOAD_32(size, 0, &buffer.i);
	if (size != (int32_t) strlen(SHARKPORT_HEADER)) {
		return 0;
	}
	if (vf->read(vf, buffer.c, size) < size) {
		return 0;
	}
	if (memcmp(SHARKPORT_HEADER, buffer.c, size) != 0) {
		return 0;
	}
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return 0;
	}
	LOAD_32(size, 0, &buffer.i);
	if (size != 0x000F0000) {
		// What is this value?
		return 0;
	}

	// Skip first three fields
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return 0;
	}
	LOAD_32(size, 0, &buffer.i);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return 0;
	}

	if (vf->read(vf, &buffer.i, 4) < 4) {
		return 0;
	}
	LOAD_32(size, 0, &buffer.i);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return 0;
	}

	if (vf->read(vf, &buffer.i, 4) < 4) {
		return 0;
	}
	LOAD_32(size, 0, &buffer.i);
	if (vf->seek(vf, size, SEEK_CUR) < 0) {
		return 0;
	}

	// Read payload
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return 0;
	}
	LOAD_32(size, 0, &buffer.i);
	return size;
}

void* GBASavedataSharkPortGetPayload(struct VFile* vf, size_t* osize, uint8_t* oheader, bool testChecksum) {
	int32_t size = GBASavedataSharkPortPayloadSize(vf);
	if (size < 0x1C || size > GBA_SIZE_FLASH1M + 0x1C) {
		return NULL;
	}
	size -= 0x1C;
	uint8_t header[0x1C];
	int8_t* payload = malloc(size);

	if (vf->read(vf, header, sizeof(header)) < (int) sizeof(header)) {
		goto cleanup;
	}
	if (vf->read(vf, payload, size) < size) {
		goto cleanup;
	}

	uint32_t buffer;
	uint32_t checksum;
	if (vf->read(vf, &buffer, 4) < 4) {
		goto cleanup;
	}
	LOAD_32(checksum, 0, &buffer);

	if (testChecksum) {
		uint32_t calcChecksum = 0;
		int i;
		for (i = 0; i < (int) sizeof(header); ++i) {
			calcChecksum += ((int32_t) header[i]) << (calcChecksum % 24);
		}
		for (i = 0; i < size; ++i) {
			calcChecksum += ((int32_t) payload[i]) << (calcChecksum % 24);
		}

		if (calcChecksum != checksum) {
			return NULL;
		}
	}
	*osize = size;
	if (oheader) {
		memcpy(oheader, header, sizeof(header));
	}
	return payload;

cleanup:
	free(payload);
	return NULL;
}

bool GBASavedataImportSharkPort(struct GBA* gba, struct VFile* vf, bool testChecksum) {
	uint8_t buffer[0x1C];
	uint8_t header[0x1C];

	size_t size;
	void* payload = GBASavedataSharkPortGetPayload(vf, &size, header, testChecksum);
	if (!payload) {
		return false;
	}

	struct GBACartridge* cart = (struct GBACartridge*) gba->memory.rom;
	memcpy(buffer, &cart->title, 16);
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
	if (memcmp(buffer, header, testChecksum ? 0x1C : 0xF) != 0) {
		free(payload);
		return false;
	}

	return _importSavedata(gba, payload, size);
}

bool GBASavedataExportSharkPort(const struct GBA* gba, struct VFile* vf) {
	union {
		char c[0x1C];
		int32_t i;
	} buffer;
	uint32_t size = strlen(SHARKPORT_HEADER);
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
	size = 0x1C + GBASavedataSize(&gba->memory.savedata);
	if (size == 0x1C) {
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
	size_t i;
	for (i = 0; i < 0x1C; ++i) {
		checksum += buffer.c[i] << (checksum % 24);
	}

	if (gba->memory.savedata.type == SAVEDATA_EEPROM) {
		for (i = 0; i < size; ++i) {
			char byte = gba->memory.savedata.data[i ^ 7];
			checksum += byte << (checksum % 24);
			vf->write(vf, &byte, 1);
		}
	} else if (vf->write(vf, gba->memory.savedata.data, size) < size) {
		return false;
	} else {
		for (i = 0; i < size; ++i) {
			checksum += ((char) gba->memory.savedata.data[i]) << (checksum % 24);
		}
	}

	STORE_32(checksum, 0, &buffer.i);
	if (vf->write(vf, &buffer.i, 4) < 4) {
		return false;
	}

	return true;
}

int GBASavedataGSVPayloadSize(struct VFile* vf) {
	union {
		char c[8];
		int32_t i;
	} buffer;
	vf->seek(vf, 0, SEEK_SET);
	if (vf->read(vf, &buffer.c, 8) < 8) {
		return 0;
	}
	if (memcmp(GSV_HEADER, buffer.c, 8) != 0) {
		return 0;
	}

	// Skip the checksum
	if (vf->read(vf, &buffer.i, 4) < 4) {
		return 0;
	}

	struct {
		char name[12];
		int padding;
		int type;
		int unk[3];
		char description[0x400];
		char footer[4];
	} header;

	if (vf->read(vf, &header, sizeof(header)) < (ssize_t) sizeof(header)) {
		return 0;
	}
	if (memcmp(GSV_FOOTER, header.footer, 4) != 0) {
		return 0;
	}

	int type;
	LOAD_32(type, 0, &header.type);
	switch (type) {
	case 2:
		return GBA_SIZE_SRAM;
	case 3:
		return GBA_SIZE_EEPROM512;
	case 4:
		return GBA_SIZE_EEPROM;
	case 5:
		return GBA_SIZE_FLASH512;
	case 6:
		return GBA_SIZE_FLASH1M; // Unconfirmed
	default:
		return vf->size(vf) - GSV_PAYLOAD_OFFSET;
	}
}

void* GBASavedataGSVGetPayload(struct VFile* vf, size_t* osize, uint8_t* ident, bool testChecksum) {
	int32_t size = GBASavedataGSVPayloadSize(vf);
	if (!size || size > GBA_SIZE_FLASH1M) {
		return NULL;
	}

	vf->seek(vf, GSV_IDENT_OFFSET, SEEK_SET);
	if (ident && vf->read(vf, ident, 12) != 12) {
		return NULL;
	}
	vf->seek(vf, GSV_PAYLOAD_OFFSET, SEEK_SET);

	int8_t* payload = malloc(size);
	if (vf->read(vf, payload, size) != size) {
		free(payload);
		return NULL;
	}
	UNUSED(testChecksum); // The checksum format is currently unknown
	*osize = size;
	return payload;
}

bool GBASavedataImportGSV(struct GBA* gba, struct VFile* vf, bool testChecksum) {
	size_t size;
	uint8_t ident[12];
	void* payload = GBASavedataGSVGetPayload(vf, &size, ident, testChecksum);
	if (!payload) {
		return false;
	}
	struct GBACartridge* cart = (struct GBACartridge*) gba->memory.rom;
	if (memcmp(ident, cart->title, sizeof(ident)) != 0) {
		free(payload);
		return false;
	}

	return _importSavedata(gba, payload, size);
}
