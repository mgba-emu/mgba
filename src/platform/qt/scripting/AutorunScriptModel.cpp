/* Copyright (c) 2013-2025 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "scripting/AutorunScriptModel.h"

#include "ConfigController.h"
#include "LogController.h"

using namespace QGBA;

AutorunScriptModel::AutorunScriptModel(ConfigController* config, QObject* parent)
	: QAbstractListModel(parent)
	, m_config(config)
{
	QList<QVariant> autorun = m_config->getList("autorunSettings");
	for (const auto& item: autorun) {
		if (!item.canConvert<ScriptInfo>()) {
			continue;
		}
		m_scripts.append(qvariant_cast<ScriptInfo>(item));
	}
}

int AutorunScriptModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return m_scripts.count();
}

bool AutorunScriptModel::setData(const QModelIndex& index, const QVariant& data, int role) {
	if (!index.isValid() || index.parent().isValid() || index.row() >= m_scripts.count()) {
		return {};
	}

	switch (role) {
	case Qt::CheckStateRole:
		m_scripts[index.row()].active = data.value<Qt::CheckState>() == Qt::Checked;
		save();
		return true;
	}
	return false;

}

QVariant AutorunScriptModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid() || index.parent().isValid() || index.row() >= m_scripts.count()) {
		return {};
	}

	switch (role) {
	case Qt::DisplayRole:
		return m_scripts.at(index.row()).filename;
	case Qt::CheckStateRole:
		return m_scripts.at(index.row()).active ? Qt::Checked : Qt::Unchecked;
	}
	return {};
}

Qt::ItemFlags AutorunScriptModel::flags(const QModelIndex& index) const {
	if (!index.isValid() || index.parent().isValid()) {
		return Qt::NoItemFlags;
	}
	return Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
}

bool AutorunScriptModel::removeRows(int row, int count, const QModelIndex& parent) {
	if (parent.isValid()) {
		return false;
	}
	if (m_scripts.size() < row) {
		return false;
	}
	if (m_scripts.size() < row + count) {
		count = m_scripts.size() - row;
	}
	m_scripts.erase(m_scripts.begin() + row, m_scripts.begin() + row + count);
	save();
	return true;
}

bool AutorunScriptModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count, const QModelIndex& destinationParent, int destinationChild) {
	if (sourceParent.isValid() || destinationParent.isValid()) {
		return false;
	}

	if (sourceRow < 0 || destinationChild < 0) {
		return false;
	}

	if (sourceRow >= m_scripts.size() || destinationChild >= m_scripts.size()) {
		return false;
	}

	if (count > 1) {
		LOG(QT, WARN) << tr("Moving more than one row at once is not yet supported");
		return false;
	}

	auto item = m_scripts.takeAt(sourceRow);
	m_scripts.insert(destinationChild, item);
	save();
	return true;
}

void AutorunScriptModel::addScript(const QString& filename) {
	beginInsertRows({}, m_scripts.count(), m_scripts.count());
	m_scripts.append(ScriptInfo { filename, true });
	endInsertRows();
	save();
}

QList<QString> AutorunScriptModel::activeScripts() const {
	QList<QString> scripts;
	for (const auto& pair: m_scripts) {
		if (!pair.active) {
			continue;
		}
		scripts.append(pair.filename);
	}
	return scripts;
}

void AutorunScriptModel::save() {
	QList<QVariant> list;
	for (const auto& script : m_scripts) {
		list.append(QVariant::fromValue(script));
	}
	m_config->setList("autorunSettings", list);
}
