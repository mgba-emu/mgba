/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryTree.h"

#include "utils.h"

#include "ui_LibraryTree.h"

#include <QDir>

namespace QGBA {

class TreeWidgetItem : public QTreeWidgetItem {
public:
	TreeWidgetItem(QTreeWidget* parent = nullptr) : QTreeWidgetItem(parent) {}
	void setFilesize(size_t size);

	virtual bool operator<(const QTreeWidgetItem& other) const override;
protected:
	size_t m_size = 0;
};

void TreeWidgetItem::setFilesize(size_t size) {
	m_size = size;
	setText(LibraryTree::COL_SIZE, niceSizeFormat(size));
}

bool TreeWidgetItem::operator<(const QTreeWidgetItem& other) const {
	const int column = treeWidget()->sortColumn();
	return ((column == LibraryTree::COL_SIZE) ?
		m_size < dynamic_cast<const TreeWidgetItem*>(&other)->m_size :
		QTreeWidgetItem::operator<(other));
}

LibraryTree::LibraryTree(QWidget* parent)
	: QTreeWidget(parent)
	, m_ui(new Ui::LibraryTree)
{
	m_ui->setupUi(this);

	setViewStyle(LibraryStyle::STYLE_TREE);
	sortByColumn(COL_NAME, Qt::AscendingOrder);

	connect(this, &QTreeWidget::itemActivated, this, &LibraryTree::itemActivated);
}

void LibraryTree::resizeAllCols() {
	for (int i = 0; i < columnCount(); i++) {
		resizeColumnToContents(i);
	}
}

void LibraryTree::itemActivated(QTreeWidgetItem* item) {
	if (!m_pathNodes.values().contains(item)) {
		emit startGame();
	}
}

LibraryEntryRef LibraryTree::selectedEntry() {
	if (!selectedItems().empty()) {
		return m_items.key(selectedItems().at(0));
	} else {
		return LibraryEntryRef();
	}
}

void LibraryTree::selectEntry(LibraryEntryRef game) {
	if (!game) {
		return;
	}
	if (!selectedItems().empty()) {
		selectedItems().at(0)->setSelected(false);
	}
	m_items.value(game)->setSelected(true);
}

void LibraryTree::setViewStyle(LibraryStyle newStyle) {
	if (newStyle == LibraryStyle::STYLE_LIST) {
		m_currentStyle = LibraryStyle::STYLE_LIST;
		setIndentation(0);
		rebuildTree();
	} else {
		m_currentStyle = LibraryStyle::STYLE_TREE;
		setIndentation(20);
		rebuildTree();
	}
}

void LibraryTree::addEntries(QList<LibraryEntryRef> items) {
	m_deferredTreeRebuild = true;
	AbstractGameList::addEntries(items);
	m_deferredTreeRebuild = false;
	rebuildTree();
}

void LibraryTree::addEntry(LibraryEntryRef item) {
	if (m_items.contains(item)) {
		return;
	}

	QString folder = item->base();
	if (!m_pathNodes.contains(folder)) {
		QTreeWidgetItem* i = new TreeWidgetItem;
		i->setText(0, folder.section("/", -1));
		m_pathNodes.insert(folder, i);
		if (m_currentStyle == LibraryStyle::STYLE_TREE) {
			addTopLevelItem(i);
		}
	}

	TreeWidgetItem* i = new TreeWidgetItem;
	i->setText(COL_NAME, item->displayTitle());
	i->setText(COL_LOCATION, QDir::toNativeSeparators(item->base()));
	i->setText(COL_PLATFORM, nicePlatformFormat(item->platform()));
	i->setFilesize(item->filesize());
	i->setTextAlignment(COL_SIZE, Qt::AlignRight);
	i->setText(COL_CRC32, QString("%0").arg(item->crc32(), 8, 16, QChar('0')));
	m_items.insert(item, i);

	rebuildTree();
}

void LibraryTree::removeEntry(LibraryEntryRef item) {
	if (!m_items.contains(item)) {
		return;
	}
	delete m_items.take(item);
}

void LibraryTree::rebuildTree() {
	if (m_deferredTreeRebuild) {
		return;
	}

	LibraryEntryRef currentGame = selectedEntry();

	int count = topLevelItemCount();
	for (int a = 0; a < count; a++) {
		takeTopLevelItem(0);
	}

	for (QTreeWidgetItem* i : m_pathNodes.values()) {
		count = i->childCount();
		for (int a = 0; a < count; a++) {
			i->takeChild(0);
		}
	}

	if (m_currentStyle == LibraryStyle::STYLE_TREE) {
		for (QTreeWidgetItem* i : m_pathNodes.values()) {
			addTopLevelItem(i);
		}
		for (QTreeWidgetItem* i : m_items.values()) {
			m_pathNodes.value(m_items.key(i)->base())->addChild(i);
		}
	} else {
		for (QTreeWidgetItem* i : m_items.values()) {
			addTopLevelItem(i);
		}
	}

	expandAll();
	resizeAllCols();
	selectEntry(currentGame);
}

}
