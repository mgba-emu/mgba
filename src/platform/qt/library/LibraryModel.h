/* Copyright (c) 2013-2022 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAbstractItemModel>
#include <QIcon>
#include <QTreeView>

#include <mgba/core/library.h>

#include <memory>
#include <vector>

#include "LibraryEntry.h"

class QTreeView;
class LibraryModelTest;

namespace QGBA {

class LibraryModel final : public QAbstractItemModel {
Q_OBJECT

public:
	enum Columns {
		COL_NAME = 0,
		COL_LOCATION = 1,
		COL_PLATFORM = 2,
		COL_SIZE = 3,
		COL_CRC32 = 4,
		MAX_COLUMN = 4,
	};

	enum ItemDataRole {
		FullPathRole = Qt::UserRole + 1,
	};

	explicit LibraryModel(QObject* parent = nullptr);

	bool treeMode() const;
	void setTreeMode(bool tree);

	bool showFilename() const;
	void setShowFilename(bool show);

	void resetEntries(const QList<LibraryEntry>& items);
	void addEntries(const QList<LibraryEntry>& items);
	void updateEntries(const QList<LibraryEntry>& items);
	void removeEntries(const QList<QString>& items);

	QModelIndex index(const QString& game) const;
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;

	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	LibraryEntry entry(const QString& game) const;

private:
	friend class ::LibraryModelTest;

	QModelIndex indexForPath(const QString& path);
	QModelIndex indexForPath(const QString& path) const;

	QVariant folderData(const QModelIndex& index, int role = Qt::DisplayRole) const;

	bool validateIndex(const QModelIndex& index) const;

	void addEntriesList(const QList<LibraryEntry>& items);
	void addEntriesTree(const QList<LibraryEntry>& items);
	void addEntryInternal(const LibraryEntry& item);

	bool m_treeMode;
	bool m_showFilename;

	std::vector<std::unique_ptr<LibraryEntry>> m_games;
	QStringList m_pathOrder;
	QHash<QString, QList<const LibraryEntry*>> m_pathIndex;
	QHash<QString, int> m_gameIndex;
};

}
