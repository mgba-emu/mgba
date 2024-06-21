/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAbstractListModel>
#include <QPixmap>

namespace QGBA {

class BattleChipUpdater;

class BattleChipModel : public QAbstractListModel {
Q_OBJECT

public:
	BattleChipModel(QObject* parent = nullptr);

	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QStringList mimeTypes() const override;
	virtual QMimeData* mimeData(const QModelIndexList& indices) const override;
	virtual bool dropMimeData(const QMimeData* data, Qt::DropAction, int row, int column, const QModelIndex& parent) override;

	int flavor() const { return m_flavor; }
	QMap<int, QString> chipNames() const { return m_chipIdToName; }

public slots:
	void setFlavor(int);
	void addChip(int id);
	void removeChip(const QModelIndex&);
	void setChips(QList<int> ids);
	void clear();
	void setScale(int);
	void reloadAssets();

private:
	struct BattleChip {
		int id;
		QString name;
		QPixmap icon;
	};

	BattleChip createChip(int id) const;

	QMap<int, QString> m_chipIdToName;
	int m_flavor = 0;
	int m_scale = 1;

	QList<BattleChip> m_deck;
};

}
