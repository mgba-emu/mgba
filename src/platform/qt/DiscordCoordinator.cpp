/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DiscordCoordinator.h"

#include "CoreController.h"
#include "GBAApp.h"

#ifdef USE_SQLITE3
#include "feature/sqlite3/no-intro.h"
#endif

#include "discord_rpc.h"

namespace QGBA {

namespace DiscordCoordinator {

static bool s_gameRunning = false;
static bool s_inited = false;
static QString s_title;

static void updatePresence() {
	if (!s_inited) {
		return;
	}
	if (s_gameRunning) {
		DiscordRichPresence discordPresence{};
		discordPresence.details = s_title.toUtf8().constData();
		discordPresence.instance = 1;
		Discord_UpdatePresence(&discordPresence);
	} else {
		Discord_ClearPresence();
	}
}

void init() {
	if (s_inited) {
		return;
	}
	DiscordEventHandlers handlers{};
	Discord_Initialize("554440738952183828", &handlers, 1, nullptr);
	s_inited = true;
	updatePresence();
}

void deinit() {
	if (!s_inited) {
		return;
	}
	Discord_ClearPresence();
	Discord_Shutdown();
	s_inited = false;
	s_gameRunning = false;
}

void gameStarted(std::shared_ptr<CoreController> controller) {
	if (s_gameRunning) {
		return;
	}
	s_gameRunning = true;

	CoreController::Interrupter interrupter(controller);
	mCore *core = controller->thread()->core;
	s_title = core->dirs.baseName;

#ifdef USE_SQLITE3
	const NoIntroDB* db = GBAApp::app()->gameDB();
	NoIntroGame game{};
	uint32_t crc32 = 0;
	core->checksum(core, &crc32, CHECKSUM_CRC32);

	if (db && crc32 && NoIntroDBLookupGameByCRC(db, crc32, &game)) {
		s_title = QLatin1String(game.name);
	}
#endif

	updatePresence();
}

void gameStopped() {
	if (!s_gameRunning) {
		return;
	}
	s_gameRunning = false;
	updatePresence();
}

}

}