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

struct LibraryEntry {
	LibraryEntry() {}
	LibraryEntry(const mLibraryEntry* entry);

	bool isNull() const { return fullpath.isNull(); }

	QString displayTitle() const { return title.isNull() ? filename : title; }

	QString base;
	QString filename;
	QString fullpath;
	QString title;
	QByteArray internalTitle;
	QByteArray internalCode;
	mPlatform platform;
	size_t filesize;
	uint32_t crc32;

	bool operator==(const LibraryEntry& other) const { return other.fullpath == fullpath; }
};

class AbstractGameList {
public:
	virtual QString selectedEntry() = 0;
	virtual void selectEntry(const QString& fullpath) = 0;

	virtual void setViewStyle(LibraryStyle newStyle) = 0;

	virtual void resetEntries(const QList<LibraryEntry>&) = 0;
	virtual void addEntries(const QList<LibraryEntry>&) = 0;
	virtual void updateEntries(const QList<LibraryEntry>&) = 0;
	virtual void removeEntries(const QList<QString>&) = 0;

	virtual void addEntry(const LibraryEntry&);
	virtual void updateEntry(const LibraryEntry&);
	virtual void removeEntry(const QString&);
	virtual void setShowFilename(bool showFilename);

	virtual QWidget* widget() = 0;

protected:
	bool m_showFilename = false;
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

private:
	void loadDirectory(const QString&, bool recursive = true); // Called on separate thread

	ConfigController* m_config = nullptr;
	std::shared_ptr<mLibrary> m_library;
	QAtomicInteger<qint64> m_libraryJob = -1;
	QHash<QString, LibraryEntry> m_entries;

	LibraryStyle m_currentStyle;
	AbstractGameList* m_currentList = nullptr;

	std::unique_ptr<LibraryGrid> m_libraryGrid;
	std::unique_ptr<LibraryTree> m_libraryTree;
	bool m_showFilename = false;
};

}
