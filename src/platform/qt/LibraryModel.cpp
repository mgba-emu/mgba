/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryModel.h"

#include <mgba-util/vfs.h>

using namespace QGBA;

Q_DECLARE_METATYPE(mLibraryEntry);

LibraryModel::LibraryModel(const QString& path, QObject* parent)
	: QAbstractItemModel(parent)
{
	if (!path.isNull()) {
		m_library = mLibraryLoad(path.toUtf8().constData());
	} else {
		m_library = mLibraryCreateEmpty();
	}
	memset(&m_constraints, 0, sizeof(m_constraints));
	m_constraints.platform = PLATFORM_NONE;
}

LibraryModel::~LibraryModel() {
	clearConstraints();
	mLibraryDestroy(m_library);
}

void LibraryModel::loadDirectory(const QString& path) {
	beginResetModel();
	mLibraryLoadDirectory(m_library, path.toUtf8().constData());
	endResetModel();
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
	if (role != Qt::DisplayRole) {
		return QVariant();
	}
	switch (index.column()) {
	case 0:
		return entry.filename;
	case 1:
		return (unsigned long long) entry.filesize;
	}
	return QVariant();
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (role != Qt::DisplayRole) {
		return QAbstractItemModel::headerData(section, orientation, role);
	}
	if (orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("Filename");
		case 1:
			return tr("Size");
		}
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
	return 2;
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
