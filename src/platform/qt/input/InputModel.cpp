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

QString InputModel::InputModelItem::visibleName() const {
	if (item) {
		return item->visibleName();
	}
	if (menu) {
		return menu->title()
			.remove(QRegExp("&(?!&)"))
			.remove("...");
	}
	return QString();
}

InputModel::InputModel(const InputIndex& index, QObject* parent)
	: QAbstractItemModel(parent)
{
	clone(index);
}

InputModel::InputModel(QObject* parent)
	: QAbstractItemModel(parent)
{
}

void InputModel::clone(const InputIndex& index) {
	emit beginResetModel();
	m_index.clone(&index);
	m_menus.clear();
	m_topLevelMenus.clear();
	QList<const QMenu*> menus;
	for (auto& item : m_index.items()) {
		const QMenu* menu = item->menu();
		if (!menus.contains(menu)) {
			menus.append(menu);
			m_menus.insert(menu);
		}
	}
	for (auto& menu : menus) {
		if (m_menus.contains(menu->parent())) {
			m_tree[menu->parent()].append(menu);
		} else {
			m_topLevelMenus.append(menu);
		}
	}
	for (auto& item : m_index.items()) {
		const QMenu* menu = item->menu();
		m_tree[menu].append(item);
	}
	emit endResetModel();
}

QVariant InputModel::data(const QModelIndex& index, int role) const {
	if (role != Qt::DisplayRole || !index.isValid()) {
		return QVariant();
	}
	int row = index.row();
	const InputModelItem* item = static_cast<const InputModelItem*>(index.internalPointer());
	switch (index.column()) {
	case 0:
		return item->visibleName();
	case 1:
		if (item->item && item->item->shortcut() > 0) {
			return QKeySequence(item->item->shortcut()).toString(QKeySequence::NativeText);
		}
		break;
	case 2:
		if (!item->item) {
			break;
		}
		if (item->item->button() >= 0) {
			return item->item->button();
		}
		if (item->item->axis() >= 0) {
			char d = '\0';
			if (item->item->direction() == GamepadAxisEvent::POSITIVE) {
				d = '+';
			}
			if (item->item->direction() == GamepadAxisEvent::NEGATIVE) {
				d = '-';
			}
			return QString("%1%2").arg(d).arg(item->item->axis());
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
	if (parent.isValid()) {
		InputModelItem* p = static_cast<InputModelItem*>(parent.internalPointer());
		if (row >= m_tree[p->obj].count()) {
			return QModelIndex();
		}
		return createIndex(row, column, const_cast<InputModelItem*>(&m_tree[p->obj][row]));
	}
	if (row >= m_topLevelMenus.count()) {
		return QModelIndex();
	}
	return createIndex(row, column, const_cast<InputModelItem*>(&m_topLevelMenus[row]));
}

QModelIndex InputModel::parent(const QModelIndex& index) const {
	if (!index.isValid() || !index.internalPointer()) {
		return QModelIndex();
	}
	const QObject* obj = static_cast<const InputModelItem*>(index.internalPointer())->obj;
	if (m_menus.contains(obj->parent())) {
		return this->index(obj->parent());
	} else {
		return QModelIndex();
	}
}

QModelIndex InputModel::index(const QObject* item, int column) const {
	if (!item) {
		return QModelIndex();
	}
	const QObject* parent = item->parent();
	if (parent && m_tree.contains(parent)) {
		int index = m_tree[parent].indexOf(item);
		return createIndex(index, column, const_cast<InputModelItem*>(&m_tree[parent][index]));
	} 
	if (m_topLevelMenus.contains(item)) {
		int index = m_topLevelMenus.indexOf(item);
		return createIndex(index, column, const_cast<InputModelItem*>(&m_topLevelMenus[index]));
	}
	return QModelIndex();
}

int InputModel::columnCount(const QModelIndex& index) const {
	return 3;
}

int InputModel::rowCount(const QModelIndex& index) const {
	if (!index.isValid() || !index.internalPointer()) {
		return m_topLevelMenus.count();
	}
	const QObject* obj = static_cast<const InputModelItem*>(index.internalPointer())->obj;
	if (!m_tree.contains(obj)) {
		return 0;
	}
	return m_tree[obj].count();
}

InputItem* InputModel::itemAt(const QModelIndex& index) {
	if (!index.isValid() || !index.internalPointer()) {
		return nullptr;
	}
	return static_cast<InputModelItem*>(index.internalPointer())->item;
}

const InputItem* InputModel::itemAt(const QModelIndex& index) const {
	if (!index.isValid() || !index.internalPointer()) {
		return nullptr;
	}
	return static_cast<const InputModelItem*>(index.internalPointer())->item;

}
