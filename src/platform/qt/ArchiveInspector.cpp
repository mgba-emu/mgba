/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ArchiveInspector.h"

#include <mgba-util/vfs.h>

using namespace QGBA;

ArchiveInspector::ArchiveInspector(const QString& filename, QWidget* parent)
	: QDialog(parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint)
{
	m_ui.setupUi(this);
	connect(m_ui.archiveView, &LibraryController::doneLoading, [this]() {
		m_ui.loading->hide();
	});
	connect(m_ui.archiveView, &LibraryController::startGame, this, &ArchiveInspector::accepted);
	m_ui.archiveView->setViewStyle(LibraryStyle::STYLE_LIST);
	m_ui.archiveView->addDirectory(filename);
}

VFile* ArchiveInspector::selectedVFile() const {
	return m_ui.archiveView->selectedVFile();
}

QPair<QString, QString> ArchiveInspector::selectedPath() const {
	return m_ui.archiveView->selectedPath();
}
