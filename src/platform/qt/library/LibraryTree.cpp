/* Copyright (c) 2014-2017 waddlesplash
 * Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryTree.h"

#include "utils.h"

#include <QApplication>
#include <QDir>

using namespace QGBA;

namespace QGBA {

class LibraryTreeItem : public QTreeWidgetItem {
public:
	LibraryTreeItem(QTreeWidget* parent = nullptr) : QTreeWidgetItem(parent) {}
	void setFilesize(size_t size);

	virtual bool operator<(const QTreeWidgetItem& other) const override;
protected:
	size_t m_size = 0;
};

}

void LibraryTreeItem::setFilesize(size_t size) {
	m_size = size;
	setText(LibraryTree::COL_SIZE, niceSizeFormat(size));
}

bool LibraryTreeItem::operator<(const QTreeWidgetItem& other) const {
	const int column = treeWidget()->sortColumn();
	return ((column == LibraryTree::COL_SIZE) ?
		m_size < dynamic_cast<const LibraryTreeItem*>(&other)->m_size :
		QTreeWidgetItem::operator<(other));
}

LibraryTree::LibraryTree(LibraryController* parent)
	: m_widget(new QTreeWidget(parent))
	, m_controller(parent)
{
	m_widget->setObjectName("LibraryTree");
	m_widget->setSortingEnabled(true);
	m_widget->setAlternatingRowColors(true);

	QTreeWidgetItem* header = new QTreeWidgetItem({
		QApplication::translate("QGBA::LibraryTree", "Name", nullptr),
		QApplication::translate("QGBA::LibraryTree", "Location", nullptr),
		QApplication::translate("QGBA::LibraryTree", "Platform", nullptr),
		QApplication::translate("QGBA::LibraryTree", "Size", nullptr),
		QApplication::translate("QGBA::LibraryTree", "CRC32", nullptr),
	});
	header->setTextAlignment(3, Qt::AlignTrailing | Qt::AlignVCenter);
	m_widget->setHeaderItem(header);

	setViewStyle(LibraryStyle::STYLE_TREE);
	m_widget->sortByColumn(COL_NAME, Qt::AscendingOrder);

	QObject::connect(m_widget, &QTreeWidget::itemActivated, [this](QTreeWidgetItem* item, int) -> void {
		if (m_items.values().contains(item)) {
			emit m_controller->startGame();
		}
	});
}

LibraryTree::~LibraryTree() {
	m_widget->clear();
}

void LibraryTree::resizeAllCols() {
	for (int i = 0; i < m_widget->columnCount(); i++) {
		m_widget->resizeColumnToContents(i);
	}
}

QString LibraryTree::selectedEntry() {
	if (!m_widget->selectedItems().empty()) {
		return m_items.key(m_widget->selectedItems().at(0));
	} else {
		return {};
	}
}

void LibraryTree::selectEntry(const QString& game) {
	if (game.isNull()) {
		return;
	}
	m_widget->setCurrentItem(m_items.value(game));
}

void LibraryTree::setViewStyle(LibraryStyle newStyle) {
	if (newStyle == LibraryStyle::STYLE_LIST) {
		m_widget->setIndentation(0);
	} else {
		m_widget->setIndentation(20);
	}
	m_currentStyle = newStyle;
	rebuildTree();
}

void LibraryTree::resetEntries(const QList<LibraryEntry>& items) {
	m_deferredTreeRebuild = true;
	m_entries.clear();
	m_pathNodes.clear();
	addEntries(items);
}

void LibraryTree::addEntries(const QList<LibraryEntry>& items) {
	m_deferredTreeRebuild = true;
	for (const auto& item : items) {
		addEntry(item);
	}
	m_deferredTreeRebuild = false;
	rebuildTree();
}

void LibraryTree::addEntry(const LibraryEntry& item) {
	m_entries[item.fullpath] = item;

	QString folder = item.base;
	if (!m_pathNodes.contains(folder)) {
		m_pathNodes.insert(folder, 1);
	} else {
		++m_pathNodes[folder];
	}

	rebuildTree();
}

void LibraryTree::updateEntries(const QList<LibraryEntry>& items) {
	for (const auto& item : items) {
		updateEntry(item);
	}
}

void LibraryTree::updateEntry(const LibraryEntry& item) {
	m_entries[item.fullpath] = item;

	LibraryTreeItem* i = static_cast<LibraryTreeItem*>(m_items.value(item.fullpath));
	i->setText(COL_NAME, m_showFilename ? item.filename : item.displayTitle());
	i->setText(COL_PLATFORM, nicePlatformFormat(item.platform));
	i->setFilesize(item.filesize);
	i->setText(COL_CRC32, QString("%0").arg(item.crc32, 8, 16, QChar('0')));
}

void LibraryTree::removeEntries(const QList<QString>& items) {
	m_deferredTreeRebuild = true;
	for (const auto& item : items) {
		removeEntry(item);
	}
	m_deferredTreeRebuild = false;
	rebuildTree();
}

void LibraryTree::removeEntry(const QString& item) {
	if (!m_entries.contains(item)) {
		return;
	}
	QString folder = m_entries.value(item).base;
	--m_pathNodes[folder];
	if (m_pathNodes[folder] <= 0) {
		m_pathNodes.remove(folder);
	}

	m_entries.remove(item);
	rebuildTree();
}

void LibraryTree::rebuildTree() {
	if (m_deferredTreeRebuild) {
		return;
	}

	QString currentGame = selectedEntry();
	m_widget->clear();
	m_items.clear();

	QHash<QString, QTreeWidgetItem*> pathNodes;
	if (m_currentStyle == LibraryStyle::STYLE_TREE) {
		for (const QString& folder : m_pathNodes.keys()) { 
			QTreeWidgetItem* i = new LibraryTreeItem;
			pathNodes.insert(folder, i);
			i->setText(0, folder.section("/", -1));
			m_widget->addTopLevelItem(i);
		}
	}

	for (const auto& item : m_entries.values()) {
		LibraryTreeItem* i = new LibraryTreeItem;
		i->setText(COL_NAME, item.displayTitle());
		i->setText(COL_LOCATION, QDir::toNativeSeparators(item.base));
		i->setText(COL_PLATFORM, nicePlatformFormat(item.platform));
		i->setFilesize(item.filesize);
		i->setTextAlignment(COL_SIZE, Qt::AlignTrailing | Qt::AlignVCenter);
		i->setText(COL_CRC32, QString("%0").arg(item.crc32, 8, 16, QChar('0')));
		m_items.insert(item.fullpath, i);

		if (m_currentStyle == LibraryStyle::STYLE_TREE) {
			pathNodes.value(item.base)->addChild(i);
		} else {
			m_widget->addTopLevelItem(i);
		}
	}

	m_widget->expandAll();
	resizeAllCols();
	selectEntry(currentGame);
}
