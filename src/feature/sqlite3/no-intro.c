/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "no-intro.h"

#include <mgba-util/string.h>
#include <mgba-util/vector.h>
#include <mgba-util/vfs.h>

#include <sqlite3.h>

struct NoIntroDB {
	sqlite3* db;
	sqlite3_stmt* crc32;
	sqlite3_stmt* md5;
	sqlite3_stmt* sha1;
};

struct NoIntroDB* NoIntroDBLoad(const char* path) {
	struct NoIntroDB* db = calloc(1, sizeof(*db));

	if (sqlite3_open_v2(path, &db->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL)) {
		goto error;
	}

	static const char createTables[] =
		"PRAGMA foreign_keys = ON;\n"
		"PRAGMA journal_mode = MEMORY;\n"
		"PRAGMA synchronous = NORMAL;\n"
		"CREATE TABLE IF NOT EXISTS gamedb ("
			"dbid INTEGER NOT NULL PRIMARY KEY ASC,"
			"name TEXT,"
			"version TEXT,"
			"CONSTRAINT versioning UNIQUE (name, version)"
		");\n"
		"CREATE TABLE IF NOT EXISTS games ("
			"gid INTEGER NOT NULL PRIMARY KEY ASC,"
			"name TEXT,"
			"dbid INTEGER NOT NULL REFERENCES gamedb(dbid) ON DELETE CASCADE"
		");\n"
		"CREATE TABLE IF NOT EXISTS roms ("
			"name TEXT,"
			"size INTEGER,"
			"crc32 INTEGER,"
			"md5 BLOB,"
			"sha1 BLOB,"
			"flags INTEGER DEFAULT 0,"
			"gid INTEGER NOT NULL REFERENCES games(gid) ON DELETE CASCADE"
		");\n"
		"CREATE INDEX IF NOT EXISTS crc32 ON roms (crc32);\n"
		"CREATE INDEX IF NOT EXISTS md5 ON roms (md5);\n"
		"CREATE INDEX IF NOT EXISTS sha1 ON roms (sha1);\n";
	if (sqlite3_exec(db->db, createTables, NULL, NULL, NULL)) {
		goto error;
	}

	static const char selectCrc32[] = "SELECT games.name, roms.name, size, crc32, md5, sha1, flags FROM games JOIN roms USING (gid) WHERE roms.crc32 = ?;";
	if (sqlite3_prepare_v2(db->db, selectCrc32, -1, &db->crc32, NULL)) {
		goto error;
	}

	static const char selectMd5[] = "SELECT games.name, roms.name, size, crc32, md5, sha1, flags FROM games JOIN roms USING (gid) WHERE roms.md5 = ?;";
	if (sqlite3_prepare_v2(db->db, selectMd5, -1, &db->md5, NULL)) {
		goto error;
	}

	static const char selectSha1[] = "SELECT games.name, roms.name, size, crc32, md5, sha1, flags FROM games JOIN roms USING (gid) WHERE roms.sha1 = ?;";
	if (sqlite3_prepare_v2(db->db, selectSha1, -1, &db->sha1, NULL)) {
		goto error;
	}

	return db;

error:
	NoIntroDBDestroy(db);
	return NULL;

}

