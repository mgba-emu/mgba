/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/core/library.h>

#include <mgba/core/core.h>
#include <mgba-util/vfs.h>

#ifdef USE_SQLITE3

#include <sqlite3.h>
#include "feature/sqlite3/no-intro.h"

DEFINE_VECTOR(mLibraryListing, struct mLibraryEntry);

struct mLibrary {
	sqlite3* db;
	sqlite3_stmt* insertPath;
	sqlite3_stmt* insertRom;
	sqlite3_stmt* insertRoot;
	sqlite3_stmt* selectRom;
	sqlite3_stmt* selectRoot;
	sqlite3_stmt* deletePath;
	sqlite3_stmt* deleteRoot;
	sqlite3_stmt* count;
	sqlite3_stmt* select;
	const struct NoIntroDB* gameDB;
};

#define CONSTRAINTS_ROMONLY \
	"CASE WHEN :useSize THEN roms.size = :size ELSE 1 END AND " \
	"CASE WHEN :usePlatform THEN roms.platform = :platform ELSE 1 END AND " \
	"CASE WHEN :useCrc32 THEN roms.crc32 = :crc32 ELSE 1 END AND " \
	"CASE WHEN :useInternalCode THEN roms.internalCode = :internalCode ELSE 1 END"

#define CONSTRAINTS \
	CONSTRAINTS_ROMONLY " AND " \
	"CASE WHEN :useFilename THEN paths.path = :path ELSE 1 END AND " \
	"CASE WHEN :useRoot THEN roots.path = :root ELSE 1 END"

static void _mLibraryDeleteEntry(struct mLibrary* library, struct mLibraryEntry* entry);
static void _mLibraryInsertEntry(struct mLibrary* library, struct mLibraryEntry* entry);
static bool _mLibraryAddEntry(struct mLibrary* library, const char* filename, const char* base, struct VFile* vf);

static void _bindConstraints(sqlite3_stmt* statement, const struct mLibraryEntry* constraints) {
	if (!constraints) {
		return;
	}

	int useIndex, index;
	if (constraints->crc32) {
		useIndex = sqlite3_bind_parameter_index(statement, ":useCrc32");
		index = sqlite3_bind_parameter_index(statement, ":crc32");
		sqlite3_bind_int(statement, useIndex, 1);
		sqlite3_bind_int(statement, index, constraints->crc32);
	}

	if (constraints->filesize) {
		useIndex = sqlite3_bind_parameter_index(statement, ":useSize");
		index = sqlite3_bind_parameter_index(statement, ":size");
		sqlite3_bind_int(statement, useIndex, 1);
		sqlite3_bind_int64(statement, index, constraints->filesize);
	}

	if (constraints->filename) {
		useIndex = sqlite3_bind_parameter_index(statement, ":useFilename");
		index = sqlite3_bind_parameter_index(statement, ":path");
		sqlite3_bind_int(statement, useIndex, 1);
		sqlite3_bind_text(statement, index, constraints->filename, -1, SQLITE_TRANSIENT);
	}

	if (constraints->base) {
		useIndex = sqlite3_bind_parameter_index(statement, ":useRoot");
		index = sqlite3_bind_parameter_index(statement, ":root");
		sqlite3_bind_int(statement, useIndex, 1);
		sqlite3_bind_text(statement, index, constraints->base, -1, SQLITE_TRANSIENT);
	}

	if (constraints->internalCode[0]) {
		useIndex = sqlite3_bind_parameter_index(statement, ":useInternalCode");
		index = sqlite3_bind_parameter_index(statement, ":internalCode");
		sqlite3_bind_int(statement, useIndex, 1);
		sqlite3_bind_text(statement, index, constraints->internalCode, -1, SQLITE_TRANSIENT);
	}

	if (constraints->platform != mPLATFORM_NONE) {
		useIndex = sqlite3_bind_parameter_index(statement, ":usePlatform");
		index = sqlite3_bind_parameter_index(statement, ":platform");
		sqlite3_bind_int(statement, useIndex, 1);
		sqlite3_bind_int(statement, index, constraints->platform);
	}
}

struct mLibrary* mLibraryCreateEmpty(void) {
	return mLibraryLoad(":memory:");
}

