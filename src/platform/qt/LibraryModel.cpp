/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryModel.h"

#include <QFontMetrics>

#include <mgba-util/vfs.h>

using namespace QGBA;

Q_DECLARE_METATYPE(mLibraryEntry);

QMap<QString, LibraryModel::LibraryColumn> LibraryModel::s_columns;

LibraryModel::LibraryModel(const QString& path, QObject* parent)
	: QAbstractItemModel(parent)
{
	if (s_columns.empty()) {
		s_columns["filename"] = {
			tr("Filename"),
			[](const mLibraryEntry& e) -> QString {
				return e.filename;
			}
		};
		s_columns["size"] = {
			tr("Size"),
			[](const mLibraryEntry& e) -> QString {
				double size = e.filesize;
				QString unit = "B";
				if (size > 1024.0) {
					size /= 1024.0;
					unit = "kiB";
				}
				if (size > 1024.0) {
					size /= 1024.0;
					unit = "MiB";
				}
				return QString("%0 %1").arg(size, 0, 'f', 1).arg(unit);
			}
		};
		s_columns["platform"] = {
			tr("Platform"),
			[](const mLibraryEntry& e) -> QString {
				int platform = e.platform;
				switch (platform) {
#ifdef M_CORE_GBA
				case PLATFORM_GBA:
					return tr("GBA");
#endif
#ifdef M_CORE_GB
				case PLATFORM_GB:
					return tr("GB");
#endif
				default:
					return tr("?");
				}
			}
		};
	}
	if (!path.isNull()) {
		m_library = mLibraryLoad(path.toUtf8().constData());
	} else {
		m_library = mLibraryCreateEmpty();
	}
	memset(&m_constraints, 0, sizeof(m_constraints));
	m_constraints.platform = PLATFORM_NONE;
	m_columns.append(s_columns["filename"]);
	m_columns.append(s_columns["platform"]);
	m_columns.append(s_columns["size"]);

	if (!m_library) {
		return;
	}
	m_loader = new LibraryLoader(m_library);
	connect(m_loader, SIGNAL(directoryLoaded(const QString&)), this, SLOT(directoryLoaded(const QString&)));
	m_loader->moveToThread(&m_loaderThread);
	m_loaderThread.setObjectName("Library Loader Thread");
	m_loaderThread.start();
}

LibraryModel::~LibraryModel() {
	clearConstraints();
	mLibraryDestroy(m_library);
	m_loaderThread.quit();
	m_loaderThread.wait();
}

void LibraryModel::loadDirectory(const QString& path) {
	m_queue.append(path);
	QMetaObject::invokeMethod(m_loader, "loadDirectory", Q_ARG(const QString&, path));
}

bool LibraryModel::entryAt(int row, mLibraryEntry* out) const {
	mLibraryListing entries;
	mLibraryListingInit(&entries, 0);
	if (!mLibraryGetEntries(m_library, &entries, 1, row, &m_constraints)) {
		mLibraryListingDeinit(&entries);
		return false;
	}
	*out = *mLibraryListingGetPointer(&entries, 0);
	mLibraryListingDeinit(&entries);
	return true;
}

VFile* LibraryModel::openVFile(const QModelIndex& index) const {
	mLibraryEntry entry;
	if (!entryAt(index.row(), &entry)) {
		return nullptr;
	}
	return mLibraryOpenVFile(m_library, &entry);
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid()) {
		return QVariant();
	}
	mLibraryEntry entry;
	if (!entryAt(index.row(), &entry)) {
		return QVariant();
	}
	if (role == Qt::UserRole) {
		return QVariant::fromValue(entry);
	}
	if (index.column() >= m_columns.count()) {
		return QVariant();
	}
	switch (role) {
	case Qt::DisplayRole:
		return m_columns[index.column()].value(entry);
	case Qt::SizeHintRole: {
		QFontMetrics fm((QFont()));
		return fm.size(Qt::TextSingleLine, m_columns[index.column()].value(entry));
	}
	default:
		return QVariant();
	}
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (role != Qt::DisplayRole) {
		return QAbstractItemModel::headerData(section, orientation, role);
	}
	if (orientation == Qt::Horizontal) {
		if (section >= m_columns.count()) {
			return QVariant();
		}
		return m_columns[section].name;
	}
	return section;
}

QModelIndex LibraryModel::index(int row, int column, const QModelIndex& parent) const {
	if (parent.isValid()) {
		return QModelIndex();
	}
	return createIndex(row, column, nullptr);
}

QModelIndex LibraryModel::parent(const QModelIndex&) const {
	return QModelIndex();
}

int LibraryModel::columnCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return m_columns.count();
}

int LibraryModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return mLibraryCount(m_library, &m_constraints);
}

void LibraryModel::constrainBase(const QString& path) {
	if (m_constraints.base) {
		free(const_cast<char*>(m_constraints.base));
	}
	m_constraints.base = strdup(path.toUtf8().constData());
}

void LibraryModel::clearConstraints() {
	if (m_constraints.base) {
		free(const_cast<char*>(m_constraints.base));
	}
	if (m_constraints.filename) {
		free(const_cast<char*>(m_constraints.filename));
	}
	if (m_constraints.title) {
		free(const_cast<char*>(m_constraints.title));
	}
	memset(&m_constraints, 0, sizeof(m_constraints));
}

void LibraryModel::directoryLoaded(const QString& path) {
	m_queue.removeOne(path);
	beginResetModel();
	endResetModel();
	if (m_queue.empty()) {
		emit doneLoading();
	}
}

LibraryLoader::LibraryLoader(mLibrary* library, QObject* parent)
	: QObject(parent)
	, m_library(library)
{
}

void LibraryLoader::loadDirectory(const QString& path) {
	mLibraryLoadDirectory(m_library, path.toUtf8().constData());
	emit directoryLoaded(path);
}
