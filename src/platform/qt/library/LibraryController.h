/* Copyright (c) 2014-2017 waddlesplash
 * Copyright (c) 2014-2021 Jeffrey Pfau
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

#include <mgba/core/library.h>

namespace QGBA {

// Predefinitions
class LibraryGrid;
class LibraryTree;
class ConfigController;

enum class LibraryStyle {
	STYLE_LIST = 0,
	STYLE_TREE,
	STYLE_GRID,
	STYLE_ICON
};

class AbstractGameList {
public:
	virtual mLibraryEntry* selectedEntry() = 0;
	virtual void selectEntry(mLibraryEntry* game) = 0;

	virtual void setViewStyle(LibraryStyle newStyle) = 0;

	virtual void addEntry(mLibraryEntry* item) = 0;
	virtual void addEntries(QList<mLibraryEntry*> items);

	virtual void removeEntry(mLibraryEntry* item) = 0;
	virtual void removeEntries(QList<mLibraryEntry*> items);

	virtual QWidget* widget() = 0;
};

class LibraryController final : public QStackedWidget {
Q_OBJECT

public:
	LibraryController(QWidget* parent = nullptr, const QString& path = QString(),
	    ConfigController* config = nullptr);
	~LibraryController();

	LibraryStyle viewStyle() const { return m_currentStyle; }
	void setViewStyle(LibraryStyle newStyle);

	void selectEntry(mLibraryEntry* entry);
	mLibraryEntry* selectedEntry();
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

private:
	void loadDirectory(const QString&, bool recursive = true); // Called on separate thread
	void freeLibrary();

	ConfigController* m_config = nullptr;
	std::shared_ptr<mLibrary> m_library;
	QAtomicInteger<qint64> m_libraryJob = -1;
	mLibraryListing m_listing;
	QHash<QString, mLibraryEntry*> m_entries;

	LibraryStyle m_currentStyle;
	AbstractGameList* m_currentList = nullptr;

	std::unique_ptr<LibraryGrid> m_libraryGrid;
	std::unique_ptr<LibraryTree> m_libraryTree;
};

}
