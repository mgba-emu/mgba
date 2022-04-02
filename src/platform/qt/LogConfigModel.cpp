/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogConfigModel.h"

#include <algorithm>

using namespace QGBA;

LogConfigModel::LogConfigModel(LogController* controller, QObject* parent)
	: QAbstractItemModel(parent)
	, m_controller(controller)
{
	for (int i = 0; mLogCategoryId(i); ++i) {
		int levels = controller->levels(i);
		m_cache.append({ i, mLogCategoryName(i), mLogCategoryId(i), levels ? levels : -1 });
	}
	std::sort(m_cache.begin(), m_cache.end());
	m_levels = m_controller->levels();
}

QVariant LogConfigModel::data(const QModelIndex& index, int role) const {
	if (role != Qt::CheckStateRole) {
		return QVariant();
	}
	int levels;
	if (index.row() == 0) {
		if (index.column() == 0) {
			return QVariant();
		}
		levels = m_levels;
	} else {
		levels = m_cache[index.row() - 1].levels;
	}
	if (index.column() == 0) {
		return levels < 0 ? Qt::Checked : Qt::Unchecked;
	} else if (levels < 0 && index.row() > 0) {
		return (m_levels >> (index.column() - 1)) & 1 ? Qt::PartiallyChecked : Qt::Unchecked;
	} else {
		return (levels >> (index.column() - 1)) & 1 ? Qt::Checked : Qt::Unchecked;
	}
}

bool LogConfigModel::setData(const QModelIndex& index, const QVariant& value, int role) {
	if (role != Qt::CheckStateRole) {
		return false;
	}
	int levels;
	if (index.row() == 0) {
		if (index.column() == 0) {
			return false;
		}
		levels = m_levels;
	} else {
		levels = m_cache[index.row() - 1].levels;
	}
	if (index.column() == 0) {
		 levels = -1;
	} else {
		if (levels < 0) {
			levels = m_levels;
		}
		int bit = 1 << (index.column() - 1);
		if (value.value<Qt::CheckState>() == Qt::Unchecked) {
			levels &= ~bit;
		} else {
			levels |= bit;
		}
	}
	if (index.row() == 0) {
		beginResetModel();
		m_levels = levels;
		endResetModel();
	} else {
		m_cache[index.row() - 1].levels = levels;
		emit dataChanged(createIndex(0, index.row(), nullptr), createIndex(8, index.row(), nullptr));
	}
	return true;
}

QVariant LogConfigModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (role != Qt::DisplayRole) {
		return QVariant();
	}
	if (orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("Default");
		case 1:
			return tr("Fatal");
		case 2:
			return tr("Error");
		case 3:
			return tr("Warning");
		case 4:
			return tr("Info");
		case 5:
			return tr("Debug");
		case 6:
			return tr("Stub");
		case 7:
			return tr("Game Error");
		default:
			return QVariant();
		}
	} else if (section) {
		return m_cache[section - 1].name;
	} else {
		return tr("Default");
	}
}

QModelIndex LogConfigModel::index(int row, int column, const QModelIndex& parent) const {
	if (parent.isValid()) {
		return QModelIndex();
	}
	return createIndex(row, column, nullptr);
}

QModelIndex LogConfigModel::parent(const QModelIndex&) const {
	return QModelIndex();
}

int LogConfigModel::columnCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return 8;
}

int LogConfigModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return m_cache.size() + 1;
}

Qt::ItemFlags LogConfigModel::flags(const QModelIndex& index) const {
	if (!index.isValid() || (index.row() == 0 && index.column() == 0)) {
		return Qt::NoItemFlags;
	}
	return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
}

void LogConfigModel::reset() {
	beginResetModel();
	for (auto& row : m_cache) {
		row.levels = m_controller->levels(row.index);
		if (!row.levels) {
			row.levels = -1;
		}
	}
	m_levels = m_controller->levels();
	endResetModel();
}

void LogConfigModel::save(ConfigController* config) {
	for (auto& row : m_cache) {
		if (row.levels < 0) {
			m_controller->clearLevels(row.index);
		} else {
			m_controller->setLevels(row.levels, row.index);
		}
	}
	m_controller->setLevels(m_levels);
	m_controller->save(config);
}