bool NoIntroDBLoadClrMamePro(struct NoIntroDB* db, struct VFile* vf) {
	struct NoIntroGame buffer = { 0 };

	sqlite3_stmt* gamedbTable = NULL;
	sqlite3_stmt* gamedbDrop = NULL;
	sqlite3_stmt* gamedbSelect = NULL;
	sqlite3_stmt* gameTable = NULL;
	sqlite3_stmt* romTable = NULL;
	char* fieldName = NULL;
	sqlite3_int64 currentGame = -1;
	sqlite3_int64 currentDb = -1;
	char* dbType = NULL;
	char* dbVersion = NULL;
	char line[512];

	static const char insertGamedb[] = "INSERT INTO gamedb (name, version) VALUES (?, ?);";
	if (sqlite3_prepare_v2(db->db, insertGamedb, -1, &gamedbTable, NULL)) {
		return false;
	}

	static const char deleteGamedb[] = "DELETE FROM gamedb WHERE name = ? AND version < ?;";
	if (sqlite3_prepare_v2(db->db, deleteGamedb, -1, &gamedbDrop, NULL)) {
		return false;
	}

	static const char selectGamedb[] = "SELECT * FROM gamedb WHERE name = ? AND version >= ?;";
	if (sqlite3_prepare_v2(db->db, selectGamedb, -1, &gamedbSelect, NULL)) {
		return false;
	}

	static const char insertGame[] = "INSERT INTO games (dbid, name) VALUES (?, ?);";
	if (sqlite3_prepare_v2(db->db, insertGame, -1, &gameTable, NULL)) {
		return false;
	}

	static const char insertRom[] = "INSERT INTO roms (gid, name, size, crc32, md5, sha1, flags) VALUES (:game, :name, :size, :crc32, :md5, :sha1, :flags);";
	if (sqlite3_prepare_v2(db->db, insertRom, -1, &romTable, NULL)) {
		return false;
	}

	size_t remainingInTransaction = 0;

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
				if (!fieldName) {
					break;
				}
				if (!remainingInTransaction) {
					remainingInTransaction = 16;
					sqlite3_exec(db->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
				} else {
					--remainingInTransaction;
				}

				if (strcmp(fieldName, "clrmamepro") == 0) {
					free((void*) dbType);
					free((void*) dbVersion);
					dbType = NULL;
					dbVersion = NULL;
					currentDb = -1;
					currentGame = -1;
				} else if (currentDb >= 0 && strcmp(fieldName, "game") == 0) {
					free((void*) buffer.name);
					free((void*) buffer.romName);
					memset(&buffer, 0, sizeof(buffer));
					currentGame = -1;
				} else if (currentDb >= 0 && strcmp(fieldName, "rom") == 0) {
					sqlite3_clear_bindings(gameTable);
					sqlite3_reset(gameTable);
					sqlite3_bind_int64(gameTable, 1, currentDb);
					sqlite3_bind_text(gameTable, 2, buffer.name, -1, SQLITE_TRANSIENT);
					sqlite3_step(gameTable);
					currentGame = sqlite3_last_insert_rowid(db->db);
				}
				free(fieldName);
				fieldName = NULL;
				break;
			case ')':
				if (currentDb < 0 && dbType && dbVersion) {
					sqlite3_clear_bindings(gamedbSelect);
					sqlite3_reset(gamedbSelect);
					sqlite3_bind_text(gamedbSelect, 1, dbType, -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(gamedbSelect, 2, dbVersion, -1, SQLITE_TRANSIENT);
					if (sqlite3_step(gamedbSelect) != SQLITE_ROW) {
						sqlite3_clear_bindings(gamedbDrop);
						sqlite3_reset(gamedbDrop);
						sqlite3_bind_text(gamedbDrop, 1, dbType, -1, SQLITE_TRANSIENT);
						sqlite3_bind_text(gamedbDrop, 2, dbVersion, -1, SQLITE_TRANSIENT);
						sqlite3_step(gamedbDrop);

						sqlite3_clear_bindings(gamedbTable);
						sqlite3_reset(gamedbTable);
						sqlite3_bind_text(gamedbTable, 1, dbType, -1, SQLITE_TRANSIENT);
						sqlite3_bind_text(gamedbTable, 2, dbVersion, -1, SQLITE_TRANSIENT);
						if (sqlite3_step(gamedbTable) == SQLITE_DONE) {
							currentDb = sqlite3_last_insert_rowid(db->db);
						}
					}
					free((void*) dbType);
					free((void*) dbVersion);
					dbType = NULL;
					dbVersion = NULL;
				}
				if (currentGame >= 0 && buffer.romName) {
					sqlite3_clear_bindings(romTable);
					sqlite3_reset(romTable);
					sqlite3_bind_int64(romTable, 1, currentGame);
					sqlite3_bind_text(romTable, 2, buffer.romName, -1, SQLITE_TRANSIENT);
					sqlite3_bind_int64(romTable, 3, buffer.size);
					sqlite3_bind_int(romTable, 4, buffer.crc32);
					sqlite3_bind_blob(romTable, 5, buffer.md5, sizeof(buffer.md5), NULL);
					sqlite3_bind_blob(romTable, 6, buffer.sha1, sizeof(buffer.sha1), NULL);
					sqlite3_bind_int(romTable, 7, buffer.verified);
					sqlite3_step(romTable);
					free((void*) buffer.romName);
					buffer.romName = NULL;
				}
				if (!remainingInTransaction) {
					sqlite3_exec(db->db, "COMMIT;", NULL, NULL, NULL);
				}
				break;
			case '"':
				++token;
				for (; line[i] != '"' && i < bytesRead; ++i);
				// Fall through
			default:
				line[i] = '\0';
				if (fieldName) {
					if (currentGame >= 0) {
						if (strcmp("name", fieldName) == 0) {
							free((void*) buffer.romName);
							buffer.romName = strdup(token);
						} else if (strcmp("size", fieldName) == 0) {
							char* end;
							unsigned long value = strtoul(token, &end, 10);
							if (end) {
								buffer.size = value;
							}
						} else if (strcmp("crc", fieldName) == 0) {
							char* end;
							unsigned long value = strtoul(token, &end, 16);
							if (end) {
								buffer.crc32 = value;
							}
						} else if (strcmp("md5", fieldName) == 0) {
							size_t b;
							for (b = 0; b < sizeof(buffer.md5) && token && *token; ++b) {
								token = hex8(token, &buffer.md5[b]);
							}
						} else if (strcmp("sha1", fieldName) == 0) {
							size_t b;
							for (b = 0; b < sizeof(buffer.sha1) && token && *token; ++b) {
								token = hex8(token, &buffer.sha1[b]);
							}
						} else if (strcmp("flags", fieldName) == 0) {
							buffer.verified = strcmp("verified", fieldName) == 0;
						}
					} else if (currentDb >= 0) {
						if (strcmp("name", fieldName) == 0) {
							free((void*) buffer.name);
							buffer.name = strdup(token);
						}
					} else {
						if (strcmp("name", fieldName) == 0) {
							free((void*) dbType);
							dbType = strdup(token);
						} else if (strcmp("version", fieldName) == 0) {
							free((void*) dbVersion);
							dbVersion = strdup(token);
						}
					}
					free(fieldName);
					fieldName = NULL;
				} else {
					fieldName = strdup(token);
				}
				break;
			}
		}
	}

	free((void*) buffer.name);
	free((void*) buffer.romName);

	if (dbType) {
		free(dbType);
	}
	if (dbVersion) {
		free(dbVersion);
	}
	if (fieldName) {
		free(fieldName);
	}

	sqlite3_finalize(gamedbTable);
	sqlite3_finalize(gamedbDrop);
	sqlite3_finalize(gamedbSelect);
	sqlite3_finalize(gameTable);
	sqlite3_finalize(romTable);

	if (remainingInTransaction) {
		sqlite3_exec(db->db, "COMMIT;", NULL, NULL, NULL);
	}
	sqlite3_exec(db->db, "VACUUM", NULL, NULL, NULL);

	return true;
}

