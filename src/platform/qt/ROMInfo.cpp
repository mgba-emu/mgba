/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ROMInfo.h"

#include "GameController.h"

using namespace QGBA;

ROMInfo::ROMInfo(GameController* controller, QWidget* parent) {
	m_ui.setupUi(this);

	if (!controller->isLoaded()) {
		return;
	}

	controller->threadInterrupt();
	GBA* gba = controller->thread()->gba;
	char title[13] = {};
	GBAGetGameCode(gba, title);
	m_ui.id->setText(QLatin1String(title));
	GBAGetGameTitle(gba, title);
	m_ui.title->setText(QLatin1String(title));
	m_ui.size->setText(QString::number(gba->pristineRomSize));
	m_ui.crc->setText(QString::number(gba->romCrc32, 16));
	controller->threadContinue();
}
