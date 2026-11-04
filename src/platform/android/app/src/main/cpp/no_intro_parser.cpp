/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

extern "C" {
#include "feature/sqlite3/no-intro.h"
#include <mgba-util/vfs.h>
}

#include "no_intro_parser.h"
#include "log/log.h"
#include <cstring>

static NoIntroDB* g_noIntroDB = nullptr;

static bool fillMetadata(
    const NoIntroGame& game,
    NoIntroMetadata& out) {

	if (!game.name) {
		return false;
	}

	out.name = game.name;
	out.romName = game.romName ? game.romName : "";
	out.size = game.size;
	out.crc32 = game.crc32;
	out.verified = game.verified;

	return true;
}

bool noIntroInit(const char* databasePath, const char* datPath) {
	noIntroShutdown();

	g_noIntroDB = NoIntroDBLoad(databasePath);
	if (!g_noIntroDB) {
		return false;
	}

	if (datPath && datPath[0] == '\0') {
		return true;
	}

	VFile* datFile = VFileOpen(datPath, O_RDONLY);
	if (!datFile) {
		noIntroShutdown();
		return false;
	}

	if (!NoIntroDBLoadClrMamePro(g_noIntroDB, datFile)) {
		datFile->close(datFile);
		noIntroShutdown();
		return false;
	}

	datFile->close(datFile);
	return true;
}

void noIntroShutdown() {
	if (g_noIntroDB) {
		NoIntroDBDestroy(g_noIntroDB);
		g_noIntroDB = nullptr;
	}
}

[[nodiscard]] bool noIntroLookupCRC32(uint32_t crc32, NoIntroMetadata& out) {
	if (!g_noIntroDB) {
		return false;
	}

	NoIntroGame game{};
	if (!NoIntroDBLookupGameByCRC(g_noIntroDB, crc32, &game)) {
		return false;
	}

	return fillMetadata(game, out);
}

[[maybe_unused]] bool noIntroLookupMD5(const uint8_t md5[16], NoIntroMetadata& out) {
	if (!g_noIntroDB) {
		return false;
	}

	NoIntroGame game{};
	if (!NoIntroDBLookupGameByMD5(g_noIntroDB, md5, &game)) {
		return false;
	}

	return fillMetadata(game, out);
}

[[maybe_unused]] bool noIntroLookupSHA1(const uint8_t sha1[20], NoIntroMetadata& out) {
	if (!g_noIntroDB) {
		return false;
	}

	NoIntroGame game{};
	if (!NoIntroDBLookupGameBySHA1(g_noIntroDB, sha1, &game)) {
		return false;
	}

	return fillMetadata(game, out);
}