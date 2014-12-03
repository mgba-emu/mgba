/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef TABLE_H
#define TABLE_H

#include "util/common.h"

struct TableList;

struct Table {
	struct TableList* table;
	size_t tableSize;
	void (*deinitializer)(void*);
};

void TableInit(struct Table*, size_t initialSize, void (deinitializer(void*)));
void TableDeinit(struct Table*);

void* TableLookup(const struct Table*, uint32_t key);
void TableInsert(struct Table*, uint32_t key, void* value);

void TableRemove(struct Table*, uint32_t key);
void TableClear(struct Table*);

void TableEnumerate(const struct Table*, void (handler(void* value, void* user)), void* user);

static inline void HashTableInit(struct Table* table, size_t initialSize, void (deinitializer(void*))) {
	TableInit(table, initialSize, deinitializer);
}

static inline void HashTableDeinit(struct Table* table) {
	TableDeinit(table);
}

void* HashTableLookup(const struct Table*, const char* key);
void HashTableInsert(struct Table*, const char* key, void* value);

void HashTableRemove(struct Table*, const char* key);
void HashTableClear(struct Table*);

void HashTableEnumerate(const struct Table*, void (handler(const char* key, void* value, void* user)), void* user);

#endif
