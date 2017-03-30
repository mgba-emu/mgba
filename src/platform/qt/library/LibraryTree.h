/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LIBRARY_TREE
#define QGBA_LIBRARY_TREE

#include <QTreeWidget>

#include "LibraryController.h"

// Predefinitions
namespace Ui { class LibraryTree; }

namespace QGBA {

class LibraryTree final : public QTreeWidget, public AbstractGameList {
Q_OBJECT

public:
	enum Columns {
		COL_NAME = 0,
		COL_LOCATION = 1,
		COL_PLATFORM = 2,
		COL_SIZE = 3,
		COL_CRC32 = 4,
	};

	explicit LibraryTree(QWidget* parent = nullptr);

	// AbstractGameList stuff
	virtual LibraryEntryRef selectedEntry() override;
	virtual void selectEntry(LibraryEntryRef game) override;

	virtual void setViewStyle(LibraryStyle newStyle) override;

	virtual void addEntries(QList<LibraryEntryRef> items) override;
	virtual void addEntry(LibraryEntryRef item) override;
	virtual void removeEntry(LibraryEntryRef item) override;

signals:
	void startGame();

private slots:
	void itemActivated(QTreeWidgetItem* item);

private:
	std::unique_ptr<Ui::LibraryTree> m_ui;
	LibraryStyle m_currentStyle;

	bool m_deferredTreeRebuild = false;
	QMap<LibraryEntryRef, QTreeWidgetItem*> m_items;
	QMap<QString, QTreeWidgetItem*> m_pathNodes;

	void rebuildTree();
	void resizeAllCols();
};

}

#endif
