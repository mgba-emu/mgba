/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/debugger/symbols.h>

#include <mgba-util/string.h>
#include <mgba-util/table.h>
#include <mgba-util/hash.h>
#include <mgba-util/vfs.h>

struct mDebuggerSymbol {
	int32_t value;
	int segment;
};

struct mDebuggerSymbols {
	struct Table names;
	struct Table reverse;
};

struct mDebuggerSymbols* mDebuggerSymbolTableCreate(void) {
	struct mDebuggerSymbols* st = malloc(sizeof(*st));
	HashTableInit(&st->names, 0, free);
	HashTableInit(&st->reverse, 0, free);
	return st;
}

void mDebuggerSymbolTableDestroy(struct mDebuggerSymbols* st) {
	HashTableDeinit(&st->names);
	HashTableDeinit(&st->reverse);
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

const char* mDebuggerSymbolReverseLookup(const struct mDebuggerSymbols* st, int32_t value, int segment) {
	struct mDebuggerSymbol sym = { value, segment };
	return HashTableLookupBinary(&st->reverse, &sym, sizeof(sym));
}

void mDebuggerSymbolAdd(struct mDebuggerSymbols* st, const char* name, int32_t value, int segment) {
	struct mDebuggerSymbol* sym = malloc(sizeof(*sym));
	sym->value = value;
	sym->segment = segment;
	HashTableInsert(&st->names, name, sym);
	HashTableInsertBinary(&st->reverse, sym, sizeof(*sym), strdup(name));
}

void mDebuggerSymbolRemove(struct mDebuggerSymbols* st, const char* name) {
	struct mDebuggerSymbol* sym = HashTableLookup(&st->names, name);
	if (sym) {
		HashTableRemoveBinary(&st->reverse, sym, sizeof(*sym));
		HashTableRemove(&st->names, name);
	}
}

void mDebuggerLoadARMIPSSymbols(struct mDebuggerSymbols* st, struct VFile* vf) {
	char line[512];

	while (true) {
		ssize_t bytesRead = vf->readline(vf, line, sizeof(line));
		if (bytesRead <= 0) {
			break;
		}
		if (line[bytesRead - 1] == '\n') {
			line[bytesRead - 1] = '\0';
		}
		uint32_t address = 0;
		const char* buf = line;
		buf = hex32(buf, &address);
		if (!buf) {
			continue;
		}
		bytesRead -= 8;

		while (isspace((int) buf[0]) && bytesRead > 0) {
			--bytesRead;
			++buf;
		}

		if (!bytesRead) {
			continue;
		}

		if (buf[0] == '.') {
			// Directives are not handled yet
			continue;
		}

		char* buf2 = strchr(buf, ',');

		if (buf2 != NULL) {
			// Commas separate names from function sizes
			*buf2 = '\0';
		}

		mDebuggerSymbolAdd(st, buf, address, -1);
	}
}
