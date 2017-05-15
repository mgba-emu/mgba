/* Copyright (c) 2014-2017 waddlesplash
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

LibraryEntryRef LibraryGrid::selectedEntry() {
	if (!m_widget->selectedItems().empty()) {
		return m_items.key(m_widget->selectedItems().at(0));
	} else {
		return LibraryEntryRef();
	}
}

void LibraryGrid::selectEntry(LibraryEntryRef game) {
	if (!game) {
		return;
	}
	if (!m_widget->selectedItems().empty()) {
		m_widget->selectedItems().at(0)->setSelected(false);
	}
	m_items.value(game)->setSelected(true);
}

void LibraryGrid::setViewStyle(LibraryStyle newStyle) {
	if (newStyle == LibraryStyle::STYLE_GRID) {
		m_currentStyle = LibraryStyle::STYLE_GRID;
		m_widget->setIconSize(QSize(GRID_BANNER_WIDTH, GRID_BANNER_HEIGHT));
		m_widget->setViewMode(QListView::IconMode);
	} else {
		m_currentStyle = LibraryStyle::STYLE_ICON;
		m_widget->setIconSize(QSize(ICON_BANNER_WIDTH, ICON_BANNER_HEIGHT));
		m_widget->setViewMode(QListView::ListMode);
	}

	// QListView resets this when you change the view mode, so let's set it again
	m_widget->setDragEnabled(false);
}

void LibraryGrid::addEntry(LibraryEntryRef item) {
	if (m_items.contains(item)) {
		return;
	}

	QListWidgetItem* i = new QListWidgetItem;
	i->setText(item->displayTitle());

	m_widget->addItem(i);
	m_items.insert(item, i);
}

void LibraryGrid::removeEntry(LibraryEntryRef entry) {
	if (!m_items.contains(entry)) {
		return;
	}

	delete m_items.take(entry);
}

}
