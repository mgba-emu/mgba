/* Copyright (c) 2014-2017 waddlesplash
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryController.h"

#include "ConfigController.h"
#include "GBAApp.h"
#include "LibraryGrid.h"
#include "LibraryTree.h"

namespace QGBA {

LibraryEntry::LibraryEntry(mLibraryEntry* entry)
	: entry(entry)
	, m_fullpath(QString("%1/%2").arg(entry->base, entry->filename))
{
}

void AbstractGameList::addEntries(QList<LibraryEntryRef> items) {
	for (LibraryEntryRef o : items) {
		addEntry(o);
	}
}
void AbstractGameList::removeEntries(QList<LibraryEntryRef> items) {
	for (LibraryEntryRef o : items) {
		removeEntry(o);
	}
}

LibraryController::LibraryController(QWidget* parent, const QString& path, ConfigController* config)
	: QStackedWidget(parent)
	, m_config(config)
{
	mLibraryListingInit(&m_listing, 0);

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

	setViewStyle(LibraryStyle::STYLE_LIST);
	refresh();
}

LibraryController::~LibraryController() {
	mLibraryListingDeinit(&m_listing);
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
	newCurrentList->selectEntry(selectedEntry());
	newCurrentList->setViewStyle(newStyle);
	setCurrentWidget(newCurrentList->widget());
	m_currentList = newCurrentList;
}

void LibraryController::selectEntry(LibraryEntryRef entry) {
	if (!m_currentList) {
		return;
	}
	m_currentList->selectEntry(entry);
}

LibraryEntryRef LibraryController::selectedEntry() {
	if (!m_currentList) {
		return LibraryEntryRef();
	}
	return m_currentList->selectedEntry();
}

VFile* LibraryController::selectedVFile() {
	LibraryEntryRef entry = selectedEntry();
	if (entry) {
		return mLibraryOpenVFile(m_library.get(), entry->entry);
	} else {
		return nullptr;
	}
}

QPair<QString, QString> LibraryController::selectedPath() {
	LibraryEntryRef e = selectedEntry();
	return e ? qMakePair(e->base(), e->filename()) : qMakePair<QString, QString>("", "");
}

void LibraryController::addDirectory(const QString& dir) {
	// The worker thread temporarily owns the library
	std::shared_ptr<mLibrary> library = m_library;
	m_libraryJob = GBAApp::app()->submitWorkerJob(std::bind(&LibraryController::loadDirectory, this, dir), this, [this, library]() {
		m_libraryJob = -1;
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

	QStringList allEntries;
	QList<LibraryEntryRef> newEntries;

	mLibraryListingClear(&m_listing);
	mLibraryGetEntries(m_library.get(), &m_listing, 0, 0, nullptr);
	for (size_t i = 0; i < mLibraryListingSize(&m_listing); i++) {
		mLibraryEntry* entry = mLibraryListingGetPointer(&m_listing, i);
		QString fullpath = QString("%1/%2").arg(entry->base, entry->filename);
		if (m_entries.contains(fullpath)) {
			m_entries.value(fullpath)->entry = entry;
		} else {
			LibraryEntryRef libentry = std::make_shared<LibraryEntry>(entry);
			m_entries.insert(fullpath, libentry);
			newEntries.append(libentry);
		}
		allEntries.append(fullpath);
	}

	// Check for entries that were removed
	QList<LibraryEntryRef> removedEntries;
	for (QString& path : m_entries.keys()) {
		if (!allEntries.contains(path)) {
			removedEntries.append(m_entries.value(path));
			m_entries.remove(path);
		}
	}

	m_libraryTree->addEntries(newEntries);
	m_libraryGrid->addEntries(newEntries);

	m_libraryTree->removeEntries(removedEntries);
	m_libraryGrid->removeEntries(removedEntries);

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
		selectEntry(m_entries.value(lastfile));
	}
}

void LibraryController::loadDirectory(const QString& dir) {
	// This class can get delted during this function (sigh) so we need to hold onto this
	std::shared_ptr<mLibrary> library = m_library;
	mLibraryLoadDirectory(library.get(), dir.toUtf8().constData());
}

}
