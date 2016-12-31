/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/nointro.h>

#include <mgba-util/table.h>
#include <mgba-util/vector.h>
#include <mgba-util/vfs.h>

#define KEY_STACK_SIZE 8

struct NoIntroDB {
	struct Table categories;
	struct Table gameCrc;
};

struct NoIntroItem {
	union {
		struct Table hash;
		char* string;
	};
	enum NoIntroItemType {
		NI_HASH,
		NI_STRING
	} type;
};

DECLARE_VECTOR(NoIntroCategory, struct NoIntroItem*);
DEFINE_VECTOR(NoIntroCategory, struct NoIntroItem*);

static void _indexU32x(struct NoIntroDB* db, struct Table* table, const char* categoryKey, const char* key) {
	struct NoIntroCategory* category = HashTableLookup(&db->categories, categoryKey);
	if (!category) {
		return;
	}
	TableInit(table, 256, 0);
	char* tmpKey = strdup(key);
	const char* keyStack[KEY_STACK_SIZE] = { tmpKey };
	size_t i;
	for (i = 1; i < KEY_STACK_SIZE; ++i) {
		char* next = strchr(keyStack[i - 1], '.');
		if (!next) {
			break;
		}
		next[0] = '\0';
		keyStack[i] = next + 1;
	}
	for (i = 0; i < NoIntroCategorySize(category); ++i) {
		struct NoIntroItem* item = *NoIntroCategoryGetPointer(category, i);
		if (!item) {
			continue;
		}
		struct NoIntroItem* keyloc = item;
		size_t s;
		for (s = 0; s < KEY_STACK_SIZE && keyStack[s]; ++s) {
			if (keyloc->type != NI_HASH) {
				keyloc = 0;
				break;
			}
			keyloc = HashTableLookup(&keyloc->hash, keyStack[s]);
			if (!keyloc) {
				break;
			}
		}
		if (!keyloc || keyloc->type != NI_STRING) {
			continue;
		}
		char* end;
		uint32_t key = strtoul(keyloc->string, &end, 16);
		if (!end || *end) {
			continue;
		}
		TableInsert(table, key, item);
	}
	free(tmpKey);
}

static void _itemDeinit(void* value) {
	struct NoIntroItem* item = value;
	switch (item->type) {
	case NI_STRING:
		free(item->string);
		break;
	case NI_HASH:
		HashTableDeinit(&item->hash);
		break;
	}
	free(item);
}

static void _dbDeinit(void* value) {
	struct NoIntroCategory* category = value;
	size_t i;
	for (i = 0; i < NoIntroCategorySize(category); ++i) {
		struct NoIntroItem* item = *NoIntroCategoryGetPointer(category, i);
		switch (item->type) {
		case NI_STRING:
			free(item->string);
			break;
		case NI_HASH:
			HashTableDeinit(&item->hash);
			break;
		}
		free(item);
	}
	NoIntroCategoryDeinit(category);
}

static bool _itemToGame(const struct NoIntroItem* item, struct NoIntroGame* game) {
	if (item->type != NI_HASH) {
		return false;
	}
	struct NoIntroItem* subitem;
	struct NoIntroItem* rom;

	memset(game, 0, sizeof(*game));
	subitem = HashTableLookup(&item->hash, "name");
	if (subitem && subitem->type == NI_STRING) {
		game->name = subitem->string;
	}
	subitem = HashTableLookup(&item->hash, "description");
	if (subitem && subitem->type == NI_STRING) {
		game->description = subitem->string;
	}

	rom = HashTableLookup(&item->hash, "rom");
	if (!rom || rom->type != NI_HASH) {
		return false;
	}
	subitem = HashTableLookup(&rom->hash, "name");
	if (subitem && subitem->type == NI_STRING) {
		game->romName = subitem->string;
	}
	subitem = HashTableLookup(&rom->hash, "size");
	if (subitem && subitem->type == NI_STRING) {
		char* end;
		game->size = strtoul(subitem->string, &end, 0);
		if (!end || *end) {
			game->size = 0;
		}
	}
	// TODO: md5, sha1
	subitem = HashTableLookup(&rom->hash, "flags");
	if (subitem && subitem->type == NI_STRING && strcmp(subitem->string, "verified")) {
		game->verified = true;
	}

	return true;
}