struct mLibrary* mLibraryLoad(const char* path) {
	struct mLibrary* library = malloc(sizeof(*library));
	memset(library, 0, sizeof(*library));

	if (sqlite3_open_v2(path, &library->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL)) {
		goto error;
	}

	static const char createTables[] =
		"   PRAGMA foreign_keys = ON;"
		"\n PRAGMA journal_mode = MEMORY;"
		"\n PRAGMA synchronous = NORMAL;"
		"\n CREATE TABLE IF NOT EXISTS version ("
		"\n 	tname TEXT NOT NULL PRIMARY KEY,"
		"\n 	version INTEGER NOT NULL DEFAULT 1"
		"\n );"
		"\n CREATE TABLE IF NOT EXISTS roots ("
		"\n 	rootid INTEGER NOT NULL PRIMARY KEY ASC,"
		"\n 	path TEXT NOT NULL UNIQUE,"
		"\n 	mtime INTEGER NOT NULL DEFAULT 0"
		"\n );"
		"\n CREATE TABLE IF NOT EXISTS roms ("
		"\n 	romid INTEGER NOT NULL PRIMARY KEY ASC,"
		"\n 	internalTitle TEXT,"
		"\n 	internalCode TEXT,"
		"\n 	platform INTEGER NOT NULL DEFAULT -1,"
		"\n 	size INTEGER,"
		"\n 	crc32 INTEGER,"
		"\n 	md5 BLOB,"
		"\n 	sha1 BLOB"
		"\n );"
		"\n CREATE TABLE IF NOT EXISTS paths ("
		"\n 	pathid INTEGER NOT NULL PRIMARY KEY ASC,"
		"\n 	romid INTEGER NOT NULL REFERENCES roms(romid) ON DELETE CASCADE,"
		"\n 	path TEXT NOT NULL,"
		"\n 	mtime INTEGER NOT NULL DEFAULT 0,"
		"\n 	rootid INTEGER REFERENCES roots(rootid) ON DELETE CASCADE,"
		"\n 	customTitle TEXT,"
		"\n 	CONSTRAINT location UNIQUE (path, rootid)"
		"\n );"
		"\n CREATE INDEX IF NOT EXISTS crc32 ON roms (crc32);"
		"\n INSERT OR IGNORE INTO version (tname, version) VALUES ('version', 1);"
		"\n INSERT OR IGNORE INTO version (tname, version) VALUES ('roots', 1);"
		"\n INSERT OR IGNORE INTO version (tname, version) VALUES ('roms', 1);"
		"\n INSERT OR IGNORE INTO version (tname, version) VALUES ('paths', 1);";
	if (sqlite3_exec(library->db, createTables, NULL, NULL, NULL)) {
		goto error;
	}

	static const char insertPath[] = "INSERT INTO paths (romid, path, customTitle, rootid) VALUES (?, ?, ?, ?);";
	if (sqlite3_prepare_v2(library->db, insertPath, -1, &library->insertPath, NULL)) {
		goto error;
	}

	static const char insertRom[] = "INSERT INTO roms (crc32, size, internalCode, platform) VALUES (:crc32, :size, :internalCode, :platform);";
	if (sqlite3_prepare_v2(library->db, insertRom, -1, &library->insertRom, NULL)) {
		goto error;
	}

	static const char insertRoot[] = "INSERT INTO roots (path) VALUES (?);";
	if (sqlite3_prepare_v2(library->db, insertRoot, -1, &library->insertRoot, NULL)) {
		goto error;
	}

	static const char deleteRoot[] = "DELETE FROM roots WHERE path = ?;";
	if (sqlite3_prepare_v2(library->db, deleteRoot, -1, &library->deleteRoot, NULL)) {
		goto error;
	}

	static const char deletePath[] = "DELETE FROM paths WHERE path = ?;";
	if (sqlite3_prepare_v2(library->db, deletePath, -1, &library->deletePath, NULL)) {
		goto error;
	}

	static const char selectRom[] = "SELECT romid FROM roms WHERE " CONSTRAINTS_ROMONLY ";";
	if (sqlite3_prepare_v2(library->db, selectRom, -1, &library->selectRom, NULL)) {
		goto error;
	}

