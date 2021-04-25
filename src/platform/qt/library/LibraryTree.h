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
	virtual mLibraryEntry* selectedEntry() override;
	virtual void selectEntry(mLibraryEntry* game) override;

	virtual void setViewStyle(LibraryStyle newStyle) override;

	virtual void addEntries(QList<mLibraryEntry*> items) override;
	virtual void addEntry(mLibraryEntry* item) override;
	virtual void removeEntry(mLibraryEntry* item) override;

	virtual QWidget* widget() override { return m_widget; }

private:
	QTreeWidget* m_widget;
	LibraryStyle m_currentStyle;

	LibraryController* m_controller;

	bool m_deferredTreeRebuild = false;
	QHash<mLibraryEntry*, QTreeWidgetItem*> m_items;
	QHash<QString, QTreeWidgetItem*> m_pathNodes;

	void rebuildTree();
	void resizeAllCols();
};

}