struct NoIntroDB* NoIntroDBLoad(struct VFile* vf) {
	struct NoIntroDB* db = malloc(sizeof(*db));
	HashTableInit(&db->categories, 0, _dbDeinit);
	char line[512];
	struct {
		char* key;
		struct NoIntroItem* item;
	} keyStack[KEY_STACK_SIZE];
	memset(keyStack, 0, sizeof(keyStack));
	struct Table* parent = 0;

	size_t stackDepth = 0;
	while (true) {
		ssize_t bytesRead = vf->readline(vf, line, sizeof(line));
		if (!bytesRead) {
			break;
		}
		ssize_t i;
		const char* token;
		for (i = 0; i < bytesRead; ++i) {
			while (isspace((int) line[i]) && i < bytesRead) {
				++i;
			}
			if (i >= bytesRead) {
				break;
			}
			token = &line[i];
			while (!isspace((int) line[i]) && i < bytesRead) {
				++i;
			}
			if (i >= bytesRead) {
				break;
			}
			switch (token[0]) {
			case '(':
				if (!keyStack[stackDepth].key) {
					goto error;
				}
				keyStack[stackDepth].item = malloc(sizeof(*keyStack[stackDepth].item));
				keyStack[stackDepth].item->type = NI_HASH;
				HashTableInit(&keyStack[stackDepth].item->hash, 8, _itemDeinit);
				if (parent) {
					HashTableInsert(parent, keyStack[stackDepth].key, keyStack[stackDepth].item);
				} else {
					struct NoIntroCategory* category = HashTableLookup(&db->categories, keyStack[stackDepth].key);
					if (!category) {
						category = malloc(sizeof(*category));
						NoIntroCategoryInit(category, 0);
						HashTableInsert(&db->categories, keyStack[stackDepth].key, category);
					}
					*NoIntroCategoryAppend(category) = keyStack[stackDepth].item;
				}
				parent = &keyStack[stackDepth].item->hash;
				++stackDepth;
				if (stackDepth >= KEY_STACK_SIZE) {
					goto error;
				}
				keyStack[stackDepth].key = 0;
				break;
			case ')':
				if (keyStack[stackDepth].key || !stackDepth) {
					goto error;
				}
				--stackDepth;
				if (stackDepth) {
					parent = &keyStack[stackDepth - 1].item->hash;
				} else {
					parent = 0;
				}
				free(keyStack[stackDepth].key);
				keyStack[stackDepth].key = 0;
				break;
			case '"':
				++token;
				for (; line[i] != '"' && i < bytesRead; ++i);
				// Fall through
			default:
				line[i] = '\0';
				if (!keyStack[stackDepth].key) {
					keyStack[stackDepth].key = strdup(token);
				} else {
					struct NoIntroItem* item = malloc(sizeof(*keyStack[stackDepth].item));
					item->type = NI_STRING;
					item->string = strdup(token);
					if (parent) {
						HashTableInsert(parent, keyStack[stackDepth].key, item);
					} else {
						struct NoIntroCategory* category = HashTableLookup(&db->categories, keyStack[stackDepth].key);
						if (!category) {
							category = malloc(sizeof(*category));
							NoIntroCategoryInit(category, 0);
							HashTableInsert(&db->categories, keyStack[stackDepth].key, category);
						}
						*NoIntroCategoryAppend(category) = item;
					}
					free(keyStack[stackDepth].key);
					keyStack[stackDepth].key = 0;
				}
				break;
			}
		}
	}

	_indexU32x(db, &db->gameCrc, "game", "rom.crc");

	return db;

error:
	HashTableDeinit(&db->categories);
	free(db);
	return 0;
}

void NoIntroDBDestroy(struct NoIntroDB* db) {
	HashTableDeinit(&db->categories);
}

bool NoIntroDBLookupGameByCRC(const struct NoIntroDB* db, uint32_t crc32, struct NoIntroGame* game) {
	if (!db) {
		return false;
	}
	struct NoIntroItem* item = TableLookup(&db->gameCrc, crc32);
	if (item) {
		return _itemToGame(item, game);
	}
	return false;
}
