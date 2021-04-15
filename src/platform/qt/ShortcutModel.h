/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAbstractListModel>

namespace QGBA {

class ShortcutController;
class Shortcut;

class ShortcutModel : public QAbstractItemModel {
Q_OBJECT

public:
	ShortcutModel(QObject* parent = nullptr);

	void setController(ShortcutController* controller);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;

	QString name(const QModelIndex&) const;

private slots:
	void addRowNamed(const QString&);
	void clearMenu(const QString&);

private:
	ShortcutController* m_controller = nullptr;

	struct Item {
		QString name;
		const Shortcut* shortcut = nullptr;
	};

	mutable QHash<QString, Item> m_cache;
};

}