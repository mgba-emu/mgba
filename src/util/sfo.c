/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This code is loosely based on vita-mksfoex.c from the vitasdk
 * Copyright (c) 2015 Sergi Granell
 * Copyright (c) 2015 Danielle Church
 * Used under the MIT license
 *
 * Which itself is based on mksfoex.c from the pspsdk
 * Copyright (c) 2005  adresd
 * Copyright (c) 2005  Marcus R. Brown
 * Copyright (c) 2005  James Forshaw
 * Copyright (c) 2005  John Kelley
 * Copyright (c) 2005  Jesper Svennevid
 * Used under the BSD 3-clause license
*/

#include <mgba-util/common.h>
#include <mgba-util/table.h>
#include <mgba-util/vfs.h>

#define PSF_MAGIC 0x46535000
#define PSF_VERSION  0x00000101

struct SfoHeader {
	uint32_t magic;
	uint32_t version;
	uint32_t keyofs;
	uint32_t valofs;
	uint32_t count;
};

struct SfoEntry {
	uint16_t nameofs;
	uint8_t alignment;
	uint8_t type;
	uint32_t valsize;
	uint32_t totalsize;
	uint32_t dataofs;
};

enum PSFType {
	PSF_TYPE_BIN = 0,
	PSF_TYPE_STR = 2,
	PSF_TYPE_U32 = 4,
};

struct SfoEntryContainer {
	const char* name;
	enum PSFType type;
	union {
		const char* str;
		uint32_t u32;
	} data;
	uint32_t size;
};

static struct SfoEntryContainer sfoDefaults[] = {
	{ "APP_VER",             PSF_TYPE_STR, { .str = "00.00" } },
	{ "ATTRIBUTE",           PSF_TYPE_U32, { .u32 = 0x8000 } },
	{ "ATTRIBUTE2",          PSF_TYPE_U32, { .u32 = 0 } },
	{ "ATTRIBUTE_MINOR",     PSF_TYPE_U32, { .u32 = 0x10 } },
	{ "BOOT_FILE",           PSF_TYPE_STR, { .str = ""}, 0x20 },
	{ "CATEGORY",            PSF_TYPE_STR, { .str = "gd" } },
	{ "CONTENT_ID",          PSF_TYPE_STR, { .str = "" }, 0x30 },
	{ "EBOOT_APP_MEMSIZE",   PSF_TYPE_U32, { .u32 = 0 } },
	{ "EBOOT_ATTRIBUTE",     PSF_TYPE_U32, { .u32 = 0 } },
	{ "EBOOT_PHY_MEMSIZE",   PSF_TYPE_U32, { .u32 = 0 } },
	{ "LAREA_TYPE",          PSF_TYPE_U32, { .u32 = 0 } },
	{ "NP_COMMUNICATION_ID", PSF_TYPE_STR, { .str = "" }, 0x10 },
	{ "PARENTAL_LEVEL",      PSF_TYPE_U32, { .u32 = 0 } },
	{ "PSP2_DISP_VER",       PSF_TYPE_STR, { .str = "00.000" } },
	{ "PSP2_SYSTEM_VER",     PSF_TYPE_U32, { .u32 = 0 } },
	{ "STITLE",              PSF_TYPE_STR, { .str = "Homebrew" }, 52 },
	{ "TITLE",               PSF_TYPE_STR, { .str = "Homebrew" }, 0x80 },
	{ "TITLE_ID",            PSF_TYPE_STR, { .str = "ABCD99999" } },
	{ "VERSION",             PSF_TYPE_STR, { .str = "00.00" } },
};

bool SfoAddStrValue(struct Table* sfo, const char* name, const char* value) {
	struct SfoEntryContainer* entry = HashTableLookup(sfo, name);
	if (!entry) {
		entry = calloc(1, sizeof(*entry));
		if (!entry) {
			return false;
		}
		entry->name = name;
		HashTableInsert(sfo, name, entry);
	}
	entry->type = PSF_TYPE_STR;
	entry->data.str = value;	
	return true;
}

bool SfoAddU32Value(struct Table* sfo, const char* name, uint32_t value) {
	struct SfoEntryContainer* entry = HashTableLookup(sfo, name);
	if (!entry) {
		entry = calloc(1, sizeof(*entry));
		if (!entry) {
			return false;
		}
		entry->name = name;
		HashTableInsert(sfo, name, entry);
	}
	entry->type = PSF_TYPE_U32;
	entry->data.u32 = value;
	return true;
}

