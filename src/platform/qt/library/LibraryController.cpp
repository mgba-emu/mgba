/* Copyright (c) 2014-2017 waddlesplash
 * Copyright (c) 2014-2020 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryController.h"

#include "ConfigController.h"
#include "GBAApp.h"
#include "LibraryModel.h"
#include "utils.h"

#include <QHeaderView>
#include <QListView>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QTreeView>

using namespace QGBA;

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

	m_libraryModel = new LibraryModel(this);

	m_treeView = new QTreeView(this);
	addWidget(m_treeView);
	m_treeModel = new QSortFilterProxyModel(this);
	m_treeModel->setSourceModel(m_libraryModel);
	m_treeModel->setSortRole(Qt::EditRole);
	m_treeView->setModel(m_treeModel);
	m_treeView->setSortingEnabled(true);
	m_treeView->setAlternatingRowColors(true);

	m_listView = new QListView(this);
	addWidget(m_listView);
	m_listModel = new QSortFilterProxyModel(this);
	m_listModel->setSourceModel(m_libraryModel);
	m_listModel->setSortRole(Qt::EditRole);
	m_listView->setModel(m_listModel);

	QObject::connect(m_treeView, &QAbstractItemView::activated, this, &LibraryController::startGame);
	QObject::connect(m_listView, &QAbstractItemView::activated, this, &LibraryController::startGame);
	QObject::connect(m_treeView->header(), &QHeaderView::sortIndicatorChanged, this, &LibraryController::sortChanged);

	m_expandThrottle.setInterval(100);
	m_expandThrottle.setSingleShot(true);
	QObject::connect(&m_expandThrottle, &QTimer::timeout, this, qOverload<>(&LibraryController::resizeTreeView));
	QObject::connect(m_libraryModel, &QAbstractItemModel::modelReset, &m_expandThrottle, qOverload<>(&QTimer::start));
	QObject::connect(m_libraryModel, &QAbstractItemModel::rowsInserted, &m_expandThrottle, qOverload<>(&QTimer::start));

	QVariant librarySort, librarySortOrder;
	if (m_config) {
		LibraryStyle libraryStyle = LibraryStyle(m_config->getOption("libraryStyle", int(LibraryStyle::STYLE_LIST)).toInt());
		updateViewStyle(libraryStyle);
		librarySort = m_config->getQtOption("librarySort");
		librarySortOrder = m_config->getQtOption("librarySortOrder");
	} else {
		updateViewStyle(LibraryStyle::STYLE_LIST);
	}

	if (librarySort.isNull() || !librarySort.canConvert<int>()) {
		librarySort = 0;
	}
	if (librarySortOrder.isNull() || !librarySortOrder.canConvert<Qt::SortOrder>()) {
		librarySortOrder = Qt::AscendingOrder;
	}
	m_treeModel->sort(librarySort.toInt(), librarySortOrder.value<Qt::SortOrder>());
	m_listModel->sort(0, Qt::AscendingOrder);
	refresh();
}

LibraryController::~LibraryController() {
}

void LibraryController::setViewStyle(LibraryStyle newStyle) {
	if (m_currentStyle == newStyle) {
		return;
	}
	updateViewStyle(newStyle);
}

void LibraryController::updateViewStyle(LibraryStyle newStyle) {
	QString selected;
	if (m_currentView) {
		QModelIndex selectedIndex = m_currentView->selectionModel()->currentIndex();
		if (selectedIndex.isValid()) {
			selected = selectedIndex.data(LibraryModel::FullPathRole).toString();
		}
	}

	m_currentStyle = newStyle;
	m_libraryModel->setTreeMode(newStyle == LibraryStyle::STYLE_TREE);

	QAbstractItemView* newView = m_listView;
	if (newStyle == LibraryStyle::STYLE_LIST || newStyle == LibraryStyle::STYLE_TREE) {
		newView = m_treeView;
	}

	setCurrentWidget(newView);
	m_currentView = newView;
	selectEntry(selected);
}

void LibraryController::sortChanged(int column, Qt::SortOrder order) {
	if (m_config) {
		m_config->setQtOption("librarySort", column);
		m_config->setQtOption("librarySortOrder", order);
	}
}

void LibraryController::selectEntry(const QString& fullpath) {
	if (!m_currentView) {
		return;
	}
	QModelIndex index = m_libraryModel->index(fullpath);

	// If the model is proxied in the current view, map the index to the proxy
	QAbstractProxyModel* proxy = qobject_cast<QAbstractProxyModel*>(m_currentView->model());
	if (proxy) {
		index = proxy->mapFromSource(index);
	}

	if (index.isValid()) {
		m_currentView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
	}
}

LibraryEntry LibraryController::selectedEntry() {
	if (!m_currentView) {
		return {};
	}
	QModelIndex index = m_currentView->selectionModel()->currentIndex();
	if (!index.isValid()) {
		return {};
	}
	QString fullpath = index.data(LibraryModel::FullPathRole).toString();
	return m_libraryModel->entry(fullpath);
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
		libentry.platformModels = M_LIBRARY_MODEL_UNKNOWN;
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

	QSet<QString> removedEntries(qListToSet(m_knownGames.keys()));
	QList<LibraryEntry> updatedEntries;
	QList<LibraryEntry> newEntries;

	mLibraryListing listing;
	mLibraryListingInit(&listing, 0);
	mLibraryGetEntries(m_library.get(), &listing, 0, 0, nullptr);
	for (size_t i = 0; i < mLibraryListingSize(&listing); i++) {
		const mLibraryEntry* entry = mLibraryListingGetConstPointer(&listing, i);
		uint64_t checkHash = LibraryEntry::checkHash(entry);
		QString fullpath = QStringLiteral("%1/%2").arg(entry->base, entry->filename);
		if (!m_knownGames.contains(fullpath)) {
			newEntries.append(entry);
		} else if (checkHash != m_knownGames[fullpath]) {
			updatedEntries.append(entry);
		}
		removedEntries.remove(fullpath);
		m_knownGames[fullpath] = checkHash;
	}

	// Check for entries that were removed
	for (const QString& path : removedEntries) {
		m_knownGames.remove(path);
	}

	m_libraryModel->removeEntries(removedEntries.values());
	m_libraryModel->updateEntries(updatedEntries);
	m_libraryModel->addEntries(newEntries);

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
	if (m_knownGames.contains(lastfile)) {
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
	m_libraryModel->setShowFilename(m_showFilename);
	refresh();
}

void LibraryController::showEvent(QShowEvent*) {
	resizeTreeView(false);
}

void LibraryController::resizeEvent(QResizeEvent*) {
	resizeTreeView(false);
}

// This function automatically reallocates the horizontal space between the
// columns in the view in a useful way when the window is resized.
void LibraryController::resizeTreeView(bool expand) {
	// When new items are added to the model, make sure they are revealed.
	if (expand) {
		m_treeView->expandAll();
	}

	// Start off by asking the view how wide it thinks each column should be.
	int viewportWidth = m_treeView->viewport()->width();
	int totalWidth = m_treeView->header()->sectionSizeHint(LibraryModel::MAX_COLUMN);
	for (int column = 0; column < LibraryModel::MAX_COLUMN; column++) {
		totalWidth += m_treeView->columnWidth(column);
	}

	// If there would be empty space, ask the view to redistribute it.
	// The final column is set to fill any remaining width, so this
	// should (at least) fill the window.
	if (totalWidth < viewportWidth) {
		totalWidth = 0;
		for (int column = 0; column <= LibraryModel::MAX_COLUMN; column++) {
			m_treeView->resizeColumnToContents(column);
			totalWidth += m_treeView->columnWidth(column);
		}
	}

	// If the columns would be too wide for the view now, try shrinking the
	// "Location" column down to reduce horizontal scrolling, with a fixed
	// minimum width of 100px.
	if (totalWidth > viewportWidth) {
		int locationWidth = m_treeView->columnWidth(LibraryModel::COL_LOCATION);
		if (locationWidth > 100) {
			int newLocationWidth = m_treeView->viewport()->width() - (totalWidth - locationWidth);
			if (newLocationWidth < 100) {
				newLocationWidth = 100;
			}
			m_treeView->setColumnWidth(LibraryModel::COL_LOCATION, newLocationWidth);
		}
	}
}
