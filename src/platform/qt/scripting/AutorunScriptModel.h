/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QAbstractListModel>
#include <QDataStream>

namespace QGBA {

class ConfigController;

class AutorunScriptModel : public QAbstractListModel {
Q_OBJECT

public:
	struct ScriptInfo {
		static const uint16_t VERSION = 1;

		QString filename;
		bool active;
	};

	static void registerMetaTypes();

	AutorunScriptModel(QObject* parent = nullptr);

	void deserialize(const QList<QVariant>& autorun);
	QList<QVariant> serialize() const;

	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& data, int role = Qt::DisplayRole) override;
	virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual Qt::DropActions supportedDropActions() const override;

	virtual bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
	virtual bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild) override;

	void addScript(const QString& filename);
	QStringList activeScripts() const;

signals:
	void scriptsChanged(const QList<QVariant>& serialized);

private:
	QList<ScriptInfo> m_scripts;

	void emitScriptsChanged();
};

}

Q_DECLARE_METATYPE(QGBA::AutorunScriptModel::ScriptInfo);
QDataStream& operator<<(QDataStream& stream, const QGBA::AutorunScriptModel::ScriptInfo& object);
QDataStream& operator>>(QDataStream& stream, QGBA::AutorunScriptModel::ScriptInfo& object);
