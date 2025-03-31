/* Copyright (c) 2014-2017 waddlesplash
 * Copyright (c) 2013-2021 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <memory>

#include <QAtomicInteger>
#include <QHash>
#include <QList>
#include <QStackedWidget>
#include <QTimer>

#include <mgba/core/library.h>

#include "LibraryEntry.h"

class QAbstractItemView;
class QListView;
class QSortFilterProxyModel;
class QTreeView;

namespace QGBA {

class ConfigController;
class LibraryModel;

enum class LibraryStyle {
	STYLE_LIST = 0,
	STYLE_TREE,
	STYLE_GRID,
	STYLE_ICON
};

class LibraryController final : public QStackedWidget {
Q_OBJECT

public:
	LibraryController(QWidget* parent = nullptr, const QString& path = QString(),
	    ConfigController* config = nullptr);
	~LibraryController();

	LibraryStyle viewStyle() const { return m_currentStyle; }
	void setViewStyle(LibraryStyle newStyle);
	void setShowFilename(bool showFilename);

	void selectEntry(const QString& fullpath);
	LibraryEntry selectedEntry();
	VFile* selectedVFile();
	QPair<QString, QString> selectedPath();

	void selectLastBootedGame();

	void addDirectory(const QString& dir, bool recursive = true);

public slots:
	void clear();

signals:
	void startGame();
	void doneLoading();

private slots:
	void refresh();
	void sortChanged(int column, Qt::SortOrder order);
	inline void resizeTreeView() { resizeTreeView(true); }
	void resizeTreeView(bool expand);

protected:
	void showEvent(QShowEvent*) override;
	void resizeEvent(QResizeEvent*) override;

private:
	void loadDirectory(const QString&, bool recursive = true); // Called on separate thread
	void updateViewStyle(LibraryStyle newStyle);

	ConfigController* m_config = nullptr;
	std::shared_ptr<mLibrary> m_library;
	QAtomicInteger<qint64> m_libraryJob = -1;

	LibraryStyle m_currentStyle;

	QHash<QString, uint64_t> m_knownGames;
	LibraryModel* m_libraryModel;
	QSortFilterProxyModel* m_listModel;
	QSortFilterProxyModel* m_treeModel;
	QListView* m_listView;
	QTreeView* m_treeView;
	QAbstractItemView* m_currentView = nullptr;
	bool m_showFilename = false;

	QTimer m_expandThrottle;
};

}
