#ifndef TABLE_H
#define TABLE_H

#include "util/common.h"

struct TableList;

struct Table {
	struct TableList* table;
	size_t tableSize;
};

void TableInit(struct Table*, size_t initialSize);
void TableDeinit(struct Table*);

void* TableLookup(struct Table*, uint32_t key);
void TableInsert(struct Table*, uint32_t key, void* value);

#endif
