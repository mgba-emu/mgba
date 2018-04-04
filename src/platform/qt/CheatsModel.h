/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAbstractItemModel>
#include <QFont>

struct mCheatDevice;
struct mCheatSet;

namespace QGBA {

class CheatsModel : public QAbstractItemModel {
Q_OBJECT

public:
	CheatsModel(mCheatDevice* m_device, QObject* parent = nullptr);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

	mCheatSet* itemAt(const QModelIndex& index);
	void removeAt(const QModelIndex& index);
	QString toString(const QModelIndexList& indices) const;

	void beginAppendRow(const QModelIndex& index);
	void endAppendRow();

	void loadFile(const QString& path);
	void saveFile(const QString& path);

	void addSet(mCheatSet* set);

public slots:
	void invalidated();

private:
	mCheatDevice* m_device;
	QFont m_font;
};

}
