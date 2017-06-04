/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/symbols.h>

#include <mgba-util/table.h>

struct mDebuggerSymbol {
	int32_t value;
	int segment;
};

struct mDebuggerSymbols {
	struct Table names;
};

struct mDebuggerSymbols* mDebuggerSymbolTableCreate(void) {
	struct mDebuggerSymbols* st = malloc(sizeof(*st));
	HashTableInit(&st->names, 0, free);
	return st;
}

void mDebuggerSymbolTableDestroy(struct mDebuggerSymbols* st) {
	HashTableDeinit(&st->names);
	free(st);
}

bool mDebuggerSymbolLookup(const struct mDebuggerSymbols* st, const char* name, int32_t* value, int* segment) {
	struct mDebuggerSymbol* sym = HashTableLookup(&st->names, name);
	if (!sym) {
		return false;
	}
	*value = sym->value;
	*segment = sym->segment;
	return true;
}

void mDebuggerSymbolAdd(struct mDebuggerSymbols* st, const char* name, int32_t value, int segment) {
	struct mDebuggerSymbol* sym = malloc(sizeof(*sym));
	sym->value = value;
	sym->segment = segment;
	HashTableInsert(&st->names, name, sym);
}

void mDebuggerSymbolRemove(struct mDebuggerSymbols* st, const char* name) {
	HashTableRemove(&st->names, name);
}