bool SfoSetTitle(struct Table* sfo, const char* title) {
	return SfoAddStrValue(sfo, "TITLE", title) && SfoAddStrValue(sfo, "STITLE", title);
}

void SfoInit(struct Table* sfo) {
	HashTableInit(sfo, 32, free);

	size_t i;
	for (i = 0; i < sizeof(sfoDefaults) / sizeof(sfoDefaults[0]); ++i) {
		struct SfoEntryContainer* entry = calloc(1, sizeof(*entry));
		memcpy(entry, &sfoDefaults[i], sizeof(*entry));
		HashTableInsert(sfo, entry->name, entry);
	}
}

#define ALIGN4(X) (((X) + 3) & ~3)

static int _sfoSort(const void* a, const void* b) {
	const struct SfoEntryContainer* ea = a;
	const struct SfoEntryContainer* eb = b;
	return strcmp(ea->name, eb->name);
}

bool SfoWrite(struct Table* sfo, struct VFile* vf) {
	struct SfoHeader header;
	size_t count = HashTableSize(sfo);
	STORE_32LE(PSF_MAGIC, 0, &header.magic);
	STORE_32LE(PSF_VERSION, 0, &header.version);
	STORE_32LE(count, 0, &header.count);

	struct TableIterator iter;
	if (!TableIteratorStart(sfo, &iter)) {
		return false;
	}

	struct SfoEntryContainer* sortedEntries = calloc(count, sizeof(struct SfoEntryContainer));

	uint32_t keysSize = 0;
	uint32_t dataSize = 0;
	size_t i = 0;
	do {
		memcpy(&sortedEntries[i], TableIteratorGetValue(sfo, &iter), sizeof(struct SfoEntryContainer));
		keysSize += strlen(sortedEntries[i].name) + 1;
		if (!sortedEntries[i].size) {
			switch (sortedEntries[i].type) {
			case PSF_TYPE_STR:
				sortedEntries[i].size = strlen(sortedEntries[i].data.str) + 1;
				break;
			case PSF_TYPE_U32:
				sortedEntries[i].size = 4;
				break;
			}
		}
		dataSize += ALIGN4(sortedEntries[i].size);
		++i;
	} while (TableIteratorNext(sfo, &iter));

	keysSize = ALIGN4(keysSize);

	qsort(sortedEntries, count, sizeof(struct SfoEntryContainer), _sfoSort);

	uint32_t keysOffset = 0;
	uint32_t dataOffset = 0;

	char* keys = calloc(1, keysSize);
	char* data = calloc(1, dataSize);

	struct SfoEntry* entries = calloc(count, sizeof(struct SfoEntry));
	for (i = 0; i < count; ++i) {
		STORE_16LE(keysOffset, 0, &entries[i].nameofs);
		STORE_32LE(dataOffset, 0, &entries[i].dataofs);
		entries[i].alignment = 4;
		entries[i].type = sortedEntries[i].type;

		strcpy(&keys[keysOffset], sortedEntries[i].name);
		keysOffset += strlen(sortedEntries[i].name) + 1;

		if (sortedEntries[i].type == PSF_TYPE_U32) {
			STORE_32LE(4, 0, &entries[i].valsize);
			STORE_32LE(4, 0, &entries[i].totalsize);
			STORE_32LE(sortedEntries[i].data.u32, dataOffset, data);
			dataOffset += 4;
		} else {
			STORE_32LE(ALIGN4(sortedEntries[i].size), 0, &entries[i].totalsize);

			memset(&data[dataOffset], 0, ALIGN4(sortedEntries[i].size));
			if (sortedEntries[i].data.str) {
				STORE_32LE(strlen(sortedEntries[i].data.str) + 1, 0, &entries[i].valsize);
				strncpy(&data[dataOffset], sortedEntries[i].data.str, sortedEntries[i].size);
			} else {
				STORE_32LE(sortedEntries[i].size, 0, &entries[i].valsize);
			}
			dataOffset += ALIGN4(sortedEntries[i].size);
		}
	}

	if (keysSize != ALIGN4(keysOffset) || dataSize != dataOffset) {
		abort();
	}

	free(sortedEntries);

	STORE_32LE(count * sizeof(struct SfoEntry) + sizeof(header), 0, &header.keyofs);
	STORE_32LE(count * sizeof(struct SfoEntry) + sizeof(header) + keysSize, 0, &header.valofs);

	vf->write(vf, &header, sizeof(header));
	vf->write(vf, entries, sizeof(entries[0]) * count);
	vf->write(vf, keys, keysSize);
	vf->write(vf, data, dataSize);

	free(entries);
	free(keys);
	free(data);

	return true;
}
