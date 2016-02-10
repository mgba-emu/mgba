/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ROMInfo.h"

#include "GBAApp.h"
#include "GameController.h"

extern "C" {
#include "core/core.h"
#include "gba/gba.h"
#include "util/nointro.h"
}

using namespace QGBA;

ROMInfo::ROMInfo(GameController* controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);

	if (!controller->isLoaded()) {
		return;
	}

	const NoIntroDB* db = GBAApp::app()->gameDB();

	controller->threadInterrupt();
	mCore* core = controller->thread()->core;
	GBA* gba = static_cast<GBA*>(core->board);
	char title[17] = {};
	GBAGetGameCode(gba, title);
	m_ui.id->setText(QLatin1String(title));
	core->getGameTitle(core, title);
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
