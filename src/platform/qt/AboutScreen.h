/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>

#include "ui_AboutScreen.h"

namespace QGBA {

class AboutScreen : public QDialog {
Q_OBJECT

public:
	AboutScreen(QWidget* parent = nullptr);

private:
	Ui::AboutScreen m_ui;
};

}