	static const char selectRoot[] = "SELECT rootid FROM roots WHERE path = ? AND CASE WHEN :useMtime THEN mtime <= :mtime ELSE 1 END;";
	if (sqlite3_prepare_v2(library->db, selectRoot, -1, &library->selectRoot, NULL)) {
		goto error;
	}

	static const char count[] = "SELECT count(pathid) FROM paths JOIN roots USING (rootid) JOIN roms USING (romid) WHERE " CONSTRAINTS ";";
	if (sqlite3_prepare_v2(library->db, count, -1, &library->count, NULL)) {
		goto error;
	}

	static const char select[] = "SELECT *, paths.path AS filename, roots.path AS base FROM paths JOIN roots USING (rootid) JOIN roms USING (romid) WHERE " CONSTRAINTS " LIMIT :count OFFSET :offset;";
	if (sqlite3_prepare_v2(library->db, select, -1, &library->select, NULL)) {
		goto error;
	}

	return library;

error:
	mLibraryDestroy(library);
	return NULL;
}

void mLibraryDestroy(struct mLibrary* library) {
	if (!library) {
		return;
	}

	sqlite3_finalize(library->insertPath);
	sqlite3_finalize(library->insertRom);
	sqlite3_finalize(library->insertRoot);
	sqlite3_finalize(library->deletePath);
	sqlite3_finalize(library->deleteRoot);
	sqlite3_finalize(library->selectRom);
	sqlite3_finalize(library->selectRoot);
	sqlite3_finalize(library->select);
	sqlite3_finalize(library->count);
	sqlite3_close(library->db);
	free(library);
}

