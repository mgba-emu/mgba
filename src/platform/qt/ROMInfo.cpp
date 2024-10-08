/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ROMInfo.h"

#include "GBAApp.h"
#include "CoreController.h"

#include <mgba/core/core.h>
#ifdef USE_SQLITE3
#include "feature/sqlite3/no-intro.h"
#endif

using namespace QGBA;

ROMInfo::ROMInfo(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);

#ifdef USE_SQLITE3
	const NoIntroDB* db = GBAApp::app()->gameDB();
#endif
	uint32_t crc32 = 0;
	uint8_t md5[16]{};

	CoreController::Interrupter interrupter(controller);
	mCore* core = controller->thread()->core;
	mGameInfo info;
	core->getGameInfo(core, &info);
	m_ui.title->setText(QLatin1String(info.title));
	if (info.code[0]) {
		m_ui.id->setText(QLatin1String(info.code));
	} else {
		m_ui.id->setText(tr("(unknown)"));
	}
	m_ui.maker->setText(QLatin1String(info.maker));
	m_ui.version->setText(QString::number(info.version));

	core->checksum(core, &crc32, mCHECKSUM_CRC32);
	core->checksum(core, &md5, mCHECKSUM_MD5);

	m_ui.size->setText(QString::number(core->romSize(core)) + tr(" bytes"));

	if (crc32) {
		m_ui.crc->setText(QString::number(crc32, 16));
#ifdef USE_SQLITE3
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
#else
		m_ui.name->hide();
#endif
	} else {
		m_ui.crc->setText(tr("(unknown)"));
		m_ui.name->setText(tr("(unknown)"));
	}

	m_ui.md5->setText(QString::asprintf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		md5[0x0], md5[0x1], md5[0x2], md5[0x3], md5[0x4], md5[0x5], md5[0x6], md5[0x7],
		md5[0x8], md5[0x9], md5[0xA], md5[0xB], md5[0xC], md5[0xD], md5[0xE], md5[0xF]));

	QString savePath = controller->savePath();
	if (!savePath.isEmpty()) {
		m_ui.savefile->setText(savePath);
	} else {
		m_ui.savefile->setText(tr("(unknown)"));
	}
}
