/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "AboutDialog.h"

using namespace QGBA;

AboutDialog::AboutDialog(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);
	setFixedSize(size());
	m_ui.projectName->setText(PROJECT_NAME);
	m_ui.projectVersion->setText(PROJECT_VERSION);
}