void mLibraryLoadDirectory(struct mLibrary* library, const char* base, bool recursive) {
	struct VDir* dir = VDirOpenArchive(base);
	if (!dir) {
		dir = VDirOpen(base);
	}
	sqlite3_exec(library->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
	if (!dir) {
		sqlite3_clear_bindings(library->deleteRoot);
		sqlite3_reset(library->deleteRoot);
		sqlite3_bind_text(library->deleteRoot, 1, base, -1, SQLITE_TRANSIENT);
		sqlite3_step(library->deleteRoot);
		sqlite3_exec(library->db, "COMMIT;", NULL, NULL, NULL);
		return;
	}

	struct mLibraryEntry entry;
	memset(&entry, 0, sizeof(entry));
	entry.base = base;
	struct mLibraryListing entries;
	mLibraryListingInit(&entries, 0);
	mLibraryGetEntries(library, &entries, 0, 0, &entry);
	size_t i;
	for (i = 0; i < mLibraryListingSize(&entries); ++i) {
		struct mLibraryEntry* current = mLibraryListingGetPointer(&entries, i);
		struct VFile* vf = dir->openFile(dir, current->filename, O_RDONLY);
		_mLibraryDeleteEntry(library, current);
		if (!vf) {
			mLibraryEntryFree(current);
			continue;
		}
		_mLibraryAddEntry(library, current->filename, base, vf);
		mLibraryEntryFree(current);
	}
	mLibraryListingDeinit(&entries);

	dir->rewind(dir);
	struct VDirEntry* dirent = dir->listNext(dir);
	while (dirent) {
		const char* name = dirent->name(dirent);
		struct VFile* vf = dir->openFile(dir, name, O_RDONLY);
		bool wasAdded = false;

		if (vf) {
			wasAdded = _mLibraryAddEntry(library, name, base, vf);
		}
		if (!wasAdded && name[0] != '.') {
			char newBase[PATH_MAX];
			snprintf(newBase, sizeof(newBase), "%s" PATH_SEP "%s", base, name);

			if (recursive) {
				mLibraryLoadDirectory(library, newBase, recursive);
			} else if (dirent->type(dirent) == VFS_FILE) {
				mLibraryLoadDirectory(library, newBase, true); // This will add as an archive
			}
		}
		dirent = dir->listNext(dir);
	}
	dir->close(dir);
	sqlite3_exec(library->db, "COMMIT;", NULL, NULL, NULL);
}

bool _mLibraryAddEntry(struct mLibrary* library, const char* filename, const char* base, struct VFile* vf) {
	if (!vf) {
		return false;
	}
	struct mCore* core = mCoreFindVF(vf);
	if (!core) {
		vf->close(vf);
		return false;
	}
	struct mLibraryEntry entry;
	memset(&entry, 0, sizeof(entry));
	core->init(core);
	core->loadROM(core, vf);

	core->getGameTitle(core, entry.internalTitle);
	core->getGameCode(core, entry.internalCode);
	core->checksum(core, &entry.crc32, mCHECKSUM_CRC32);
	entry.platform = core->platform(core);
	entry.title = NULL;
	entry.base = base;
	entry.filename = filename;
	entry.filesize = vf->size(vf);
	_mLibraryInsertEntry(library, &entry);
	// Note: this destroys the VFile
	core->deinit(core);
	return true;
}

static void _mLibraryInsertEntry(struct mLibrary* library, struct mLibraryEntry* entry) {
	sqlite3_clear_bindings(library->selectRom);
	sqlite3_reset(library->selectRom);
	struct mLibraryEntry constraints = *entry;
	constraints.filename = NULL;
	constraints.base = NULL;
	_bindConstraints(library->selectRom, &constraints);
	sqlite3_int64 romId;
	if (sqlite3_step(library->selectRom) == SQLITE_DONE) {
		sqlite3_clear_bindings(library->insertRom);
		sqlite3_reset(library->insertRom);
		_bindConstraints(library->insertRom, entry);
		sqlite3_step(library->insertRom);
		romId = sqlite3_last_insert_rowid(library->db);
	} else {
		romId = sqlite3_column_int64(library->selectRom, 0);
	}

	sqlite3_int64 rootId = 0;
	if (entry->base) {
		sqlite3_clear_bindings(library->selectRoot);
		sqlite3_reset(library->selectRoot);
		sqlite3_bind_text(library->selectRoot, 1, entry->base, -1, SQLITE_TRANSIENT);
		if (sqlite3_step(library->selectRoot) == SQLITE_DONE) {
			sqlite3_clear_bindings(library->insertRoot);
			sqlite3_reset(library->insertRoot);
			sqlite3_bind_text(library->insertRoot, 1, entry->base, -1, SQLITE_TRANSIENT);
			sqlite3_step(library->insertRoot);
			rootId = sqlite3_last_insert_rowid(library->db);
		} else {
			rootId = sqlite3_column_int64(library->selectRoot, 0);
		}
	}

	sqlite3_clear_bindings(library->insertPath);
	sqlite3_reset(library->insertPath);
	sqlite3_bind_int64(library->insertPath, 1, romId);
	sqlite3_bind_text(library->insertPath, 2, entry->filename, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(library->insertPath, 3, entry->title, -1, SQLITE_TRANSIENT);
	if (rootId > 0) {
		sqlite3_bind_int64(library->insertPath, 4, rootId);
	}
	sqlite3_step(library->insertPath);
}

static void _mLibraryDeleteEntry(struct mLibrary* library, struct mLibraryEntry* entry) {
	sqlite3_clear_bindings(library->deletePath);
	sqlite3_reset(library->deletePath);
	sqlite3_bind_text(library->deletePath, 1, entry->filename, -1, SQLITE_TRANSIENT);
	sqlite3_step(library->insertPath);
}

void mLibraryClear(struct mLibrary* library) {
	sqlite3_exec(library->db,
		"   BEGIN TRANSACTION;"
		"\n DELETE FROM roots;"
		"\n DELETE FROM roms;"
		"\n DELETE FROM paths;"
		"\n COMMIT;"
		"\n VACUUM;", NULL, NULL, NULL);
}

size_t mLibraryCount(struct mLibrary* library, const struct mLibraryEntry* constraints) {
	sqlite3_clear_bindings(library->count);
	sqlite3_reset(library->count);
	_bindConstraints(library->count, constraints);
	if (sqlite3_step(library->count) != SQLITE_ROW) {
		return 0;
	}
	return sqlite3_column_int64(library->count, 0);
}

size_t mLibraryGetEntries(struct mLibrary* library, struct mLibraryListing* out, size_t numEntries, size_t offset, const struct mLibraryEntry* constraints) {
	mLibraryListingClear(out); // TODO: Free memory
	sqlite3_clear_bindings(library->select);
	sqlite3_reset(library->select);
	_bindConstraints(library->select, constraints);

	if (numEntries > SSIZE_MAX) {
		numEntries = SSIZE_MAX;
	}
	int countIndex = sqlite3_bind_parameter_index(library->select, ":count");
	int offsetIndex = sqlite3_bind_parameter_index(library->select, ":offset");
	sqlite3_bind_int64(library->select, countIndex, numEntries ? (ssize_t) numEntries : -1);
	sqlite3_bind_int64(library->select, offsetIndex, offset);

	size_t entryIndex;
	for (entryIndex = 0; (!numEntries || entryIndex < numEntries) && sqlite3_step(library->select) == SQLITE_ROW; ++entryIndex) {
		struct mLibraryEntry* entry = mLibraryListingAppend(out);
		memset(entry, 0, sizeof(*entry));
		int nCols = sqlite3_column_count(library->select);
		int i;
		for (i = 0; i < nCols; ++i) {
			const char* colName = sqlite3_column_name(library->select, i);
			if (strcmp(colName, "crc32") == 0) {
				entry->crc32 = sqlite3_column_int(library->select, i);
				struct NoIntroGame game;
				if (NoIntroDBLookupGameByCRC(library->gameDB, entry->crc32, &game)) {
					entry->title = strdup(game.name);
				}
			} else if (strcmp(colName, "platform") == 0) {
				entry->platform = sqlite3_column_int(library->select, i);
			} else if (strcmp(colName, "size") == 0) {
				entry->filesize = sqlite3_column_int64(library->select, i);
			} else if (strcmp(colName, "internalCode") == 0 && sqlite3_column_type(library->select, i) == SQLITE_TEXT) {
				strncpy(entry->internalCode, (const char*) sqlite3_column_text(library->select, i), sizeof(entry->internalCode) - 1);
			} else if (strcmp(colName, "internalTitle") == 0 && sqlite3_column_type(library->select, i) == SQLITE_TEXT) {
				strncpy(entry->internalTitle, (const char*) sqlite3_column_text(library->select, i), sizeof(entry->internalTitle) - 1);
			} else if (strcmp(colName, "filename") == 0) {
				entry->filename = strdup((const char*) sqlite3_column_text(library->select, i));
			} else if (strcmp(colName, "base") == 0) {
				entry->base =  strdup((const char*) sqlite3_column_text(library->select, i));
			}
		}
	}
	return mLibraryListingSize(out);
}

void mLibraryEntryFree(struct mLibraryEntry* entry) {
	free((void*) entry->title);
	free((void*) entry->filename);
	free((void*) entry->base);
}

struct VFile* mLibraryOpenVFile(struct mLibrary* library, const struct mLibraryEntry* entry) {
	struct mLibraryListing entries;
	mLibraryListingInit(&entries, 0);
	if (!mLibraryGetEntries(library, &entries, 0, 0, entry)) {
		mLibraryListingDeinit(&entries);
		return NULL;
	}
	struct VFile* vf = NULL;
	size_t i;
	for (i = 0; i < mLibraryListingSize(&entries); ++i) {
		struct mLibraryEntry* e = mLibraryListingGetPointer(&entries, i);
		struct VDir* dir = VDirOpenArchive(e->base);
		bool isArchive = true;
		if (!dir) {
			dir = VDirOpen(e->base);
			isArchive = false;
		}
		if (!dir) {
			continue;
		}
		vf = dir->openFile(dir, e->filename, O_RDONLY);
		if (vf && isArchive) {
			struct VFile* vfclone = VFileMemChunk(NULL, vf->size(vf));
			uint8_t buffer[2048];
			ssize_t read;
			while ((read = vf->read(vf, buffer, sizeof(buffer))) > 0) {
				vfclone->write(vfclone, buffer, read);
			}
			vf->close(vf);
			vf = vfclone;
		}
		dir->close(dir);
		if (vf) {
			break;
		}
	}
	for (i = 0; i < mLibraryListingSize(&entries); ++i) {
		struct mLibraryEntry* e = mLibraryListingGetPointer(&entries, i);
		mLibraryEntryFree(e);
	}
	mLibraryListingDeinit(&entries);
	return vf;
}

void mLibraryAttachGameDB(struct mLibrary* library, const struct NoIntroDB* db) {
	library->gameDB = db;
}

#endif
