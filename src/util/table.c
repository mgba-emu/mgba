/* Copyright (c) 2013-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/table.h>

#include <mgba-util/hash.h>
#include <mgba-util/math.h>
#include <mgba-util/string.h>

#define LIST_INITIAL_SIZE 4
#define TABLE_INITIAL_SIZE 8
#define REBALANCE_THRESHOLD 4

#define TABLE_COMPARATOR(LIST, INDEX) LIST->list[(INDEX)].key == key
#define HASH_TABLE_STRNCMP_COMPARATOR(LIST, INDEX) LIST->list[(INDEX)].key == hash && strncmp(LIST->list[(INDEX)].stringKey, key, LIST->list[(INDEX)].keylen) == 0
#define HASH_TABLE_MEMCMP_COMPARATOR(LIST, INDEX) LIST->list[(INDEX)].key == hash && LIST->list[(INDEX)].keylen == keylen && memcmp(LIST->list[(INDEX)].stringKey, key, LIST->list[(INDEX)].keylen) == 0
#define HASH_TABLE_CUSTOM_COMPARATOR(LIST, INDEX) LIST->list[(INDEX)].key == hash && table->fn.equal(LIST->list[(INDEX)].stringKey, key)

#define TABLE_LOOKUP_START(COMPARATOR, LIST) \
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

void HashTableInsertBinaryMoveKey(struct Table* table, void* key, size_t keylen, void* value);

static inline const struct TableList* _getConstList(const struct Table* table, uint32_t key) {
	uint32_t entry = key & (table->tableSize - 1);
	return &table->table[entry];
}

static inline struct TableList* _getList(struct Table* table, uint32_t key) {
	uint32_t entry = key & (table->tableSize - 1);
	return &table->table[entry];
}

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
	if (table->fn.deref) {
		table->fn.deref(list->list[item].stringKey);
	} else {
		free(list->list[item].stringKey);
	}
	if (table->fn.deinitializer) {
		table->fn.deinitializer(list->list[item].value);
	}
	if (item != list->nEntries) {
		list->list[item] = list->list[list->nEntries];
	}
}

static void _rebalance(struct Table* table) {
	struct Table newTable;
	TableInit(&newTable, table->tableSize * REBALANCE_THRESHOLD, NULL);
	memcpy(&newTable.fn, &table->fn, sizeof(newTable.fn));
	newTable.seed = table->seed * 134775813 + 1;
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			if (!table->fn.equal) {
				HashTableInsertBinaryMoveKey(&newTable, list->list[j].stringKey, list->list[j].keylen, list->list[j].value);
			} else {
				HashTableInsertCustom(&newTable, list->list[j].stringKey, list->list[j].value);
				table->fn.deref(list->list[j].stringKey);
			}
		}
		free(list->list);
	}
	free(table->table);
	table->tableSize = newTable.tableSize;
	table->table = newTable.table;
	table->seed = newTable.seed;
}

void TableInit(struct Table* table, size_t initialSize, void (*deinitializer)(void*)) {
	if (initialSize < 2) {
		initialSize = TABLE_INITIAL_SIZE;
	} else if (initialSize & (initialSize - 1)) {
		initialSize = toPow2(initialSize);
	}
	table->tableSize = initialSize;
	table->table = calloc(table->tableSize, sizeof(struct TableList));
	table->size = 0;
	table->fn = (struct TableFunctions) {
		.deinitializer = deinitializer
	};
	table->seed = 0;

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
			if (table->fn.deref) {
				table->fn.deref(list->list[j].stringKey);
			} else {
				free(list->list[j].stringKey);
			}
			if (table->fn.deinitializer) {
				table->fn.deinitializer(list->list[j].value);
			}
		}
		free(list->list);
	}
	free(table->table);
	table->table = 0;
	table->tableSize = 0;
}

void* TableLookup(const struct Table* table, uint32_t key) {
	const struct TableList* list = _getConstList(table, key);
	TABLE_LOOKUP_START(TABLE_COMPARATOR, list) {
		return lookupResult->value;
	} TABLE_LOOKUP_END;
	return 0;
}

void TableInsert(struct Table* table, uint32_t key, void* value) {
	struct TableList* list = _getList(table, key);
	if (table->size >= table->tableSize * REBALANCE_THRESHOLD) {
		struct Table newTable;
		TableInit(&newTable, table->tableSize * REBALANCE_THRESHOLD, NULL);
		memcpy(&newTable.fn, &table->fn, sizeof(newTable.fn));
		size_t i;
		for (i = 0; i < table->tableSize; ++i) {
			struct TableList* list = &table->table[i];
			size_t j;
			for (j = 0; j < list->nEntries; ++j) {
				TableInsert(&newTable, list->list[j].key, list->list[j].value);
			}
			free(list->list);
		}
		free(table->table);
		table->tableSize = newTable.tableSize;
		table->table = newTable.table;
		list = _getList(table, key);
	}
	TABLE_LOOKUP_START(TABLE_COMPARATOR, list) {
		if (value != lookupResult->value) {
			if (table->fn.deinitializer) {
				table->fn.deinitializer(lookupResult->value);
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
	struct TableList* list = _getList(table, key);
	TABLE_LOOKUP_START(TABLE_COMPARATOR, list) {
		_removeItemFromList(table, list, i); // TODO: Move i out of the macro
	} TABLE_LOOKUP_END;
}

void TableClear(struct Table* table) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		struct TableList* list = &table->table[i];
		if (table->fn.deinitializer) {
			size_t j;
			for (j = 0; j < list->nEntries; ++j) {
				table->fn.deinitializer(list->list[j].value);
			}
		}
		free(list->list);
		list->listSize = LIST_INITIAL_SIZE;
		list->nEntries = 0;
		list->list = calloc(LIST_INITIAL_SIZE, sizeof(struct TableTuple));
	}
}

void TableEnumerate(const struct Table* table, void (*handler)(uint32_t key, void* value, void* user), void* user) {
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

bool TableIteratorStart(const struct Table* table, struct TableIterator* iter) {
	iter->entry = 0;
	for (iter->bucket = 0; iter->bucket < table->tableSize; ++iter->bucket) {
		if (table->table[iter->bucket].nEntries) {
			break;
		}
	}
	return iter->bucket < table->tableSize;
}

bool TableIteratorNext(const struct Table* table, struct TableIterator* iter) {
	if (iter->entry + 1 < table->table[iter->bucket].nEntries) {
		++iter->entry;
		return true;
	}
	if (iter->bucket + 1 < table->tableSize) {
		iter->entry = 0;
		for (++iter->bucket; iter->bucket < table->tableSize; ++iter->bucket) {
			if (table->table[iter->bucket].nEntries) {
				break;
			}
		}
		return iter->bucket < table->tableSize;
	}
	return false;
}

uint32_t TableIteratorGetKey(const struct Table* table, const struct TableIterator* iter) {
	return table->table[iter->bucket].list[iter->entry].key;
}

void* TableIteratorGetValue(const struct Table* table, const struct TableIterator* iter) {
	return table->table[iter->bucket].list[iter->entry].value;
}

bool TableIteratorLookup(const struct Table* table, struct TableIterator* iter, uint32_t key) {
	uint32_t bucket = key & (table->tableSize - 1);
	const struct TableList* list = &table->table[bucket];
	TABLE_LOOKUP_START(TABLE_COMPARATOR, list) {
		iter->bucket = bucket;
		iter->entry = i;
		return true;
	} TABLE_LOOKUP_END;
	return false;
}

void HashTableInit(struct Table* table, size_t initialSize, void (*deinitializer)(void*)) {
	TableInit(table, initialSize, deinitializer);
	table->seed = 1;
}

void HashTableInitCustom(struct Table* table, size_t initialSize, const struct TableFunctions* funcs) {
	HashTableInit(table, initialSize, NULL);
	table->fn = *funcs;
}

void HashTableDeinit(struct Table* table) {
	TableDeinit(table);
}

void* HashTableLookup(const struct Table* table, const char* key) {
	uint32_t hash;
	if (table->fn.hash) {
		hash = table->fn.hash(key, strlen(key), table->seed);
	} else {
		hash = hash32(key, strlen(key), table->seed);
	}
	const struct TableList* list = _getConstList(table, hash);
	TABLE_LOOKUP_START(HASH_TABLE_STRNCMP_COMPARATOR, list) {
		return lookupResult->value;
	} TABLE_LOOKUP_END;
	return 0;
}

void* HashTableLookupBinary(const struct Table* table, const void* key, size_t keylen) {
	uint32_t hash;
	if (table->fn.hash) {
		hash = table->fn.hash(key, keylen, table->seed);
	} else {
		hash = hash32(key, keylen, table->seed);
	}
	const struct TableList* list = _getConstList(table, hash);
	TABLE_LOOKUP_START(HASH_TABLE_MEMCMP_COMPARATOR, list) {
		return lookupResult->value;
	} TABLE_LOOKUP_END;
	return 0;
}

void* HashTableLookupCustom(const struct Table* table, void* key) {
	uint32_t hash = table->fn.hash(key, 0, table->seed);
	const struct TableList* list = _getConstList(table, hash);
	TABLE_LOOKUP_START(HASH_TABLE_CUSTOM_COMPARATOR, list) {
		return lookupResult->value;
	} TABLE_LOOKUP_END;
	return 0;
}

void HashTableInsert(struct Table* table, const char* key, void* value) {
	uint32_t hash;
	if (table->fn.hash) {
		hash = table->fn.hash(key, strlen(key), table->seed);
	} else {
		hash = hash32(key, strlen(key), table->seed);
	}
	struct TableList* list = _getList(table, hash);
	if (table->size >= table->tableSize * REBALANCE_THRESHOLD) {
		_rebalance(table);
		if (table->fn.hash) {
			hash = table->fn.hash(key, strlen(key), table->seed);
		} else {
			hash = hash32(key, strlen(key), table->seed);
		}
		list = _getList(table, hash);
	}
	TABLE_LOOKUP_START(HASH_TABLE_STRNCMP_COMPARATOR, list) {
		if (value != lookupResult->value) {
			if (table->fn.deinitializer) {
				table->fn.deinitializer(lookupResult->value);
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

void HashTableInsertBinary(struct Table* table, const void* key, size_t keylen, void* value) {
	uint32_t hash;
	if (table->fn.hash) {
		hash = table->fn.hash(key, keylen, table->seed);
	} else {
		hash = hash32(key, keylen, table->seed);
	}
	struct TableList* list = _getList(table, hash);
	if (table->size >= table->tableSize * REBALANCE_THRESHOLD) {
		_rebalance(table);
		if (table->fn.hash) {
			hash = table->fn.hash(key, keylen, table->seed);
		} else {
			hash = hash32(key, keylen, table->seed);
		}
		list = _getList(table, hash);
	}
	TABLE_LOOKUP_START(HASH_TABLE_MEMCMP_COMPARATOR, list) {
		if (value != lookupResult->value) {
			if (table->fn.deinitializer) {
				table->fn.deinitializer(lookupResult->value);
			}
			lookupResult->value = value;
		}
		return;
	} TABLE_LOOKUP_END;
	list = _resizeAsNeeded(table, list, hash);
	list->list[list->nEntries].key = hash;
	list->list[list->nEntries].stringKey = malloc(keylen);
	memcpy(list->list[list->nEntries].stringKey, key, keylen);
	list->list[list->nEntries].keylen = keylen;
	list->list[list->nEntries].value = value;
	++list->nEntries;
	++table->size;
}

void HashTableInsertCustom(struct Table* table, void* key, void* value) {
	uint32_t hash = table->fn.hash(key, 0, table->seed);
	struct TableList* list = _getList(table, hash);
	if (table->size >= table->tableSize * REBALANCE_THRESHOLD) {
		_rebalance(table);
		hash = table->fn.hash(key, 0, table->seed);
		list = _getList(table, hash);
	}
	TABLE_LOOKUP_START(HASH_TABLE_CUSTOM_COMPARATOR, list) {
		if (value != lookupResult->value) {
			if (table->fn.deinitializer) {
				table->fn.deinitializer(lookupResult->value);
			}
			lookupResult->value = value;
		}
		return;
	} TABLE_LOOKUP_END;
	list = _resizeAsNeeded(table, list, hash);
	list->list[list->nEntries].key = hash;
	list->list[list->nEntries].stringKey = table->fn.ref(key);
	list->list[list->nEntries].keylen = 0;
	list->list[list->nEntries].value = value;
	++list->nEntries;
	++table->size;
}

void HashTableInsertBinaryMoveKey(struct Table* table, void* key, size_t keylen, void* value) {
	uint32_t hash;
	if (table->fn.hash) {
		hash = table->fn.hash(key, keylen, table->seed);
	} else {
		hash = hash32(key, keylen, table->seed);
	}
	struct TableList* list = _getList(table, hash);
	if (table->size >= table->tableSize * REBALANCE_THRESHOLD) {
		_rebalance(table);
		if (table->fn.hash) {
			hash = table->fn.hash(key, keylen, table->seed);
		} else {
			hash = hash32(key, keylen, table->seed);
		}
		list = _getList(table, hash);
	}
	TABLE_LOOKUP_START(HASH_TABLE_MEMCMP_COMPARATOR, list) {
		if (value != lookupResult->value) {
			if (table->fn.deinitializer) {
				table->fn.deinitializer(lookupResult->value);
			}
			lookupResult->value = value;
		}
		return;
	} TABLE_LOOKUP_END;
	list = _resizeAsNeeded(table, list, hash);
	list->list[list->nEntries].key = hash;
	list->list[list->nEntries].stringKey = key;
	list->list[list->nEntries].keylen = keylen;
	list->list[list->nEntries].value = value;
	++list->nEntries;
	++table->size;
}

void HashTableRemove(struct Table* table, const char* key) {
	uint32_t hash;

	if (table->fn.hash) {
		hash = table->fn.hash(key, strlen(key), table->seed);
	} else {
		hash = hash32(key, strlen(key), table->seed);
	}
	struct TableList* list = _getList(table, hash);
	TABLE_LOOKUP_START(HASH_TABLE_STRNCMP_COMPARATOR, list) {
		_removeItemFromList(table, list, i); // TODO: Move i out of the macro
	} TABLE_LOOKUP_END;
}

void HashTableRemoveBinary(struct Table* table, const void* key, size_t keylen) {
	uint32_t hash;
	if (table->fn.hash) {
		hash = table->fn.hash(key, keylen, table->seed);
	} else {
		hash = hash32(key, keylen, table->seed);
	}
	struct TableList* list = _getList(table, hash);
	TABLE_LOOKUP_START(HASH_TABLE_MEMCMP_COMPARATOR, list) {
		_removeItemFromList(table, list, i); // TODO: Move i out of the macro
	} TABLE_LOOKUP_END;
}

void HashTableRemoveCustom(struct Table* table, void* key) {
	uint32_t hash = table->fn.hash(key, 0, table->seed);
	struct TableList* list = _getList(table, hash);
	TABLE_LOOKUP_START(HASH_TABLE_CUSTOM_COMPARATOR, list) {
		_removeItemFromList(table, list, i); // TODO: Move i out of the macro
	} TABLE_LOOKUP_END;
}

void HashTableClear(struct Table* table) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			if (table->fn.deinitializer) {
				table->fn.deinitializer(list->list[j].value);
			}
			if (table->fn.deref) {
				table->fn.deref(list->list[j].stringKey);
			} else {
				free(list->list[j].stringKey);
			}
		}
		free(list->list);
		list->listSize = LIST_INITIAL_SIZE;
		list->nEntries = 0;
		list->list = calloc(LIST_INITIAL_SIZE, sizeof(struct TableTuple));
	}
}

void HashTableEnumerate(const struct Table* table, void (*handler)(const char* key, void* value, void* user), void* user) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		const struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			handler(list->list[j].stringKey, list->list[j].value, user);
		}
	}
}

void HashTableEnumerateBinary(const struct Table* table, void (*handler)(const char* key, size_t keylen, void* value, void* user), void* user) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		const struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			handler(list->list[j].stringKey, list->list[j].keylen, list->list[j].value, user);
		}
	}
}

void HashTableEnumerateCustom(const struct Table* table, void (*handler)(void* key, void* value, void* user), void* user) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		const struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			handler((char*) list->list[j].stringKey, list->list[j].value, user);
		}
	}
}

const char* HashTableSearch(const struct Table* table, bool (*predicate)(const char* key, const void* value, const void* user), const void* user) {
	size_t i;
	const char* result = NULL;
	for (i = 0; i < table->tableSize; ++i) {
		const struct TableList* list = &table->table[i];
		size_t j;
		for (j = 0; j < list->nEntries; ++j) {
			if (predicate(list->list[j].stringKey, list->list[j].value, user)) {
				return list->list[j].stringKey;
			}
		}
	}
	return result;
}

static bool HashTableRefEqual(const char* key, const void* value, const void* user) {
	UNUSED(key);
	return value == user;
}

const char* HashTableSearchPointer(const struct Table* table, const void* value) {
	return HashTableSearch(table, HashTableRefEqual, value);
}

struct HashTableSearchParam {
	const void* value;
	size_t bytes;
};

static bool HashTableMemcmp(const char* key, const void* value, const void* user) {
	UNUSED(key);
	const struct HashTableSearchParam* ref = user;
	return memcmp(value, ref->value, ref->bytes) == 0;
}

const char* HashTableSearchData(const struct Table* table, const void* value, const size_t bytes) {
	struct HashTableSearchParam ref = { value, bytes };
	return HashTableSearch(table, HashTableMemcmp, &ref);
}

static bool HashTableStrcmp(const char* key, const void* value, const void* user) {
	UNUSED(key);
	return strcmp(value, user) == 0;
}

const char* HashTableSearchString(const struct Table* table, const char* value) {
	return HashTableSearch(table, HashTableStrcmp, value);
}

size_t HashTableSize(const struct Table* table) {
	return table->size;
}

bool HashTableIteratorStart(const struct Table* table, struct TableIterator* iter) {
	return TableIteratorStart(table, iter);
}

bool HashTableIteratorNext(const struct Table* table, struct TableIterator* iter) {
	return TableIteratorNext(table, iter);
}

const char* HashTableIteratorGetKey(const struct Table* table, const struct TableIterator* iter) {
	return table->table[iter->bucket].list[iter->entry].stringKey;
}

const void* HashTableIteratorGetBinaryKey(const struct Table* table, const struct TableIterator* iter) {
	return table->table[iter->bucket].list[iter->entry].stringKey;
}

size_t HashTableIteratorGetBinaryKeyLen(const struct Table* table, const struct TableIterator* iter) {
	return table->table[iter->bucket].list[iter->entry].keylen;
}

void* HashTableIteratorGetCustomKey(const struct Table* table, const struct TableIterator* iter) {
	return (char*) table->table[iter->bucket].list[iter->entry].stringKey;
}

void* HashTableIteratorGetValue(const struct Table* table, const struct TableIterator* iter) {
	return TableIteratorGetValue(table, iter);
}

bool HashTableIteratorLookup(const struct Table* table, struct TableIterator* iter, const char* key) {
	uint32_t hash;
	if (table->fn.hash) {
		hash = table->fn.hash(key, strlen(key), table->seed);
	} else {
		hash = hash32(key, strlen(key), table->seed);
	}
	uint32_t bucket = hash & (table->tableSize - 1);
	const struct TableList* list = &table->table[bucket];
	TABLE_LOOKUP_START(HASH_TABLE_STRNCMP_COMPARATOR, list) {
		iter->bucket = bucket;
		iter->entry = i;
		return true;
	} TABLE_LOOKUP_END;
	return false;
}

bool HashTableIteratorLookupBinary(const struct Table* table, struct TableIterator* iter, const void* key, size_t keylen) {
	uint32_t hash;
	if (table->fn.hash) {
		hash = table->fn.hash(key, keylen, table->seed);
	} else {
		hash = hash32(key, keylen, table->seed);
	}
	uint32_t bucket = hash & (table->tableSize - 1);
	const struct TableList* list = &table->table[bucket];
	TABLE_LOOKUP_START(HASH_TABLE_MEMCMP_COMPARATOR, list) {
		iter->bucket = bucket;
		iter->entry = i;
		return true;
	} TABLE_LOOKUP_END;
	return false;
}

bool HashTableIteratorLookupCustom(const struct Table* table, struct TableIterator* iter, void* key) {
	uint32_t hash = table->fn.hash(key, 0, table->seed);
	uint32_t bucket = hash & (table->tableSize - 1);
	const struct TableList* list = &table->table[bucket];
	TABLE_LOOKUP_START(HASH_TABLE_CUSTOM_COMPARATOR, list) {
		iter->bucket = bucket;
		iter->entry = i;
		return true;
	} TABLE_LOOKUP_END;
	return false;
}
