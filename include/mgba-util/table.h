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

struct Table {
	struct TableList* table;
	size_t tableSize;
	size_t size;
	void (*deinitializer)(void*);
	uint32_t seed;
};

void TableInit(struct Table*, size_t initialSize, void (*deinitializer)(void*));
void TableDeinit(struct Table*);

void* TableLookup(const struct Table*, uint32_t key);
void TableInsert(struct Table*, uint32_t key, void* value);

void TableRemove(struct Table*, uint32_t key);
void TableClear(struct Table*);

void TableEnumerate(const struct Table*, void (*handler)(uint32_t key, void* value, void* user), void* user);
size_t TableSize(const struct Table*);

void HashTableInit(struct Table* table, size_t initialSize, void (*deinitializer)(void*));
void HashTableDeinit(struct Table* table);

void* HashTableLookup(const struct Table*, const char* key);
void* HashTableLookupBinary(const struct Table*, const void* key, size_t keylen);
void HashTableInsert(struct Table*, const char* key, void* value);
void HashTableInsertBinary(struct Table*, const void* key, size_t keylen, void* value);

void HashTableRemove(struct Table*, const char* key);
void HashTableRemoveBinary(struct Table*, const void* key, size_t keylen);
void HashTableClear(struct Table*);

void HashTableEnumerate(const struct Table*, void (*handler)(const char* key, void* value, void* user), void* user);
void HashTableEnumerateBinary(const struct Table*, void (*handler)(const char* key, size_t keylen, void* value, void* user), void* user);
const char* HashTableSearch(const struct Table* table, bool (*predicate)(const char* key, const void* value, const void* user), const void* user);
const char* HashTableSearchPointer(const struct Table* table, const void* value);
const char* HashTableSearchData(const struct Table* table, const void* value, size_t bytes);
const char* HashTableSearchString(const struct Table* table, const char* value);
size_t HashTableSize(const struct Table*);

CXX_GUARD_END

#endif
