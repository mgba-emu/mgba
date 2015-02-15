/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheatsView.h"

#include <QFileDialog>

using namespace QGBA;

CheatsView::CheatsView(GBACheatDevice* device, QWidget* parent)
	: QWidget(parent)
	, m_model(device)
{
	m_ui.setupUi(this);

	m_ui.cheatList->setModel(&m_model);

	connect(m_ui.load, SIGNAL(clicked()), this, SLOT(load()));
}

void CheatsView::load() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Select cheats file"));
	if (!filename.isEmpty()) {
		m_model.loadFile(filename);
	}
}