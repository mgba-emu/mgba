/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAbstractItemModel>

#include "LogController.h"

namespace QGBA {

class ConfigController;

class LogConfigModel : public QAbstractItemModel {
Q_OBJECT

public:
	LogConfigModel(LogController*, QObject* parent = nullptr);

	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;

	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

	LogController* logger() { return m_controller; }

public slots:
	void reset();
	void save(ConfigController*);

private:
	struct ConfigSetting {
		int index;
		QString name;
		const char* id;
		int levels;

		bool operator<(const ConfigSetting& other) const {
			return name < other.name;
		}
	};

	LogController* m_controller;

	QList<ConfigSetting> m_cache;
	int m_levels;
};

}
