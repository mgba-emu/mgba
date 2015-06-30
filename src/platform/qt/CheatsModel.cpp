/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheatsModel.h"

#include "VFileDevice.h"

#include <QFont>
#include <QSet>

extern "C" {
#include "gba/cheats.h"
}

using namespace QGBA;

CheatsModel::CheatsModel(GBACheatDevice* device, QObject* parent)
	: QAbstractItemModel(parent)
	, m_device(device)
{
}

QVariant CheatsModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid()) {
		return QVariant();
	}

	if (index.parent().isValid()) {
		int row = index.row();
		GBACheatSet* cheats = static_cast<GBACheatSet*>(index.internalPointer());
		const char* line = *StringListGetPointer(&cheats->lines, row);
		switch (role) {
		case Qt::DisplayRole:
			return line;
		case Qt::FontRole:
			return QFont("Courier New", 13);
		default:
			return QVariant();
		}
	}

	int row = index.row();
	const GBACheatSet* cheats = *GBACheatSetsGetPointer(&m_device->cheats, index.row());
	switch (role) {
	case Qt::DisplayRole:
	case Qt::EditRole:
		return cheats->name ? cheats->name : tr("(untitled)");
	case Qt::CheckStateRole:
		return cheats->enabled ? Qt::Checked : Qt::Unchecked;
	default:
		return QVariant();
	}
}

bool CheatsModel::setData(const QModelIndex& index, const QVariant& value, int role) {
	if (!index.isValid() || index.parent().isValid()) {
		return false;
	}

	int row = index.row();
	GBACheatSet* cheats = *GBACheatSetsGetPointer(&m_device->cheats, index.row());
	switch (role) {
	case Qt::DisplayRole:
	case Qt::EditRole:
		if (cheats->name) {
			free(cheats->name);
			cheats->name = nullptr;
		}
		cheats->name = strdup(value.toString().toLocal8Bit().constData());
		emit dataChanged(index, index);
		return true;
	case Qt::CheckStateRole:
		cheats->enabled = value == Qt::Checked;
		emit dataChanged(index, index);
		return true;
	default:
		return false;
	}
}

QModelIndex CheatsModel::index(int row, int column, const QModelIndex& parent) const {
	if (parent.isValid()) {
		return createIndex(row, column, *GBACheatSetsGetPointer(&m_device->cheats, parent.row()));
	} else {
		return createIndex(row, column, nullptr);
	}
}

QModelIndex CheatsModel::parent(const QModelIndex& index) const {
	if (!index.isValid()) {
		return QModelIndex();
	}
	const GBACheatSet* cheats = static_cast<const GBACheatSet*>(index.internalPointer());
	if (!cheats) {
		return QModelIndex();
	}
	for (size_t i = 0; i < GBACheatSetsSize(&m_device->cheats); ++i) {
		if (cheats == *GBACheatSetsGetPointer(&m_device->cheats, i)) {
			return createIndex(i, 0, nullptr);
		}
	}
	return QModelIndex();
}

Qt::ItemFlags CheatsModel::flags(const QModelIndex& index) const {
	if (!index.isValid()) {
		return 0;
	}

	if (index.parent().isValid()) {
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	return Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int CheatsModel::columnCount(const QModelIndex& parent) const {
	return 1;
}

int CheatsModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		if (parent.internalPointer()) {
			return 0;
		}
		const GBACheatSet* set = *GBACheatSetsGetPointer(&m_device->cheats, parent.row());
		return StringListSize(&set->lines);
	}
	return GBACheatSetsSize(&m_device->cheats);
}

GBACheatSet* CheatsModel::itemAt(const QModelIndex& index) {
	if (!index.isValid()) {
		return nullptr;
	}
	if (index.parent().isValid()) {
		return static_cast<GBACheatSet*>(index.internalPointer());
	}
	return *GBACheatSetsGetPointer(&m_device->cheats, index.row());
}

void CheatsModel::removeAt(const QModelIndex& index) {
	if (!index.isValid() || index.parent().isValid()) {
		return;
	}
	int row = index.row();
	GBACheatSet* set = *GBACheatSetsGetPointer(&m_device->cheats, index.row());
	beginRemoveRows(QModelIndex(), row, row);
	GBACheatRemoveSet(m_device, set);
	GBACheatSetDeinit(set);
	delete set;
	endInsertRows();
}

QString CheatsModel::toString(const QModelIndexList& indices) const {
	QMap<int, GBACheatSet*> setOrder;
	QMap<GBACheatSet*, QSet<size_t>> setIndices;
	for (const QModelIndex& index : indices) {
		GBACheatSet* set = static_cast<GBACheatSet*>(index.internalPointer());
		if (!set) {
			set = *GBACheatSetsGetPointer(&m_device->cheats, index.row());
			setOrder[index.row()] = set;
			QSet<size_t> range;
			for (size_t i = 0; i < StringListSize(&set->lines); ++i) {
				range.insert(i);
			}
			setIndices[set] = range;
		} else {
			setOrder[index.parent().row()] = set;
			setIndices[set].insert(index.row());
		}
	}

	QStringList strings;
	QList<int> order = setOrder.keys();
	std::sort(order.begin(), order.end());
	for (int i : order) {
		GBACheatSet* set = setOrder[i];
		QList<size_t> indexOrdex = setIndices[set].toList();
		std::sort(indexOrdex.begin(), indexOrdex.end());
		for (size_t j : indexOrdex) {
			strings.append(*StringListGetPointer(&set->lines, j));
		}
	}

	return strings.join('\n');
}

void CheatsModel::beginAppendRow(const QModelIndex& index) {
	if (index.parent().isValid()) {
		beginInsertRows(index.parent(), rowCount(index.parent()), rowCount(index.parent()));
		return;
	}
	beginInsertRows(index, rowCount(index), rowCount(index));
}

void CheatsModel::endAppendRow() {
	endInsertRows();
}

void CheatsModel::loadFile(const QString& path) {
	VFile* vf = VFileDevice::open(path, O_RDONLY);
	if (!vf) {
		return;
	}
	beginResetModel();
	GBACheatParseFile(m_device, vf);
	endResetModel();
	vf->close(vf);
}

void CheatsModel::saveFile(const QString& path) {
	VFile* vf = VFileDevice::open(path, O_TRUNC | O_CREAT | O_WRONLY);
	if (!vf) {
		return;
	}
	GBACheatSaveFile(m_device, vf);
	vf->close(vf);
}

void CheatsModel::addSet(GBACheatSet* set) {
	beginInsertRows(QModelIndex(), GBACheatSetsSize(&m_device->cheats), GBACheatSetsSize(&m_device->cheats));
	size_t size = GBACheatSetsSize(&m_device->cheats);
	if (size) {
		GBACheatSetCopyProperties(set, *GBACheatSetsGetPointer(&m_device->cheats, size - 1));
	}
	GBACheatAddSet(m_device, set);
	endInsertRows();
}

void CheatsModel::invalidated() {
	beginResetModel();
	endResetModel();
}
