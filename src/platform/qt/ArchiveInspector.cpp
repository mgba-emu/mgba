/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ArchiveInspector.h"

#include "util/vfs.h"

using namespace QGBA;

ArchiveInspector::ArchiveInspector(const QString& filename, QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);
	m_dir = VDirOpenArchive(filename.toUtf8().constData());
	if (m_dir) {
		m_model.loadDirectory(m_dir);
	}
	m_ui.archiveListing->setModel(&m_model);
}

ArchiveInspector::~ArchiveInspector() {
	if (m_dir) {
		m_dir->close(m_dir);
	}
}

VFile* ArchiveInspector::selectedVFile() const {
	QModelIndex index = m_ui.archiveListing->selectionModel()->currentIndex();
	if (!index.isValid()) {
		return nullptr;
	}
	return m_dir->openFile(m_dir, m_model.entryAt(index.row())->filename, O_RDONLY);
}
