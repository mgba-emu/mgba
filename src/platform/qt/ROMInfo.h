/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_ROM_INFO
#define QGBA_ROM_INFO

#include <QWidget>

#include "ui_ROMInfo.h"

namespace QGBA {

class GameController;

class ROMInfo : public QDialog {
Q_OBJECT

public:
	ROMInfo(GameController* controller, QWidget* parent = nullptr);

private:
	Ui::ROMInfo m_ui;
};

}

#endif
