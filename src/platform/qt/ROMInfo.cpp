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

template<size_t N> bool isZeroed(const uint8_t* mem) {
	for (size_t i = 0; i < N; ++i) {
		if (mem[i]) {
			return false;
		}
	}
	return true;
}

ROMInfo::ROMInfo(std::shared_ptr<CoreController> controller, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);

#ifdef USE_SQLITE3
	const NoIntroDB* db = GBAApp::app()->gameDB();
#endif
	uint32_t crc32 = 0;
	uint8_t md5[16]{};
	uint8_t sha1[20]{};

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
	core->checksum(core, &sha1, mCHECKSUM_SHA1);

	m_ui.size->setText(QString::number(core->romSize(core)) + tr(" bytes"));

	if (crc32) {
		m_ui.crc->setText(QString::number(crc32, 16));
	} else {
		m_ui.crc->setText(tr("(unknown)"));
	}

	if (!isZeroed<16>(md5)) {
		m_ui.md5->setText(QString::asprintf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			md5[0x0], md5[0x1], md5[0x2], md5[0x3], md5[0x4], md5[0x5], md5[0x6], md5[0x7],
			md5[0x8], md5[0x9], md5[0xA], md5[0xB], md5[0xC], md5[0xD], md5[0xE], md5[0xF]));
	} else {
		m_ui.md5->setText(tr("(unknown)"));
	}

	if (!isZeroed<20>(sha1)) {
		m_ui.sha1->setText(QString::asprintf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
			sha1[ 0], sha1[ 1], sha1[ 2], sha1[ 3], sha1[ 4], sha1[ 5], sha1[ 6], sha1[ 7], sha1[ 8], sha1[ 9],
			sha1[10], sha1[11], sha1[12], sha1[13], sha1[14], sha1[15], sha1[16], sha1[17], sha1[18], sha1[19]));
	} else {
		m_ui.sha1->setText(tr("(unknown)"));
	}

#ifdef USE_SQLITE3
	if (db) {
		NoIntroGame game{};
		if (!isZeroed<20>(sha1) && NoIntroDBLookupGameBySHA1(db, sha1, &game)) {
			m_ui.name->setText(game.name);
		} else if (crc32 && NoIntroDBLookupGameByCRC(db, crc32, &game)) {
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

	QString savePath = controller->savePath();
	if (!savePath.isEmpty()) {
		m_ui.savefile->setText(savePath);
	} else {
		m_ui.savefile->setText(tr("(unknown)"));
	}
}
