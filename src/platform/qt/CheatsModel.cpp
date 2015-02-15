/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "CheatsModel.h"

extern "C" {
#include "gba/cheats.h"
#include "util/vfs.h"
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

	int row = index.row();
	const GBACheatSet* cheats = static_cast<const GBACheatSet*>(index.internalPointer());
	switch (role) {
	case Qt::DisplayRole:
	case Qt::EditRole:
		return cheats->name ? cheats->name : tr("(untitled)");
	case Qt::CheckStateRole:
		return Qt::Checked;
	default:
		return QVariant();
	}
}

bool CheatsModel::setData(const QModelIndex& index, const QVariant& value, int role) {
	if (!index.isValid()) {
		return false;
	}

	int row = index.row();
	GBACheatSet* cheats = static_cast<GBACheatSet*>(index.internalPointer());
	switch (role) {
	case Qt::DisplayRole:
	case Qt::EditRole:
		if (cheats->name) {
			free(cheats->name);
			cheats->name = nullptr;
		}
		cheats->name = strdup(value.toString().toLocal8Bit().constData());
		return true;
	case Qt::CheckStateRole:
		return false;
	default:
		return false;
	}
}

QModelIndex CheatsModel::index(int row, int column, const QModelIndex& parent) const {
	return createIndex(row, column, *GBACheatSetsGetPointer(&m_device->cheats, row));
}

QModelIndex CheatsModel::parent(const QModelIndex& index) const {
	return QModelIndex();
}

Qt::ItemFlags CheatsModel::flags(const QModelIndex &index) const {
	if (!index.isValid()) {
		return 0;
	}

	return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

int CheatsModel::columnCount(const QModelIndex& parent) const {
	return 1;
}

int CheatsModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return GBACheatSetsSize(&m_device->cheats);
}

GBACheatSet* CheatsModel::itemAt(const QModelIndex& index) {
	if (!index.isValid()) {
		return nullptr;
	}
	return static_cast<GBACheatSet*>(index.internalPointer());
}

void CheatsModel::loadFile(const QString& path) {
	VFile* vf = VFileOpen(path.toLocal8Bit().constData(), O_RDONLY);
	if (!vf) {
		return;
	}
	beginResetModel();
	GBACheatParseFile(m_device, vf);
	endResetModel();
	vf->close(vf);
}

void CheatsModel::addSet(GBACheatSet* set) {
	beginInsertRows(QModelIndex(), GBACheatSetsSize(&m_device->cheats), GBACheatSetsSize(&m_device->cheats) + 1);
	*GBACheatSetsAppend(&m_device->cheats) = set;
	endInsertRows();
}

void CheatsModel::invalidated() {
	beginResetModel();
	endResetModel();
}
