/* Copyright (c) 2014-2017 waddlesplash
 * Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryGrid.h"

namespace QGBA {

LibraryGrid::LibraryGrid(LibraryController* parent)
    : m_widget(new QListWidget(parent))
{
	m_widget->setObjectName("LibraryGrid");
	m_widget->setWrapping(true);
	m_widget->setResizeMode(QListView::Adjust);
	m_widget->setUniformItemSizes(true);
	setViewStyle(LibraryStyle::STYLE_GRID);

	QObject::connect(m_widget, &QListWidget::itemActivated, parent, &LibraryController::startGame);
}

LibraryGrid::~LibraryGrid() {
	delete m_widget;
}

QString LibraryGrid::selectedEntry() {
	if (!m_widget->selectedItems().empty()) {
		return m_items.key(m_widget->selectedItems().at(0));
	} else {
		return {};
	}
}

void LibraryGrid::selectEntry(const QString& game) {
	m_widget->setCurrentItem(m_items.value(game));
}

void LibraryGrid::setViewStyle(LibraryStyle newStyle) {
	if (newStyle == LibraryStyle::STYLE_GRID) {
		m_currentStyle = LibraryStyle::STYLE_GRID;
		m_widget->setIconSize(QSize(GRID_BANNER_WIDTH, GRID_BANNER_HEIGHT));
		m_widget->setViewMode(QListView::IconMode);
	} else {
		m_widget->setIconSize(QSize(ICON_BANNER_WIDTH, ICON_BANNER_HEIGHT));
		m_widget->setViewMode(QListView::ListMode);
	}
	m_currentStyle = newStyle;

	// QListView resets this when you change the view mode, so let's set it again
	m_widget->setDragEnabled(false);
}

void LibraryGrid::resetEntries(const QList<LibraryEntry>& items) {
	m_widget->clear();
	m_items.clear();
	addEntries(items);
}

void LibraryGrid::addEntries(const QList<LibraryEntry>& items) {
	for (const auto& item : items) {
		addEntry(item);
	}
}

void LibraryGrid::addEntry(const LibraryEntry& item) {
	if (m_items.contains(item.fullpath)) {
		return;
	}

	QListWidgetItem* i = new QListWidgetItem;
	i->setText(m_showFilename ? item.filename : item.displayTitle());
	m_widget->addItem(i);
	m_items.insert(item.fullpath, i);
}

void LibraryGrid::updateEntries(const QList<LibraryEntry>& items) {
	for (const auto& item : items) {
		updateEntry(item);
	}
}

void LibraryGrid::updateEntry(const LibraryEntry& item) {
	QListWidgetItem* i = m_items.value(item.fullpath);
	i->setText(m_showFilename ? item.filename : item.displayTitle());
}

void LibraryGrid::removeEntries(const QList<QString>& items) {
	for (const auto& item : items) {
		removeEntry(item);
	}
}

void LibraryGrid::removeEntry(const QString& item) {
	if (!m_items.contains(item)) {
		return;
	}

	delete m_items.take(item);
}

}
