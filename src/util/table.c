/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/table.h>

#include <mgba-util/hash.h>
#include <mgba-util/string.h>

#define LIST_INITIAL_SIZE 8
#define TABLE_INITIAL_SIZE 8

#define TABLE_COMPARATOR(LIST, INDEX) LIST->list[(INDEX)].key == key
#define HASH_TABLE_COMPARATOR(LIST, INDEX) LIST->list[(INDEX)].key == hash && strncmp(LIST->list[(INDEX)].stringKey, key, LIST->list[(INDEX)].keylen) == 0

#define TABLE_LOOKUP_START(COMPARATOR, LIST, KEY) \
	uint32_t entry = (KEY) & (table->tableSize - 1); \
	LIST = &table->table[entry]; \
	size_t i; \
	for (i = 0; i < LIST->nEntries; ++i) { \
		if (COMPARATOR(LIST, i)) { \
			struct TableTuple* lookupResult = &LIST->list[i]; \
			UNUSED(lookupResult);

#define TABLE_LOOKUP_END \
			break; \
		} \
	}

struct TableTuple {
	uint32_t key;
	char* stringKey;
	size_t keylen;
	void* value;
};

struct TableList {
	struct TableTuple* list;
	size_t nEntries;
	size_t listSize;
};

static struct TableList* _resizeAsNeeded(struct Table* table, struct TableList* list, uint32_t key) {
	UNUSED(table);
	UNUSED(key);
	// TODO: Expand table if needed
	if (list->nEntries + 1 == list->listSize) {
		list->listSize *= 2;
		list->list = realloc(list->list, list->listSize * sizeof(struct TableTuple));
	}
	return list;
}

static void _removeItemFromList(struct Table* table, struct TableList* list, size_t item) {
	--list->nEntries;
	--table->size;
	free(list->list[item].stringKey);
	if (table->deinitializer) {
		table->deinitializer(list->list[item].value);
	}
	if (item != list->nEntries) {
		list->list[item] = list->list[list->nEntries];
	}
}

void TableInit(struct Table* table, size_t initialSize, void (deinitializer(void*))) {
	if (initialSize < 2 || (initialSize & (initialSize - 1))) {
		initialSize = TABLE_INITIAL_SIZE;
	}
	table->tableSize = initialSize;
	table->table = calloc(table->tableSize, sizeof(struct TableList));
	table->size = 0;
	table->deinitializer = deinitializer;

	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		table->table[i].listSize = LIST_INITIAL_SIZE;
		table->table[i].nEntries = 0;
		table->table[i].list = calloc(LIST_INITIAL_SIZE, sizeof(struct TableTuple));
	}
}

void TableDeinit(struct Table* table) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			free(list->list[j].stringKey);
			if (table->deinitializer) {
				table->deinitializer(list->list[j].value);
			}
		}
		free(list->list);
	}
	free(table->table);
	table->table = 0;
	table->tableSize = 0;
}

void* TableLookup(const struct Table* table, uint32_t key) {
	const struct TableList* list;
	TABLE_LOOKUP_START(TABLE_COMPARATOR, list, key) {
		return lookupResult->value;
	} TABLE_LOOKUP_END;
	return 0;
}

void TableInsert(struct Table* table, uint32_t key, void* value) {
	struct TableList* list;
	TABLE_LOOKUP_START(TABLE_COMPARATOR, list, key) {
		if (value != lookupResult->value) {
			if (table->deinitializer) {
				table->deinitializer(lookupResult->value);
			}
			lookupResult->value = value;
		}
		return;
	} TABLE_LOOKUP_END;
	list = _resizeAsNeeded(table, list, key);
	list->list[list->nEntries].key = key;
	list->list[list->nEntries].stringKey = 0;
	list->list[list->nEntries].value = value;
	++list->nEntries;
	++table->size;
}

void TableRemove(struct Table* table, uint32_t key) {
	struct TableList* list;
	TABLE_LOOKUP_START(TABLE_COMPARATOR, list, key) {
		_removeItemFromList(table, list, i); // TODO: Move i out of the macro
	} TABLE_LOOKUP_END;
}

void TableClear(struct Table* table) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		struct TableList* list = &table->table[i];
		if (table->deinitializer) {
			size_t j;
			for (j = 0; j < list->nEntries; ++j) {
				table->deinitializer(list->list[j].value);
			}
		}
		free(list->list);
		list->listSize = LIST_INITIAL_SIZE;
		list->nEntries = 0;
		list->list = calloc(LIST_INITIAL_SIZE, sizeof(struct TableTuple));
	}
}

void TableEnumerate(const struct Table* table, void (handler(uint32_t key, void* value, void* user)), void* user) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		const struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			handler(list->list[j].key, list->list[j].value, user);
		}
	}
}

size_t TableSize(const struct Table* table) {
	return table->size;
}

void HashTableInit(struct Table* table, size_t initialSize, void (deinitializer(void*))) {
	TableInit(table, initialSize, deinitializer);
}

void HashTableDeinit(struct Table* table) {
	TableDeinit(table);
}

void* HashTableLookup(const struct Table* table, const char* key) {
	uint32_t hash = hash32(key, strlen(key), 0);
	const struct TableList* list;
	TABLE_LOOKUP_START(HASH_TABLE_COMPARATOR, list, hash) {
		return lookupResult->value;
	} TABLE_LOOKUP_END;
	return 0;
}

void HashTableInsert(struct Table* table, const char* key, void* value) {
	uint32_t hash = hash32(key, strlen(key), 0);
	struct TableList* list;
	TABLE_LOOKUP_START(HASH_TABLE_COMPARATOR, list, hash) {
		if (value != lookupResult->value) {
			if (table->deinitializer) {
				table->deinitializer(lookupResult->value);
			}
			lookupResult->value = value;
		}
		return;
	} TABLE_LOOKUP_END;
	list = _resizeAsNeeded(table, list, hash);
	list->list[list->nEntries].key = hash;
	list->list[list->nEntries].stringKey = strdup(key);
	list->list[list->nEntries].keylen = strlen(key);
	list->list[list->nEntries].value = value;
	++list->nEntries;
	++table->size;
}

void HashTableRemove(struct Table* table, const char* key) {
	uint32_t hash = hash32(key, strlen(key), 0);
	struct TableList* list;
	TABLE_LOOKUP_START(HASH_TABLE_COMPARATOR, list, hash) {
		_removeItemFromList(table, list, i); // TODO: Move i out of the macro
	} TABLE_LOOKUP_END;
}

void HashTableClear(struct Table* table) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			if (table->deinitializer) {
				table->deinitializer(list->list[j].value);
			}
			free(list->list[j].stringKey);
		}
		free(list->list);
		list->listSize = LIST_INITIAL_SIZE;
		list->nEntries = 0;
		list->list = calloc(LIST_INITIAL_SIZE, sizeof(struct TableTuple));
	}
}

void HashTableEnumerate(const struct Table* table, void (handler(const char* key, void* value, void* user)), void* user) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		const struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			handler(list->list[j].stringKey, list->list[j].value, user);
		}
	}
}

size_t HashTableSize(const struct Table* table) {
	return table->size;
}
