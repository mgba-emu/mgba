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
};

void TableInit(struct Table*, size_t initialSize, void (deinitializer(void*)));
void TableDeinit(struct Table*);

void* TableLookup(const struct Table*, uint32_t key);
void TableInsert(struct Table*, uint32_t key, void* value);

void TableRemove(struct Table*, uint32_t key);
void TableClear(struct Table*);

void TableEnumerate(const struct Table*, void (handler(uint32_t key, void* value, void* user)), void* user);
size_t TableSize(const struct Table*);

void HashTableInit(struct Table* table, size_t initialSize, void (deinitializer(void*)));
void HashTableDeinit(struct Table* table);

void* HashTableLookup(const struct Table*, const char* key);
void HashTableInsert(struct Table*, const char* key, void* value);

void HashTableRemove(struct Table*, const char* key);
void HashTableClear(struct Table*);

void HashTableEnumerate(const struct Table*, void (handler(const char* key, void* value, void* user)), void* user);
size_t HashTableSize(const struct Table*);

CXX_GUARD_END

#endif
