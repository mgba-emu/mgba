/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LIBRARY_CONTROLLER
#define QGBA_LIBRARY_CONTROLLER

#include <memory>

#include <QList>
#include <QMap>
#include <QThread>
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

class LibraryEntry final {
public:
	LibraryEntry(mLibraryEntry* entry);

	QString displayTitle() const { return title().isNull() ? filename() : title(); }

	QString base() const { return QString(entry->base); }
	QString filename() const { return QString(entry->filename); }
	QString fullpath() const { return m_fullpath; }
	QString title() const { return QString(entry->title); }
	QByteArray internalTitle() const { return QByteArray(entry->internalTitle); }
	QByteArray internalCode() const { return QByteArray(entry->internalCode); }
	mPlatform platform() const { return entry->platform; }
	size_t filesize() const { return entry->filesize; }
	uint32_t crc32() const { return entry->crc32; }

	const mLibraryEntry* entry;
private:
	const QString m_fullpath;
};
typedef std::shared_ptr<LibraryEntry> LibraryEntryRef;

class AbstractGameList {
public:
	virtual LibraryEntryRef selectedEntry() = 0;
	virtual void selectEntry(LibraryEntryRef game) = 0;

	virtual void setViewStyle(LibraryStyle newStyle) = 0;

	virtual void addEntry(LibraryEntryRef item) = 0;
	virtual void addEntries(QList<LibraryEntryRef> items);

	virtual void removeEntry(LibraryEntryRef item) = 0;
	virtual void removeEntries(QList<LibraryEntryRef> items);

	virtual QWidget* widget() = 0;
};

class LibraryLoaderThread final : public QThread {
Q_OBJECT

public:
	LibraryLoaderThread(QObject* parent = nullptr);

	mLibrary* m_library = nullptr;
	QString m_directory;

protected:
	virtual void run() override;
};

class LibraryController final : public QStackedWidget {
Q_OBJECT

public:
	LibraryController(QWidget* parent = nullptr, const QString& path = QString(),
	    ConfigController* config = nullptr);
	~LibraryController();

	LibraryStyle viewStyle() const { return m_currentStyle; }
	void setViewStyle(LibraryStyle newStyle);

	void selectEntry(LibraryEntryRef entry);
	LibraryEntryRef selectedEntry();
	VFile* selectedVFile();
	QPair<QString, QString> selectedPath();

	void selectLastBootedGame();

	void addDirectory(const QString& dir);

public slots:
	void clear();

signals:
	void startGame();
	void doneLoading();

private slots:
	void refresh();

private:
	ConfigController* m_config = nullptr;
	LibraryLoaderThread m_loaderThread;
	mLibrary* m_library = nullptr;
	mLibraryListing m_listing;
	QMap<QString, LibraryEntryRef> m_entries;

	LibraryStyle m_currentStyle;
	AbstractGameList* m_currentList = nullptr;

	LibraryGrid* m_libraryGrid = nullptr;
	LibraryTree* m_libraryTree = nullptr;
};

}

#endif
