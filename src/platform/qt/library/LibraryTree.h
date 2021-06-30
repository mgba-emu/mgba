/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QTreeWidget>

#include "LibraryController.h"

namespace QGBA {

class LibraryTreeItem;

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

	QString selectedEntry() override;
	void selectEntry(const QString& fullpath) override;

	void setViewStyle(LibraryStyle newStyle) override;

	void resetEntries(const QList<LibraryEntry>& items) override;
	void addEntries(const QList<LibraryEntry>& items) override;
	void updateEntries(const QList<LibraryEntry>& items) override;
	void removeEntries(const QList<QString>& items) override;

	void addEntry(const LibraryEntry& items) override;
	void updateEntry(const LibraryEntry& items) override;
	void removeEntry(const QString& items) override;

	QWidget* widget() override { return m_widget; }

private:
	QTreeWidget* m_widget;
	LibraryStyle m_currentStyle;

	LibraryController* m_controller;

	bool m_deferredTreeRebuild = false;
	QHash<QString, LibraryEntry> m_entries;
	QHash<QString, QTreeWidgetItem*> m_items;
	QHash<QString, int> m_pathNodes;

	void rebuildTree();
	void resizeAllCols();
};

}
