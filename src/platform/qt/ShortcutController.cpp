/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutController.h"

#include <QAction>
#include <QMenu>

using namespace QGBA;

ShortcutController::ShortcutController(QObject* parent)
	: QAbstractItemModel(parent)
{
}

QVariant ShortcutController::data(const QModelIndex& index, int role) const {
	if (role != Qt::DisplayRole || !index.isValid()) {
		return QVariant();
	}
	const QModelIndex& parent = index.parent();
	if (parent.isValid()) {
		const ShortcutMenu& menu = m_menus[parent.row()];
		const ShortcutItem& item = menu.shortcuts()[index.row()];
		switch (index.column()) {
		case 0:
			return item.visibleName();
		case 1:
			return item.action()->shortcut().toString(QKeySequence::NativeText);
		case 2:
			if (item.button() >= 0) {
				return item.button();
			}
			return QVariant();
		}
	} else if (index.column() == 0) {
		return m_menus[index.row()].visibleName();
	}
	return QVariant();
}

QVariant ShortcutController::headerData(int section, Qt::Orientation orientation, int role) const {
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

QModelIndex ShortcutController::index(int row, int column, const QModelIndex& parent) const {
	if (!parent.isValid()) {
		return createIndex(row, column, -1);
	}
	return createIndex(row, column, parent.row());
}

QModelIndex ShortcutController::parent(const QModelIndex& index) const {
	if (!index.isValid()) {
		return QModelIndex();
	}
	if (index.internalId() == -1) {
		return QModelIndex();
	}
	return createIndex(index.internalId(), 0, -1);
}

int ShortcutController::columnCount(const QModelIndex& index) const {
	return 3;
}

int ShortcutController::rowCount(const QModelIndex& index) const {
	if (index.parent().isValid()) {
		return 0;
	}
	if (index.isValid()) {
		return m_menus[index.row()].shortcuts().count();
	}
	return m_menus.count();
}

void ShortcutController::addAction(QMenu* menu, QAction* action, const QString& name) {
	ShortcutMenu* smenu = nullptr;
	int row = 0;
	for (auto iter = m_menus.end(); iter-- != m_menus.begin(); ++row) {
		if (iter->menu() == menu) {
			smenu = &(*iter);
			break;
		}
	}
	if (!smenu) {
		return;
	}
	QModelIndex parent = createIndex(row, 0, -1);
	beginInsertRows(parent, smenu->shortcuts().count(), smenu->shortcuts().count());
	smenu->addAction(action, name);
	endInsertRows();
	emit dataChanged(createIndex(smenu->shortcuts().count() - 1, 0, row), createIndex(smenu->shortcuts().count() - 1, 2, row));
}

void ShortcutController::addMenu(QMenu* menu) {
	beginInsertRows(QModelIndex(), m_menus.count(), m_menus.count());
	m_menus.append(ShortcutMenu(menu));
	endInsertRows();
	emit dataChanged(createIndex(m_menus.count() - 1, 0, -1), createIndex(m_menus.count() - 1, 0, -1));
}

const QAction* ShortcutController::actionAt(const QModelIndex& index) const {
	if (!index.isValid()) {
		return nullptr;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return nullptr;
	}
	if (parent.row() > m_menus.count()) {
		return nullptr;
	}
	const ShortcutMenu& menu = m_menus[parent.row()];
	if (index.row() > menu.shortcuts().count()) {
		return nullptr;
	}
	const ShortcutItem& item = menu.shortcuts()[index.row()];
	return item.action();
}

void ShortcutController::updateKey(const QModelIndex& index, const QKeySequence& keySequence) {
	if (!index.isValid()) {
		return;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return;
	}
	ShortcutMenu& menu = m_menus[parent.row()];
	ShortcutItem& item = menu.shortcuts()[index.row()];
	item.action()->setShortcut(keySequence);
	emit dataChanged(createIndex(index.row(), 0, index.internalId()), createIndex(index.row(), 2, index.internalId()));
}

void ShortcutController::updateButton(const QModelIndex& index, int button) {
	if (!index.isValid()) {
		return;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return;
	}
	ShortcutMenu& menu = m_menus[parent.row()];
	ShortcutItem& item = menu.shortcuts()[index.row()];
	int oldButton = item.button();
	item.setButton(button);
	if (oldButton >= 0) {
		m_buttons.take(oldButton);
	}
	m_buttons[button] = &item;
	emit dataChanged(createIndex(index.row(), 0, index.internalId()), createIndex(index.row(), 2, index.internalId()));
}

void ShortcutController::pressButton(int button) {
	auto item = m_buttons.find(button);
	if (item == m_buttons.end()) {
		return;
	}
	QAction* action = item.value()->action();
	if (!action->isEnabled()) {
		return;
	}
	action->trigger();
}

ShortcutController::ShortcutItem::ShortcutItem(QAction* action, const QString& name)
	: m_action(action)
	, m_name(name)
	, m_button(-1)
{
	m_visibleName = action->text()
		.remove(QRegExp("&(?!&)"))
		.remove("...");
}

ShortcutController::ShortcutMenu::ShortcutMenu(QMenu* menu)
	: m_menu(menu)
{
	m_visibleName = menu->title()
		.remove(QRegExp("&(?!&)"))
		.remove("...");
}

void ShortcutController::ShortcutMenu::addAction(QAction* action, const QString& name) {
	m_shortcuts.append(ShortcutItem(action, name));
}
