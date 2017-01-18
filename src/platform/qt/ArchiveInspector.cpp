/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ArchiveInspector.h"

#include <mgba-util/vfs.h>

#include "ConfigController.h"

using namespace QGBA;

ArchiveInspector::ArchiveInspector(const QString& filename, QWidget* parent)
	: QDialog(parent)
	, m_model(ConfigController::configDir() + "/library.sqlite3")
{
	m_ui.setupUi(this);
	m_model.loadDirectory(filename);
	m_model.constrainBase(filename);
	m_ui.archiveListing->setModel(&m_model);
}

VFile* ArchiveInspector::selectedVFile() const {
	QModelIndex index = m_ui.archiveListing->selectionModel()->currentIndex();
	if (!index.isValid()) {
		return nullptr;
	}
	return m_model.openVFile(index);
}
