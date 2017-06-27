/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputModel.h"

#include "GamepadButtonEvent.h"
#include "InputIndex.h"

#include <QAction>
#include <QKeyEvent>
#include <QMenu>

using namespace QGBA;

InputModel::InputModel(const InputIndex& index, QObject* parent)
	: QAbstractItemModel(parent)
{
	m_root.clone(&index);
}

InputModel::InputModel(QObject* parent)
	: QAbstractItemModel(parent)
{
}

void InputModel::clone(const InputIndex& index) {
	emit beginResetModel();
	m_root.clone(&index);
	emit endResetModel();
}

QVariant InputModel::data(const QModelIndex& index, int role) const {
	if (role != Qt::DisplayRole || !index.isValid()) {
		return QVariant();
	}
	int row = index.row();
	const InputItem* item = static_cast<const InputItem*>(index.internalPointer());
	switch (index.column()) {
	case 0:
		return item->visibleName();
	case 1:
		return QKeySequence(item->shortcut()).toString(QKeySequence::NativeText);
	case 2:
		if (item->button() >= 0) {
			return item->button();
		}
		if (item->axis() >= 0) {
			char d = '\0';
			if (item->direction() == GamepadAxisEvent::POSITIVE) {
				d = '+';
			}
			if (item->direction() == GamepadAxisEvent::NEGATIVE) {
				d = '-';
			}
			return QString("%1%2").arg(d).arg(item->axis());
		}
		break;
	}
	return QVariant();
}

QVariant InputModel::headerData(int section, Qt::Orientation orientation, int role) const {
	if (role != Qt::DisplayRole) {
		return QAbstractItemModel::headerData(section, orientation, role);
	}
	if (orientation == Qt::Horizontal) {
		switch (section) {
		case 0:
			return tr("Action");
		case 1:
			return tr("Keyboard");
		case 2:
			return tr("Gamepad");
		}
	}
	return section;
}

QModelIndex InputModel::index(int row, int column, const QModelIndex& parent) const {
	const InputItem* pmenu = m_root.root();
	if (parent.isValid()) {
		pmenu = static_cast<InputItem*>(parent.internalPointer());
	}
	return createIndex(row, column, const_cast<InputItem*>(pmenu->items()[row]));
}

QModelIndex InputModel::parent(const QModelIndex& index) const {
	if (!index.isValid() || !index.internalPointer()) {
		return QModelIndex();
	}
	InputItem* item = static_cast<InputItem*>(index.internalPointer());
	return this->index(item->parent());
}

QModelIndex InputModel::index(InputItem* item, int row) const {
	if (!item || !item->parent()) {
		return QModelIndex();
	}
	return createIndex(item->parent()->items().indexOf(item), row, item);
}

int InputModel::columnCount(const QModelIndex& index) const {
	return 3;
}

int InputModel::rowCount(const QModelIndex& index) const {
	if (!index.isValid()) {
		return m_root.root()->items().count();
	}
	const InputItem* item = static_cast<const InputItem*>(index.internalPointer());
	return item->items().count();
}

InputItem* InputModel::itemAt(const QModelIndex& index) {
	if (!index.isValid()) {
		return nullptr;
	}
	if (index.internalPointer()) {
		return static_cast<InputItem*>(index.internalPointer());
	}
	if (!index.parent().isValid()) {
		return nullptr;
	}
	InputItem* pmenu = static_cast<InputItem*>(index.parent().internalPointer());
	return pmenu->items()[index.row()];
}

const InputItem* InputModel::itemAt(const QModelIndex& index) const {
	if (!index.isValid()) {
		return nullptr;
	}
	if (index.internalPointer()) {
		return static_cast<InputItem*>(index.internalPointer());
	}
	if (!index.parent().isValid()) {
		return nullptr;
	}
	InputItem* pmenu = static_cast<InputItem*>(index.parent().internalPointer());
	return pmenu->items()[index.row()];
}
