#include "table.h"

#define LIST_INITIAL_SIZE 8

struct TableTuple {
	uint32_t key;
	void* value;
};

struct TableList {
	struct TableTuple* list;
	size_t nEntries;
	size_t listSize;
};

void TableInit(struct Table* table, size_t initialSize) {
	table->tableSize = initialSize;
	table->table = calloc(table->tableSize, sizeof(struct TableList));

	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		table->table[i].listSize = LIST_INITIAL_SIZE;
		table->table[i].list = calloc(LIST_INITIAL_SIZE, sizeof(struct TableTuple));
	}
}

void TableDeinit(struct Table* table) {
	size_t i;
	for (i = 0; i < table->tableSize; ++i) {
		// TODO: Don't leak entries
		free(table->table[i].list);
	}
	free(table->table);
	table->table = 0;
	table->tableSize = 0;
}

void* TableLookup(struct Table* table, uint32_t key) {
	uint32_t entry = key & (table->tableSize - 1);
	struct TableList* list = &table->table[entry];
	size_t i;
	for (i = 0; i < list->nEntries; ++i) {
		if (list->list[i].key == key) {
			return list->list[i].value;
		}
	}
	return 0;
}

void TableInsert(struct Table* table, uint32_t key, void* value) {
	uint32_t entry = key & (table->tableSize - 1);
	struct TableList* list = &table->table[entry];
	if (list->nEntries + 1 == list->listSize) {
		list->listSize *= 2;
		list->list = realloc(list->list, list->listSize * sizeof(struct TableTuple));
	}
	list->list[list->nEntries].key = key;
	list->list[list->nEntries].value = value;
	++list->nEntries;
}
