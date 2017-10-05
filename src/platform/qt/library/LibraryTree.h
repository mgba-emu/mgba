/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QTreeWidget>

#include "LibraryController.h"

namespace QGBA {

class LibraryTree final : public AbstractGameList {

public:
	enum Columns {
		COL_NAME = 0,
		COL_LOCATION = 1,
		COL_PLATFORM = 2,
		COL_SIZE = 3,
		COL_CRC32 = 4,
	};

	explicit LibraryTree(LibraryController* parent = nullptr);
	~LibraryTree();

	// AbstractGameList stuff
	virtual LibraryEntryRef selectedEntry() override;
	virtual void selectEntry(LibraryEntryRef game) override;

	virtual void setViewStyle(LibraryStyle newStyle) override;

	virtual void addEntries(QList<LibraryEntryRef> items) override;
	virtual void addEntry(LibraryEntryRef item) override;
	virtual void removeEntry(LibraryEntryRef item) override;

	virtual QWidget* widget() override { return m_widget; }

private:
	QTreeWidget* m_widget;
	LibraryStyle m_currentStyle;

	LibraryController* m_controller;

	bool m_deferredTreeRebuild = false;
	QMap<LibraryEntryRef, QTreeWidgetItem*> m_items;
	QMap<QString, QTreeWidgetItem*> m_pathNodes;

	void rebuildTree();
	void resizeAllCols();
};

}
