/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LibraryModel.h"

#include "util/vfs.h"

using namespace QGBA;

LibraryModel::LibraryModel(QObject* parent)
	: QAbstractItemModel(parent)
{
	mLibraryInit(&m_library);
}

LibraryModel::~LibraryModel() {
	mLibraryDeinit(&m_library);
}

void LibraryModel::loadDirectory(VDir* dir) {
	mLibraryLoadDirectory(&m_library, dir);
}

const mLibraryEntry* LibraryModel::entryAt(int row) const {
	if ((unsigned) row < mLibraryListingSize(&m_library.listing)) {
		return mLibraryListingGetConstPointer(&m_library.listing, row);
	}
	return nullptr;
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid()) {
		return QVariant();
	}
	if (role != Qt::DisplayRole) {
		return QVariant();
	}
	const mLibraryEntry* entry = mLibraryListingGetConstPointer(&m_library.listing, index.row());
	switch (index.column()) {
	case 0:
		return entry->filename;
	case 1:
		return (unsigned long long) entry->filesize;
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
	return mLibraryListingSize(&m_library.listing);
}
