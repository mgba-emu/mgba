/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheatsModel.h"

#include "GBAApp.h"
#include "LogController.h"
#include "VFileDevice.h"

#include <QSet>

#include <mgba/core/cheats.h>
#include <mgba-util/vfs.h>

using namespace QGBA;

CheatsModel::CheatsModel(mCheatDevice* device, QObject* parent)
	: QAbstractItemModel(parent)
	, m_device(device)
{
	m_font = GBAApp::app()->monospaceFont();
}

QVariant CheatsModel::data(const QModelIndex& index, int role) const {
	if (!index.isValid()) {
		return QVariant();
	}

	if (index.parent().isValid()) {
		int row = index.row();
		mCheatSet* cheats = static_cast<mCheatSet*>(index.internalPointer());
		const char* line = *StringListGetPointer(&cheats->lines, row);
		switch (role) {
		case Qt::DisplayRole:
			return line;
		case Qt::FontRole:
			return m_font;
		default:
			return QVariant();
		}
	}

	if ((size_t) index.row() >= mCheatSetsSize(&m_device->cheats)) {
		return QVariant();
	}

	const mCheatSet* cheats = *mCheatSetsGetPointer(&m_device->cheats, index.row());
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
	if (!index.isValid() || index.parent().isValid() || (size_t) index.row() > mCheatSetsSize(&m_device->cheats)) {
		return false;
	}

	mCheatSet* cheats = *mCheatSetsGetPointer(&m_device->cheats, index.row());
	switch (role) {
	case Qt::DisplayRole:
	case Qt::EditRole:
		mCheatSetRename(cheats, value.toString().toUtf8().constData());
		mCheatAutosave(m_device);
		emit dataChanged(index, index);
		return true;
	case Qt::CheckStateRole:
		cheats->enabled = value == Qt::Checked;
		mCheatAutosave(m_device);
		emit dataChanged(index, index);
		return true;
	default:
		return false;
	}
}

QModelIndex CheatsModel::index(int row, int column, const QModelIndex& parent) const {
	if (parent.isValid()) {
		return createIndex(row, column, *mCheatSetsGetPointer(&m_device->cheats, parent.row()));
	} else {
		return createIndex(row, column, nullptr);
	}
}

QModelIndex CheatsModel::parent(const QModelIndex& index) const {
	if (!index.isValid()) {
		return QModelIndex();
	}
	const mCheatSet* cheats = static_cast<const mCheatSet*>(index.internalPointer());
	if (!cheats) {
		return QModelIndex();
	}
	for (size_t i = 0; i < mCheatSetsSize(&m_device->cheats); ++i) {
		if (cheats == *mCheatSetsGetPointer(&m_device->cheats, i)) {
			return createIndex(i, 0, nullptr);
		}
	}
	return QModelIndex();
}

Qt::ItemFlags CheatsModel::flags(const QModelIndex& index) const {
	if (!index.isValid()) {
		return Qt::NoItemFlags;
	}

	if (index.parent().isValid()) {
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	return Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int CheatsModel::columnCount(const QModelIndex&) const {
	return 1;
}

int CheatsModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		if (parent.internalPointer()) {
			return 0;
		}
		const mCheatSet* set = *mCheatSetsGetPointer(&m_device->cheats, parent.row());
		return StringListSize(&set->lines);
	}
	return mCheatSetsSize(&m_device->cheats);
}

mCheatSet* CheatsModel::itemAt(const QModelIndex& index) {
	if (!index.isValid()) {
		return nullptr;
	}
	if (index.parent().isValid()) {
		return static_cast<mCheatSet*>(index.internalPointer());
	}
	if ((size_t) index.row() >= mCheatSetsSize(&m_device->cheats)) {
		return nullptr;
	}
	return *mCheatSetsGetPointer(&m_device->cheats, index.row());
}

void CheatsModel::removeAt(const QModelIndex& index) {
	if (!index.isValid() || index.parent().isValid() || (size_t) index.row() >= mCheatSetsSize(&m_device->cheats)) {
		return;
	}
	int row = index.row();
	mCheatSet* set = *mCheatSetsGetPointer(&m_device->cheats, index.row());
	beginRemoveRows(QModelIndex(), row, row);
	mCheatRemoveSet(m_device, set);
	mCheatSetDeinit(set);
	endRemoveRows();
	mCheatAutosave(m_device);
}

QString CheatsModel::toString(const QModelIndexList& indices) const {
	QMap<int, mCheatSet*> setOrder;
	QMap<mCheatSet*, QSet<size_t>> setIndices;
	for (const QModelIndex& index : indices) {
		mCheatSet* set = static_cast<mCheatSet*>(index.internalPointer());
		if (!set) {
			set = *mCheatSetsGetPointer(&m_device->cheats, index.row());
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
		mCheatSet* set = setOrder[i];
		QList<size_t> indexOrdex = setIndices[set].values();
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
	mCheatAutosave(m_device);
}

void CheatsModel::loadFile(const QString& path) {
	VFile* vf = VFileDevice::open(path, O_RDONLY);
	if (!vf) {
		LOG(QT, WARN) << tr("Failed to open cheats file: %1").arg(path);
		return;
	}
	beginResetModel();
	mCheatParseFile(m_device, vf);
	endResetModel();
	vf->close(vf);
}

void CheatsModel::saveFile(const QString& path) {
	VFile* vf = VFileDevice::open(path, O_TRUNC | O_CREAT | O_WRONLY);
	if (!vf) {
		return;
	}
	mCheatSaveFile(m_device, vf);
	vf->close(vf);
}

void CheatsModel::addSet(mCheatSet* set) {
	beginInsertRows(QModelIndex(), mCheatSetsSize(&m_device->cheats), mCheatSetsSize(&m_device->cheats));
	size_t size = mCheatSetsSize(&m_device->cheats);
	if (size) {
		set->copyProperties(set, *mCheatSetsGetPointer(&m_device->cheats, size - 1));
	}
	mCheatAddSet(m_device, set);
	endInsertRows();
	mCheatAutosave(m_device);
}

void CheatsModel::invalidated() {
	beginResetModel();
	endResetModel();
}
