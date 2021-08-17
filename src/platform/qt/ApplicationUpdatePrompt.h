/* Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>

#include "ui_ApplicationUpdatePrompt.h"

namespace QGBA {

class ApplicationUpdatePrompt : public QDialog {
Q_OBJECT

public:
	ApplicationUpdatePrompt(QWidget* parent = nullptr);

private slots:
	void startUpdate();
	void promptRestart();

private:
	Ui::ApplicationUpdatePrompt m_ui;
	QMetaObject::Connection m_okDownload;
};

}
