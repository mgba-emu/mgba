/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutController.h"

#include "GamepadButtonEvent.h"

#include <QAction>
#include <QMenu>

using namespace QGBA;

ShortcutController::ShortcutController(QObject* parent)
	: QAbstractItemModel(parent)
	, m_rootMenu(nullptr)
{
}

QVariant ShortcutController::data(const QModelIndex& index, int role) const {
	if (role != Qt::DisplayRole || !index.isValid()) {
		return QVariant();
	}
	int row = index.row();
	const ShortcutItem* item = static_cast<const ShortcutItem*>(index.internalPointer());
	switch (index.column()) {
	case 0:
		return item->visibleName();
	case 1:
		if (item->action()) {
			return item->action()->shortcut().toString(QKeySequence::NativeText);
		}
		break;
	case 2:
		if (item->button() >= 0) {
			return item->button();
		}
		break;
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
	const ShortcutItem* pmenu = &m_rootMenu;
	if (parent.isValid()) {
		pmenu = static_cast<ShortcutItem*>(parent.internalPointer());
	}
	return createIndex(row, column, const_cast<ShortcutItem*>(&pmenu->items()[row]));
}

QModelIndex ShortcutController::parent(const QModelIndex& index) const {
	if (!index.isValid() || !index.internalPointer()) {
		return QModelIndex();
	}
	ShortcutItem* item = static_cast<ShortcutItem*>(index.internalPointer());
	if (!item->parent() || !item->parent()->parent()) {
		return QModelIndex();
	}
	return createIndex(item->parent()->parent()->items().indexOf(*item->parent()), 0, item->parent());
}

int ShortcutController::columnCount(const QModelIndex& index) const {
	return 3;
}

int ShortcutController::rowCount(const QModelIndex& index) const {
	if (!index.isValid()) {
		return m_rootMenu.items().count();
	}
	const ShortcutItem* item = static_cast<const ShortcutItem*>(index.internalPointer());
	return item->items().count();
}

void ShortcutController::addAction(QMenu* menu, QAction* action, const QString& name) {
	ShortcutItem* smenu = m_menuMap[menu];
	if (!smenu) {
		return;
	}
	ShortcutItem* pmenu = smenu->parent();
	int row = pmenu->items().indexOf(*smenu);
	QModelIndex parent = createIndex(row, 0, smenu);
	beginInsertRows(parent, smenu->items().count(), smenu->items().count());
	smenu->addAction(action, name);
	endInsertRows();
	ShortcutItem* item = &smenu->items().last();
	emit dataChanged(createIndex(smenu->items().count() - 1, 0, item), createIndex(smenu->items().count() - 1, 2, item));
}

void ShortcutController::addMenu(QMenu* menu, QMenu* parentMenu) {
	ShortcutItem* smenu = m_menuMap[parentMenu];
	if (!smenu) {
		smenu = &m_rootMenu;
	}
	QModelIndex parent;
	ShortcutItem* pmenu = smenu->parent();
	if (pmenu) {
		int row = pmenu->items().indexOf(*smenu);
		parent = createIndex(row, 0, smenu);
	}
	beginInsertRows(parent, smenu->items().count(), smenu->items().count());
	smenu->addSubmenu(menu);
	endInsertRows();
	ShortcutItem* item = &smenu->items().last();
	emit dataChanged(createIndex(smenu->items().count() - 1, 0, item), createIndex(smenu->items().count() - 1, 2, item));
	m_menuMap[menu] = item;
}

ShortcutController::ShortcutItem* ShortcutController::itemAt(const QModelIndex& index) {
	if (!index.isValid()) {
		return nullptr;
	}
	return static_cast<ShortcutItem*>(index.internalPointer());
}

const ShortcutController::ShortcutItem* ShortcutController::itemAt(const QModelIndex& index) const {
	if (!index.isValid()) {
		return nullptr;
	}
	return static_cast<const ShortcutItem*>(index.internalPointer());
}

const QAction* ShortcutController::actionAt(const QModelIndex& index) const {
	const ShortcutItem* item = itemAt(index);
	if (!item) {
		return nullptr;
	}
	return item->action();
}

void ShortcutController::updateKey(const QModelIndex& index, const QKeySequence& keySequence) {
	if (!index.isValid()) {
		return;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return;
	}
	ShortcutItem* item = itemAt(index);
	item->action()->setShortcut(keySequence);
	emit dataChanged(createIndex(index.row(), 0, index.internalPointer()), createIndex(index.row(), 2, index.internalPointer()));
}

void ShortcutController::updateButton(const QModelIndex& index, int button) {
	if (!index.isValid()) {
		return;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return;
	}
	ShortcutItem* item = itemAt(index);
	int oldButton = item->button();
	item->setButton(button);
	if (oldButton >= 0) {
		m_buttons.take(oldButton);
	}
	m_buttons[button] = item;
	emit dataChanged(createIndex(index.row(), 0, index.internalPointer()), createIndex(index.row(), 2, index.internalPointer()));
}

bool ShortcutController::eventFilter(QObject*, QEvent* event) {
	if (event->type() == GamepadButtonEvent::Down()) {
		auto item = m_buttons.find(static_cast<GamepadButtonEvent*>(event)->value());
		if (item == m_buttons.end()) {
			return false;
		}
		QAction* action = item.value()->action();
		if (action->isEnabled()) {
			action->trigger();
		}
		event->accept();
		return true;
	}
	return false;
}

ShortcutController::ShortcutItem::ShortcutItem(QAction* action, const QString& name, ShortcutItem* parent)
	: m_action(action)
	, m_menu(nullptr)
	, m_name(name)
	, m_button(-1)
	, m_parent(parent)
{
	m_visibleName = action->text()
		.remove(QRegExp("&(?!&)"))
		.remove("...");
}

ShortcutController::ShortcutItem::ShortcutItem(QMenu* menu, ShortcutItem* parent)
	: m_action(nullptr)
	, m_menu(menu)
	, m_button(-1)
	, m_parent(parent)
{
	if (menu) {
		m_visibleName = menu->title()
			.remove(QRegExp("&(?!&)"))
			.remove("...");
	}
}

void ShortcutController::ShortcutItem::addAction(QAction* action, const QString& name) {
	m_items.append(ShortcutItem(action, name, this));
}

void ShortcutController::ShortcutItem::addSubmenu(QMenu* menu) {
	m_items.append(ShortcutItem(menu, this));
}
