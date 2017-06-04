/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryTree.h"

#include "utils.h"

#include <QApplication>
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

LibraryTree::LibraryTree(LibraryController* parent)
	: m_widget(new QTreeWidget(parent))
	, m_controller(parent)
{
	m_widget->setObjectName("LibraryTree");
	m_widget->setSortingEnabled(true);
	m_widget->setAlternatingRowColors(true);

	QTreeWidgetItem* header = new QTreeWidgetItem({
		QApplication::translate("LibraryTree", "Name", nullptr),
		QApplication::translate("LibraryTree", "Location", nullptr),
		QApplication::translate("LibraryTree", "Platform", nullptr),
		QApplication::translate("LibraryTree", "Size", nullptr),
		QApplication::translate("LibraryTree", "CRC32", nullptr),
	});
	header->setTextAlignment(3, Qt::AlignTrailing | Qt::AlignVCenter);
	m_widget->setHeaderItem(header);

	setViewStyle(LibraryStyle::STYLE_TREE);
	m_widget->sortByColumn(COL_NAME, Qt::AscendingOrder);

	QObject::connect(m_widget, &QTreeWidget::itemActivated, [this](QTreeWidgetItem* item, int) -> void {
		if (!m_pathNodes.values().contains(item)) {
			emit m_controller->startGame();
		}
	});
}

void LibraryTree::resizeAllCols() {
	for (int i = 0; i < m_widget->columnCount(); i++) {
		m_widget->resizeColumnToContents(i);
	}
}

LibraryEntryRef LibraryTree::selectedEntry() {
	if (!m_widget->selectedItems().empty()) {
		return m_items.key(m_widget->selectedItems().at(0));
	} else {
		return LibraryEntryRef();
	}
}

void LibraryTree::selectEntry(LibraryEntryRef game) {
	if (!game) {
		return;
	}
	if (!m_widget->selectedItems().empty()) {
		m_widget->selectedItems().at(0)->setSelected(false);
	}
	m_items.value(game)->setSelected(true);
}

void LibraryTree::setViewStyle(LibraryStyle newStyle) {
	if (newStyle == LibraryStyle::STYLE_LIST) {
		m_currentStyle = LibraryStyle::STYLE_LIST;
		m_widget->setIndentation(0);
		rebuildTree();
	} else {
		m_currentStyle = LibraryStyle::STYLE_TREE;
		m_widget->setIndentation(20);
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
			m_widget->addTopLevelItem(i);
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

	int count = m_widget->topLevelItemCount();
	for (int a = count - 1; a >= 0; --a) {
		m_widget->takeTopLevelItem(a);
	}

	for (QTreeWidgetItem* i : m_pathNodes.values()) {
		i->takeChildren();
	}

	if (m_currentStyle == LibraryStyle::STYLE_TREE) {
		for (QTreeWidgetItem* i : m_pathNodes.values()) {
			m_widget->addTopLevelItem(i);
		}
		for (QTreeWidgetItem* i : m_items.values()) {
			m_pathNodes.value(m_items.key(i)->base())->addChild(i);
		}
	} else {
		for (QTreeWidgetItem* i : m_items.values()) {
			m_widget->addTopLevelItem(i);
		}
	}

	m_widget->expandAll();
	resizeAllCols();
	selectEntry(currentGame);
}

}
