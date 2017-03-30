/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryGrid.h"

#include "ui_LibraryGrid.h"

namespace QGBA {

LibraryGrid::LibraryGrid(QWidget* parent)
    : QListWidget(parent)
    , m_ui(new Ui::LibraryGrid)
{
	m_ui->setupUi(this);
	setViewStyle(LibraryStyle::STYLE_GRID);

	connect(this, &QListWidget::itemActivated, this, &LibraryGrid::startGame);
}

LibraryEntryRef LibraryGrid::selectedEntry() {
	if (!selectedItems().empty()) {
		return m_items.key(selectedItems().at(0));
	} else {
		return LibraryEntryRef();
	}
}

void LibraryGrid::selectEntry(LibraryEntryRef game) {
	if (!game) {
		return;
	}
	if (!selectedItems().empty()) {
		selectedItems().at(0)->setSelected(false);
	}
	m_items.value(game)->setSelected(true);
}

void LibraryGrid::setViewStyle(LibraryStyle newStyle) {
	if (newStyle == LibraryStyle::STYLE_GRID) {
		m_currentStyle = LibraryStyle::STYLE_GRID;
		setIconSize(QSize(GRID_BANNER_WIDTH, GRID_BANNER_HEIGHT));
		setViewMode(QListView::IconMode);
	} else {
		m_currentStyle = LibraryStyle::STYLE_ICON;
		setIconSize(QSize(ICON_BANNER_WIDTH, ICON_BANNER_HEIGHT));
		setViewMode(QListView::ListMode);
	}

	// QListView resets this when you change the view mode, so let's set it again
	setDragEnabled(false);
}

void LibraryGrid::addEntry(LibraryEntryRef item) {
	if (m_items.contains(item)) {
		return;
	}

	QListWidgetItem* i = new QListWidgetItem;
	//i->setIcon(QIcon(item.bitmap
	//	.scaled(GRID_BANNER_WIDTH, GRID_BANNER_HEIGHT, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
	i->setText(item->displayTitle());

	addItem(i);
	m_items.insert(item, i);
}

void LibraryGrid::removeEntry(LibraryEntryRef entry) {
	if (!m_items.contains(entry)) {
		return;
	}

	delete m_items.take(entry);
}

}
