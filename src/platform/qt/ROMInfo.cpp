/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ROMInfo.h"

#include "GBAApp.h"
#include "GameController.h"

#include "core/core.h"
#ifdef M_CORE_GB
#include "gb/gb.h"
#endif
#ifdef M_CORE_GBA
#include "gba/gba.h"
#endif
#include "util/nointro.h"

using namespace QGBA;

ROMInfo::ROMInfo(GameController* controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);

	if (!controller->isLoaded()) {
		return;
	}

	const NoIntroDB* db = GBAApp::app()->gameDB();
	uint32_t crc32 = 0;

	GameController::Interrupter interrupter(controller);
	mCore* core = controller->thread()->core;
	char title[17] = {};
	core->getGameTitle(core, title);
	m_ui.title->setText(QLatin1String(title));
	title[8] = '\0';
	core->getGameCode(core, title);
	if (title[0]) {
		m_ui.id->setText(QLatin1String(title));
	} else {
		m_ui.id->setText(tr("(unknown)"));
	}

	switch (controller->thread()->core->platform(controller->thread()->core)) {
#ifdef M_CORE_GBA
	case PLATFORM_GBA: {
		GBA* gba = static_cast<GBA*>(core->board);
		m_ui.size->setText(QString::number(gba->pristineRomSize) + tr(" bytes"));
		crc32 = gba->romCrc32;
		break;
	}
#endif
#ifdef M_CORE_GB
	case PLATFORM_GB: {
		GB* gb = static_cast<GB*>(core->board);
		m_ui.size->setText(QString::number(gb->pristineRomSize) + tr(" bytes"));
		crc32 = gb->romCrc32;
		break;
	}
#endif
	default:
		m_ui.size->setText(tr("(unknown)"));
		break;
	}
	if (crc32) {
		m_ui.crc->setText(QString::number(crc32, 16));
		if (db) {
			NoIntroGame game{};
			if (NoIntroDBLookupGameByCRC(db, crc32, &game)) {
				m_ui.name->setText(game.name);
			} else {
				m_ui.name->setText(tr("(unknown)"));
			}
		} else {
			m_ui.name->setText(tr("(no database present)"));
		}
	} else {
		m_ui.crc->setText(tr("(unknown)"));
		m_ui.name->setText(tr("(unknown)"));
	}
}
