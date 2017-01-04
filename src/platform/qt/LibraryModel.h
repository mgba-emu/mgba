/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_LIBRARY_MODEL
#define QGBA_LIBRARY_MODEL

#include <QAbstractItemModel>

#include <mgba/core/library.h>

struct VDir;

namespace QGBA {

class LibraryModel : public QAbstractItemModel {
Q_OBJECT

public:
	LibraryModel(QObject* parent = nullptr);
	virtual ~LibraryModel();

	void loadDirectory(VDir* dir);

	const mLibraryEntry* entryAt(int row) const;

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

private:
	mLibrary m_library;

};

}

#endif
