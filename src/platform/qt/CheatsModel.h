/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_CHEATS_MODEL
#define QGBA_CHEATS_MODEL

#include <QAbstractItemModel>

struct GBACheatDevice;
struct GBACheatSet;

namespace QGBA {

class CheatsModel : public QAbstractItemModel {
Q_OBJECT

public:
	CheatsModel(GBACheatDevice* m_device, QObject* parent = nullptr);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

	GBACheatSet* itemAt(const QModelIndex& index);

	void loadFile(const QString& path);
	void addSet(GBACheatSet* set);

public slots:
	void invalidated();

private:
	GBACheatDevice* m_device;
};

}

#endif