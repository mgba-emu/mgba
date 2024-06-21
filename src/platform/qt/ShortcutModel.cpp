/* Copyright (c) 2013-2019 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutModel.h"

#include "ShortcutController.h"
#include "utils.h"

using namespace QGBA;

ShortcutModel::ShortcutModel(QObject* parent)
	: QAbstractItemModel(parent)
{
}

void ShortcutModel::setController(ShortcutController* controller) {
	beginResetModel();
	m_controller = controller;
	m_cache.clear();
	connect(controller, &ShortcutController::shortcutAdded, this, &ShortcutModel::addRowNamed);
	connect(controller, &ShortcutController::menuCleared, this, &ShortcutModel::clearMenu);
	endResetModel();
}

QVariant ShortcutModel::data(const QModelIndex& index, int role) const {
	if (role != Qt::DisplayRole || !index.isValid()) {
		return QVariant();
	}
	const Item* item = static_cast<Item*>(index.internalPointer());
	const Shortcut* shortcut = item->shortcut;
	switch (index.column()) {
	case 0:
		return m_controller->visibleName(item->name);
	case 1:
		return shortcut ? keyName(shortcut->shortcut()) : QVariant();
	case 2:
		if (!shortcut) {
			return QVariant();
		}
		if (shortcut->button() >= 0) {
			return shortcut->button();
		}
		if (shortcut->axis() >= 0) {
			char d = '\0';
			if (shortcut->direction() == GamepadAxisEvent::POSITIVE) {
				d = '+';
			}
			if (shortcut->direction() == GamepadAxisEvent::NEGATIVE) {
				d = '-';
			}
			return QString("%1%2").arg(d).arg(shortcut->axis());
		}
		break;
	}
	return QVariant();
}

QVariant ShortcutModel::headerData(int section, Qt::Orientation orientation, int role) const {
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

QModelIndex ShortcutModel::index(int row, int column, const QModelIndex& parent) const {
	QString pmenu;
	if (parent.isValid()) {
		pmenu = static_cast<Item*>(parent.internalPointer())->name;
	}
	QString name = m_controller->name(row, pmenu);
	Item* item = &m_cache[name];
	item->name = name;
	item->shortcut = m_controller->shortcut(name);
	return createIndex(row, column, item);
}

QModelIndex ShortcutModel::parent(const QModelIndex& index) const {
	if (!index.isValid() || !index.internalPointer()) {
		return QModelIndex();
	}
	Item* item = static_cast<Item*>(index.internalPointer());
	QString parent = m_controller->parent(item->name);
	if (parent.isNull()) {
		return QModelIndex();
	}
	Item* pitem = &m_cache[parent];
	pitem->name = parent;
	pitem->shortcut = m_controller->shortcut(parent);
	return createIndex(m_controller->indexIn(parent), 0, pitem);
}

int ShortcutModel::columnCount(const QModelIndex&) const {
	return 3;
}

int ShortcutModel::rowCount(const QModelIndex& index) const {
	if (!index.isValid()) {
		return m_controller->count();
	}
	Item* item = static_cast<Item*>(index.internalPointer());
	return m_controller->count(item->name);
}

QString ShortcutModel::name(const QModelIndex& index) const {
	if (!index.isValid()) {
		return {};
	}
	Item* item = static_cast<Item*>(index.internalPointer());
	return item->name;
}

void ShortcutModel::addRowNamed(const QString& name) {
	QString parent = m_controller->parent(name);
	Item* item = &m_cache[parent];
	item->name = parent;
	item->shortcut = m_controller->shortcut(parent);
	int index = m_controller->indexIn(name);
	beginInsertRows(createIndex(m_controller->indexIn(parent), 0, item), index, index + 1);
	endInsertRows();
}

void ShortcutModel::clearMenu(const QString&) {
	// TODO
	beginResetModel();
	endResetModel();
}
