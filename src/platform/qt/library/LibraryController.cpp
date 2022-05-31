/* Copyright (c) 2014-2017 waddlesplash
 * Copyright (c) 2014-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryController.h"

#include "ConfigController.h"
#include "GBAApp.h"
#include "LibraryGrid.h"
#include "LibraryTree.h"

using namespace QGBA;

LibraryEntry::LibraryEntry(const mLibraryEntry* entry)
	: base(entry->base)
	, filename(entry->filename)
	, fullpath(QString("%1/%2").arg(entry->base, entry->filename))
	, title(entry->title)
	, internalTitle(entry->internalTitle)
	, internalCode(entry->internalCode)
	, platform(entry->platform)
	, filesize(entry->filesize)
	, crc32(entry->crc32)
{
}

void AbstractGameList::addEntry(const LibraryEntry& item) {
	addEntries({item});
}

void AbstractGameList::updateEntry(const LibraryEntry& item) {
	updateEntries({item});
}

void AbstractGameList::removeEntry(const QString& item) {
	removeEntries({item});
}
void AbstractGameList::setShowFilename(bool showFilename) {
	m_showFilename = showFilename;
}

LibraryController::LibraryController(QWidget* parent, const QString& path, ConfigController* config)
	: QStackedWidget(parent)
	, m_config(config)
{
	if (!path.isNull()) {
		// This can return NULL if the library is already open
		m_library = std::shared_ptr<mLibrary>(mLibraryLoad(path.toUtf8().constData()), mLibraryDestroy);
	}
	if (!m_library) {
		m_library = std::shared_ptr<mLibrary>(mLibraryCreateEmpty(), mLibraryDestroy);
	}

	mLibraryAttachGameDB(m_library.get(), GBAApp::app()->gameDB());

	m_libraryTree = std::make_unique<LibraryTree>(this);
	addWidget(m_libraryTree->widget());

	m_libraryGrid = std::make_unique<LibraryGrid>(this);
	addWidget(m_libraryGrid->widget());

	m_currentStyle = LibraryStyle::STYLE_TREE; // Make sure setViewStyle does something
	setViewStyle(LibraryStyle::STYLE_LIST);
	refresh();
}

LibraryController::~LibraryController() {
}

void LibraryController::setViewStyle(LibraryStyle newStyle) {
	if (m_currentStyle == newStyle) {
		return;
	}
	m_currentStyle = newStyle;

	AbstractGameList* newCurrentList = nullptr;
	if (newStyle == LibraryStyle::STYLE_LIST || newStyle == LibraryStyle::STYLE_TREE) {
		newCurrentList = m_libraryTree.get();
	} else {
		newCurrentList = m_libraryGrid.get();
	}
	newCurrentList->selectEntry(selectedEntry().fullpath);
	newCurrentList->setViewStyle(newStyle);
	setCurrentWidget(newCurrentList->widget());
	m_currentList = newCurrentList;
}

void LibraryController::selectEntry(const QString& fullpath) {
	if (!m_currentList) {
		return;
	}
	m_currentList->selectEntry(fullpath);
}

LibraryEntry LibraryController::selectedEntry() {
	if (!m_currentList) {
		return {};
	}
	return m_entries.value(m_currentList->selectedEntry());
}

VFile* LibraryController::selectedVFile() {
	LibraryEntry entry = selectedEntry();
	if (!entry.isNull()) {
		mLibraryEntry libentry = {0};
		QByteArray baseUtf8(entry.base.toUtf8());
		QByteArray filenameUtf8(entry.filename.toUtf8());
		libentry.base = baseUtf8.constData();
		libentry.filename = filenameUtf8.constData();
		libentry.platform = mPLATFORM_NONE;
		return mLibraryOpenVFile(m_library.get(), &libentry);
	} else {
		return nullptr;
	}
}

QPair<QString, QString> LibraryController::selectedPath() {
	LibraryEntry entry = selectedEntry();
	if (!entry.isNull()) {
		return qMakePair(QString(entry.base), QString(entry.filename));
	} else {
		return qMakePair(QString(), QString());
	}
}

void LibraryController::addDirectory(const QString& dir, bool recursive) {
	// The worker thread temporarily owns the library
	std::shared_ptr<mLibrary> library = m_library;
	m_libraryJob = GBAApp::app()->submitWorkerJob(std::bind(&LibraryController::loadDirectory, this, dir, recursive), this, [this, library]() {
		refresh();
	});
}

void LibraryController::clear() {
	if (m_libraryJob > 0) {
		return;
	}

	mLibraryClear(m_library.get());
	refresh();
}

void LibraryController::refresh() {
	if (m_libraryJob > 0) {
		return;
	}

	setDisabled(true);

	QHash<QString, LibraryEntry> removedEntries = m_entries;
	QHash<QString, LibraryEntry> updatedEntries;
	QList<LibraryEntry> newEntries;

	mLibraryListing listing;
	mLibraryListingInit(&listing, 0);
	mLibraryGetEntries(m_library.get(), &listing, 0, 0, nullptr);
	for (size_t i = 0; i < mLibraryListingSize(&listing); i++) {
		LibraryEntry entry = mLibraryListingGetConstPointer(&listing, i);
		if (!m_entries.contains(entry.fullpath)) {
			newEntries.append(entry);
		} else {
			updatedEntries[entry.fullpath] = entry;
		}
		m_entries[entry.fullpath] = entry;
		removedEntries.remove(entry.fullpath);
	}

	// Check for entries that were removed
	for (QString& path : removedEntries.keys()) {
		m_entries.remove(path);
	}

	if (!removedEntries.size() && !newEntries.size()) {
		m_libraryTree->updateEntries(updatedEntries.values());
		m_libraryGrid->updateEntries(updatedEntries.values());
	} else if (!updatedEntries.size()) {
		m_libraryTree->removeEntries(removedEntries.keys());
		m_libraryGrid->removeEntries(removedEntries.keys());

		m_libraryTree->addEntries(newEntries);
		m_libraryGrid->addEntries(newEntries);
	} else {
		m_libraryTree->resetEntries(m_entries.values());
		m_libraryGrid->resetEntries(m_entries.values());
	}

	for (size_t i = 0; i < mLibraryListingSize(&listing); ++i) {
		mLibraryEntryFree(mLibraryListingGetPointer(&listing, i));
	}
	mLibraryListingDeinit(&listing);

	setDisabled(false);
	selectLastBootedGame();
	emit doneLoading();
}

void LibraryController::selectLastBootedGame() {
	if (!m_config || m_config->getMRU().isEmpty()) {
		return;
	}
	const QString lastfile = m_config->getMRU().first();
	if (m_entries.contains(lastfile)) {
		selectEntry(lastfile);
	}
}

void LibraryController::loadDirectory(const QString& dir, bool recursive) {
	// This class can get deleted during this function (sigh) so we need to hold onto this
	std::shared_ptr<mLibrary> library = m_library;
	qint64 libraryJob = m_libraryJob;
	mLibraryLoadDirectory(library.get(), dir.toUtf8().constData(), recursive);
	m_libraryJob.testAndSetOrdered(libraryJob, -1);
}
void LibraryController::setShowFilename(bool showFilename) {
	if (showFilename == m_showFilename) {
		return;
	}
	m_showFilename = showFilename;
	if (m_libraryGrid) {
		m_libraryGrid->setShowFilename(m_showFilename);
	}
	if (m_libraryTree) {
		m_libraryTree->setShowFilename(m_showFilename);
	}
	refresh();
}
