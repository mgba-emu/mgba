/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ROMInfo.h"

#include "GBAApp.h"
#include "GameController.h"

extern "C" {
#include "util/nointro.h"
}

using namespace QGBA;

ROMInfo::ROMInfo(GameController* controller, QWidget* parent) {
	m_ui.setupUi(this);

	if (!controller->isLoaded()) {
		return;
	}

	const NoIntroDB* db = GBAApp::app()->noIntroDB();

	controller->threadInterrupt();
	GBA* gba = controller->thread()->gba;
	char title[13] = {};
	GBAGetGameCode(gba, title);
	m_ui.id->setText(QLatin1String(title));
	GBAGetGameTitle(gba, title);
	m_ui.title->setText(QLatin1String(title));
	m_ui.size->setText(QString::number(gba->pristineRomSize));
	m_ui.crc->setText(QString::number(gba->romCrc32, 16));
	if (db) {
		NoIntroGame game;
		if (NoIntroDBLookupGameByCRC(db, gba->romCrc32, &game)) {
			m_ui.name->setText(game.name);
		} else {
			m_ui.name->setText(tr("(unknown)"));
		}
	} else {
		m_ui.name->setText(tr("(no database present)"));
	}
	controller->threadContinue();
}
