/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "BattleChipModel.h"

#include "ConfigController.h"
#include "GBAApp.h"

#include <QFile>
#include <QMimeData>
#include <QResource>

using namespace QGBA;

BattleChipModel::BattleChipModel(QObject* parent)
	: QAbstractListModel(parent)
{
	QResource::registerResource(GBAApp::dataDir() + "/chips.rcc", "/exe");
	QResource::registerResource(ConfigController::configDir() + "/chips.rcc", "/exe");
}

int BattleChipModel::rowCount(const QModelIndex& parent) const {
	if (parent.isValid()) {
		return 0;
	}
	return m_deck.count();
}

QVariant BattleChipModel::data(const QModelIndex& index, int role) const {
	const BattleChip& item = m_deck[index.row()];

	switch (role) {
	case Qt::DisplayRole:
		return item.name;
	case Qt::DecorationRole:
		return item.icon.scaled(item.icon.size() * m_scale);
	case Qt::UserRole:
		return item.id;
	}
	return QVariant();
}

Qt::ItemFlags BattleChipModel::flags(const QModelIndex&) const {
	return Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren;
}

bool BattleChipModel::removeRows(int row, int count, const QModelIndex& parent) {
	if (parent.isValid()) {
		return false;
	}
	beginRemoveRows(QModelIndex(), row, row + count - 1);
	for (int i = 0; i < count; ++i) {
		m_deck.removeAt(row);
	}
	endRemoveRows();
	return true;
}

QStringList BattleChipModel::mimeTypes() const {
	return {"text/plain"};
}

Qt::DropActions BattleChipModel::supportedDropActions() const {
	return Qt::MoveAction;
}

QMimeData* BattleChipModel::mimeData(const QModelIndexList& indices) const {
	QStringList deck;
	for (const QModelIndex& index : indices) {
		if (index.parent().isValid()) {
			continue;
		}
		deck.append(QString::number(m_deck[index.row()].id));
	}

	QMimeData* mimeData = new QMimeData();
	mimeData->setData("text/plain", deck.join(',').toLocal8Bit());
	return mimeData;
}

bool BattleChipModel::dropMimeData(const QMimeData* data, Qt::DropAction, int row, int, const QModelIndex& parent) {
	if (parent.parent().isValid()) {
		return false;
	}
	QStringList deck = QString::fromLocal8Bit(data->data("text/plain")).split(',');
	if (deck.isEmpty()) {
		return true;
	}

	row = parent.row();
	beginInsertRows(QModelIndex(), row, row + deck.count() - 1);
	for (int i = 0; i < deck.count(); ++i) {
		int id = deck[i].toInt();
		m_deck.insert(row + i, createChip(id));
	}
	endInsertRows();
	return true;
}

void BattleChipModel::setFlavor(int flavor) {
	m_chipIdToName.clear();
	if (flavor == GBA_FLAVOR_BEAST_LINK_GATE_US) {
		flavor = GBA_FLAVOR_BEAST_LINK_GATE;
	}
	m_flavor = flavor;

	QFile file(QString(":/exe/exe%1/chip-names.txt").arg(flavor));
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	int id = 0;
	while (true) {
		QByteArray line = file.readLine();
		if (line.isEmpty()) {
			break;
		}
		++id;
		if (line.trimmed().isEmpty()) {
			continue;
		}
		QString name = QString::fromUtf8(line).trimmed();
		m_chipIdToName[id] = name;
	}

}

void BattleChipModel::addChip(int id) {
	beginInsertRows(QModelIndex(), m_deck.count(), m_deck.count());
	m_deck.append(createChip(id));
	endInsertRows();
}

void BattleChipModel::removeChip(const QModelIndex& index) {
	beginRemoveRows(QModelIndex(), index.row(), index.row());
	m_deck.removeAt(index.row());
	endRemoveRows();
}

void BattleChipModel::setChips(QList<int> ids) {
	beginResetModel();
	m_deck.clear();
	for (int id : ids) {
		m_deck.append(createChip(id));
	}
	endResetModel();
}

void BattleChipModel::clear() {
	beginResetModel();
	m_deck.clear();
	endResetModel();
}

void BattleChipModel::setScale(int scale) {
	m_scale = scale;
}

void BattleChipModel::reloadAssets() {
	QResource::unregisterResource(ConfigController::configDir() + "/chips.rcc", "/exe");
	QResource::unregisterResource(GBAApp::dataDir() + "/chips.rcc", "/exe");

	QResource::registerResource(GBAApp::dataDir() + "/chips.rcc", "/exe");
	QResource::registerResource(ConfigController::configDir() + "/chips.rcc", "/exe");

	emit layoutAboutToBeChanged();
	setFlavor(m_flavor);
	for (int i = 0; i < m_deck.count(); ++i) {
		m_deck[i] = createChip(m_deck[i].id);
	}
	emit layoutChanged();
}

BattleChipModel::BattleChip BattleChipModel::createChip(int id) const {
	QString path = QString(":/exe/exe%1/%2.png").arg(m_flavor).arg(id, 3, 10, QLatin1Char('0'));
	if (!QFile(path).exists()) {
		path = QString(":/exe/exe%1/placeholder.png").arg(m_flavor);
	}
	QPixmap icon(path);

	BattleChip chip = {
		id,
		m_chipIdToName[id],
		icon
	};
	return chip;
}
