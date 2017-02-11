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

QMap<QString, LibraryModel::LibraryHandle*> LibraryModel::s_handles;
QMap<QString, LibraryModel::LibraryColumn> LibraryModel::s_columns;

LibraryModel::LibraryModel(const QString& path, QObject* parent)
	: QAbstractItemModel(parent)
{
	if (s_columns.empty()) {
		s_columns["name"] = {
			tr("Name"),
			[](const mLibraryEntry& e) -> QString {
				if (e.title) {
					return QString::fromUtf8(e.title);
				}
				return QString::fromUtf8(e.filename);
			}
		};
		s_columns["filename"] = {
			tr("Filename"),
			[](const mLibraryEntry& e) -> QString {
				return QString::fromUtf8(e.filename);
			}
		};
		s_columns["size"] = {
			tr("Size"),
			[](const mLibraryEntry& e) -> QString {
				double size = e.filesize;
				QString unit = "B";
				if (size >= 1024.0) {
					size /= 1024.0;
					unit = "kiB";
				}
				if (size >= 1024.0) {
					size /= 1024.0;
					unit = "MiB";
				}
				return QString("%0 %1").arg(size, 0, 'f', 1).arg(unit);
			},
			Qt::AlignRight
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
		s_columns["location"] = {
			tr("Location"),
			[](const mLibraryEntry& e) -> QString {
				return QString::fromUtf8(e.base);
			}
		};
		s_columns["crc32"] = {
			tr("CRC32"),
			[](const mLibraryEntry& e) -> QString {
				return QString("%0").arg(e.crc32, 8, 16, QChar('0'));
			}
		};
	}
	if (!path.isNull()) {
		if (s_handles.contains(path)) {
			m_library = s_handles[path];
			m_library->ref();
		} else {
			m_library = new LibraryHandle(mLibraryLoad(path.toUtf8().constData()), path);
			if (m_library->library) {
				s_handles[path] = m_library;
			} else {
				delete m_library;
				m_library = new LibraryHandle(mLibraryCreateEmpty());
			}
		}
	} else {
		m_library = new LibraryHandle(mLibraryCreateEmpty());
	}
	memset(&m_constraints, 0, sizeof(m_constraints));
	m_constraints.platform = PLATFORM_NONE;
	m_columns.append(s_columns["name"]);
	m_columns.append(s_columns["location"]);
	m_columns.append(s_columns["platform"]);
	m_columns.append(s_columns["size"]);
	m_columns.append(s_columns["crc32"]);

	connect(m_library->loader, SIGNAL(directoryLoaded(const QString&)), this, SLOT(directoryLoaded(const QString&)));
}

LibraryModel::~LibraryModel() {
	clearConstraints();
	if (!m_library->deref()) {
		s_handles.remove(m_library->path);
		delete m_library;
	}
}

void LibraryModel::loadDirectory(const QString& path) {
	m_queue.append(path);
	QMetaObject::invokeMethod(m_library->loader, "loadDirectory", Q_ARG(const QString&, path));
}

bool LibraryModel::entryAt(int row, mLibraryEntry* out) const {
	mLibraryListing entries;
	mLibraryListingInit(&entries, 0);
	if (!mLibraryGetEntries(m_library->library, &entries, 1, row, &m_constraints)) {
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
	return mLibraryOpenVFile(m_library->library, &entry);
}

QString LibraryModel::filename(const QModelIndex& index) const {
	mLibraryEntry entry;
	if (!entryAt(index.row(), &entry)) {
		return QString();
	}
	return QString::fromUtf8(entry.filename);
}

QString LibraryModel::location(const QModelIndex& index) const {
	mLibraryEntry entry;
	if (!entryAt(index.row(), &entry)) {
		return QString();
	}
	return QString::fromUtf8(entry.base);
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
	case Qt::TextAlignmentRole:
		return m_columns[index.column()].alignment;
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
	return mLibraryCount(m_library->library, &m_constraints);
}

void LibraryModel::attachGameDB(const NoIntroDB* gameDB) {
	mLibraryAttachGameDB(m_library->library, gameDB);
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

LibraryModel::LibraryColumn::LibraryColumn() {
}

LibraryModel::LibraryColumn::LibraryColumn(const QString& name, std::function<QString(const mLibraryEntry&)> value, int alignment)
	: name(name)
	, value(value)
	, alignment(alignment)
{
}

LibraryModel::LibraryHandle::LibraryHandle(mLibrary* lib, const QString& p)
	: library(lib)
	, loader(new LibraryLoader(library))
	, path(p)
	, m_ref(1)
{
	if (!library) {
		return;
	}
	loader->moveToThread(&m_loaderThread);
	m_loaderThread.setObjectName("Library Loader Thread");
	m_loaderThread.start();
}

LibraryModel::LibraryHandle::~LibraryHandle() {
	m_loaderThread.quit();
	m_loaderThread.wait();
	if (library) {
		mLibraryDestroy(library);
	}
}

void LibraryModel::LibraryHandle::ref() {
	++m_ref;
}

bool LibraryModel::LibraryHandle::deref() {
	--m_ref;
	return m_ref > 0;
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
