/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <QDialog>
#ifndef QBGA_ABOUT_DIALOG
#define QGBA_ABOUT_DIALOG
#include "ui_AboutDialog.h"

namespace QGBA {

class AboutDialog : public QDialog {
Q_OBJECT

public:
	AboutDialog(QDialog* parent = nullptr);

private:
	Ui::AboutDialog m_ui;
};

}

#endif
