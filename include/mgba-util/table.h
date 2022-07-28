/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef TABLE_H
#define TABLE_H

#include <mgba-util/common.h>

CXX_GUARD_START

struct TableList;
typedef uint32_t (*HashFunction)(const void* key, size_t len, uint32_t seed);

struct TableFunctions {
	void (*deinitializer)(void*);
	HashFunction hash;
	bool (*equal)(const void*, const void*);
	void* (*ref)(void*);
	void (*deref)(void*);
};

struct Table {
	struct TableList* table;
	size_t tableSize;
	size_t size;
	uint32_t seed;
	struct TableFunctions fn;
};

struct TableIterator {
	size_t bucket;
	size_t entry;
};

void TableInit(struct Table*, size_t initialSize, void (*deinitializer)(void*));
void TableDeinit(struct Table*);

void* TableLookup(const struct Table*, uint32_t key);
void TableInsert(struct Table*, uint32_t key, void* value);

void TableRemove(struct Table*, uint32_t key);
void TableClear(struct Table*);

void TableEnumerate(const struct Table*, void (*handler)(uint32_t key, void* value, void* user), void* user);
size_t TableSize(const struct Table*);

bool TableIteratorStart(const struct Table*, struct TableIterator*);
bool TableIteratorNext(const struct Table*, struct TableIterator*);
uint32_t TableIteratorGetKey(const struct Table*, const struct TableIterator*);
void* TableIteratorGetValue(const struct Table*, const struct TableIterator*);
bool TableIteratorLookup(const struct Table*, struct TableIterator*, uint32_t key);

void HashTableInit(struct Table* table, size_t initialSize, void (*deinitializer)(void*));
void HashTableInitCustom(struct Table* table, size_t initialSize, const struct TableFunctions* funcs);
void HashTableDeinit(struct Table* table);

void* HashTableLookup(const struct Table*, const char* key);
void* HashTableLookupBinary(const struct Table*, const void* key, size_t keylen);
void* HashTableLookupCustom(const struct Table*, void* key);
void HashTableInsert(struct Table*, const char* key, void* value);
void HashTableInsertBinary(struct Table*, const void* key, size_t keylen, void* value);
void HashTableInsertCustom(struct Table*, void* key, void* value);

void HashTableRemove(struct Table*, const char* key);
void HashTableRemoveBinary(struct Table*, const void* key, size_t keylen);
void HashTableRemoveCustom(struct Table*, void* key);
void HashTableClear(struct Table*);

void HashTableEnumerate(const struct Table*, void (*handler)(const char* key, void* value, void* user), void* user);
void HashTableEnumerateBinary(const struct Table*, void (*handler)(const char* key, size_t keylen, void* value, void* user), void* user);
void HashTableEnumerateCustom(const struct Table*, void (*handler)(void* key, void* value, void* user), void* user);
const char* HashTableSearch(const struct Table* table, bool (*predicate)(const char* key, const void* value, const void* user), const void* user);
const char* HashTableSearchPointer(const struct Table* table, const void* value);
const char* HashTableSearchData(const struct Table* table, const void* value, size_t bytes);
const char* HashTableSearchString(const struct Table* table, const char* value);
size_t HashTableSize(const struct Table*);

bool HashTableIteratorStart(const struct Table*, struct TableIterator*);
bool HashTableIteratorNext(const struct Table*, struct TableIterator*);
const char* HashTableIteratorGetKey(const struct Table*, const struct TableIterator*);
const void* HashTableIteratorGetBinaryKey(const struct Table*, const struct TableIterator*);
size_t HashTableIteratorGetBinaryKeyLen(const struct Table*, const struct TableIterator*);
void* HashTableIteratorGetCustomKey(const struct Table*, const struct TableIterator*);
void* HashTableIteratorGetValue(const struct Table*, const struct TableIterator*);
bool HashTableIteratorLookup(const struct Table*, struct TableIterator*, const char* key);
bool HashTableIteratorLookupBinary(const struct Table*, struct TableIterator*, const void* key, size_t keylen);
bool HashTableIteratorLookupCustom(const struct Table*, struct TableIterator*, void* key);

CXX_GUARD_END

#endif