void NoIntroDBDestroy(struct NoIntroDB* db) {
	if (db->crc32) {
		sqlite3_finalize(db->crc32);
	}
	if (db->md5) {
		sqlite3_finalize(db->md5);
	}
	if (db->sha1) {
		sqlite3_finalize(db->sha1);
	}
	if (db->db) {
		sqlite3_close(db->db);
	}
	free(db);
}

void _extractGame(sqlite3_stmt* stmt, struct NoIntroGame* game) {
	game->name = (const char*) sqlite3_column_text(stmt, 0);
	game->romName = (const char*) sqlite3_column_text(stmt, 1);
	game->size = sqlite3_column_int(stmt, 2);
	game->crc32 = sqlite3_column_int(stmt, 3);
	const void* buf = sqlite3_column_blob(stmt, 4);
	if (buf && sqlite3_column_bytes(stmt, 4) == sizeof(game->md5)) {
		memcpy(game->md5, buf, sizeof(game->md5));
	}
	buf = sqlite3_column_blob(stmt, 5);
	if (buf && sqlite3_column_bytes(stmt, 5) == sizeof(game->sha1)) {
		memcpy(game->sha1, buf, sizeof(game->sha1));
	}
	game->verified = sqlite3_column_int(stmt, 6);
}

bool NoIntroDBLookupGameByCRC(const struct NoIntroDB* db, uint32_t crc32, struct NoIntroGame* game) {
	if (!db) {
		return false;
	}
	sqlite3_clear_bindings(db->crc32);
	sqlite3_reset(db->crc32);
	sqlite3_bind_int(db->crc32, 1, crc32);
	if (sqlite3_step(db->crc32) != SQLITE_ROW) {
		return false;
	}
	_extractGame(db->crc32, game);
	return true;
}

bool NoIntroDBLookupGameByMD5(const struct NoIntroDB* db, const uint8_t* md5, struct NoIntroGame* game) {
	if (!db) {
		return false;
	}
	sqlite3_clear_bindings(db->md5);
	sqlite3_reset(db->md5);
	sqlite3_bind_blob(db->md5, 1, md5, 16, NULL);
	if (sqlite3_step(db->md5) != SQLITE_ROW) {
		return false;
	}
	_extractGame(db->md5, game);
	return true;
}

bool NoIntroDBLookupGameBySHA1(const struct NoIntroDB* db, const uint8_t* sha1, struct NoIntroGame* game) {
	if (!db) {
		return false;
	}
	sqlite3_clear_bindings(db->sha1);
	sqlite3_reset(db->sha1);
	sqlite3_bind_blob(db->sha1, 1, sha1, 20, NULL);
	if (sqlite3_step(db->sha1) != SQLITE_ROW) {
		return false;
	}
	_extractGame(db->sha1, game);
	return true;
}
