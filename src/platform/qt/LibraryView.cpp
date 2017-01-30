/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryView.h"

#include <mgba-util/vfs.h>

#include "ConfigController.h"
#include "GBAApp.h"

using namespace QGBA;

LibraryView::LibraryView(QWidget* parent)
	: QWidget(parent)
	, m_model(ConfigController::configDir() + "/library.sqlite3")
{
	m_ui.setupUi(this);
	m_model.attachGameDB(GBAApp::app()->gameDB());
	connect(&m_model, SIGNAL(doneLoading()), this, SIGNAL(doneLoading()));
	connect(&m_model, SIGNAL(doneLoading()), this, SLOT(resizeColumns()));
	connect(m_ui.listing, SIGNAL(activated(const QModelIndex&)), this, SIGNAL(accepted()));
	m_ui.listing->horizontalHeader()->setSectionsMovable(true);
	m_ui.listing->setModel(&m_model);
	m_ui.listing->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	resizeColumns();
}

void LibraryView::setDirectory(const QString& filename) {
	m_model.loadDirectory(filename);
	m_model.constrainBase(filename);
}

void LibraryView::addDirectory(const QString& filename) {
	m_model.loadDirectory(filename);
}

VFile* LibraryView::selectedVFile() const {
	QModelIndex index = m_ui.listing->selectionModel()->currentIndex();
	if (!index.isValid()) {
		return nullptr;
	}
	return m_model.openVFile(index);
}

QPair<QString, QString> LibraryView::selectedPath() const {
	QModelIndex index = m_ui.listing->selectionModel()->currentIndex();
	if (!index.isValid()) {
		return qMakePair(QString(), QString());
	}
	return qMakePair(m_model.filename(index), m_model.location(index));
}

void  LibraryView::resizeColumns() {
	m_ui.listing->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
}
