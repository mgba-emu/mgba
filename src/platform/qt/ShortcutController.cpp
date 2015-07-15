/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutController.h"

#include "ConfigController.h"
#include "GamepadButtonEvent.h"

#include <QAction>
#include <QKeyEvent>
#include <QMenu>

using namespace QGBA;

ShortcutController::ShortcutController(QObject* parent)
	: QAbstractItemModel(parent)
	, m_rootMenu(nullptr)
	, m_config(nullptr)
{
}

void ShortcutController::setConfigController(ConfigController* controller) {
	m_config = controller;
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
		return item->shortcut().toString(QKeySequence::NativeText);
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
	if (m_config) {
		loadShortcuts(item);
	}
	emit dataChanged(createIndex(smenu->items().count() - 1, 0, item),
	                 createIndex(smenu->items().count() - 1, 2, item));
}

void ShortcutController::addFunctions(QMenu* menu, std::function<void()> press, std::function<void()> release,
                                      const QKeySequence& shortcut, const QString& visibleName, const QString& name) {
	ShortcutItem* smenu = m_menuMap[menu];
	if (!smenu) {
		return;
	}
	ShortcutItem* pmenu = smenu->parent();
	int row = pmenu->items().indexOf(*smenu);
	QModelIndex parent = createIndex(row, 0, smenu);
	beginInsertRows(parent, smenu->items().count(), smenu->items().count());
	smenu->addFunctions(qMakePair(press, release), shortcut, visibleName, name);
	endInsertRows();
	ShortcutItem* item = &smenu->items().last();
	if (m_config) {
		loadShortcuts(item);
	}
	m_heldKeys[shortcut] = item;
	emit dataChanged(createIndex(smenu->items().count() - 1, 0, item),
	                 createIndex(smenu->items().count() - 1, 2, item));
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
	emit dataChanged(createIndex(smenu->items().count() - 1, 0, item),
	                 createIndex(smenu->items().count() - 1, 2, item));
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

QKeySequence ShortcutController::shortcutAt(const QModelIndex& index) const {
	const ShortcutItem* item = itemAt(index);
	if (!item) {
		return QKeySequence();
	}
	return item->shortcut();
}

bool ShortcutController::isMenuAt(const QModelIndex& index) const {
	const ShortcutItem* item = itemAt(index);
	if (!item) {
		return false;
	}
	return item->menu();
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
	if (item->functions().first) {
		QKeySequence oldShortcut = item->shortcut();
		if (!oldShortcut.isEmpty()) {
			m_heldKeys.take(oldShortcut);
		}
		if (!keySequence.isEmpty()) {
			m_heldKeys[keySequence] = item;
		}
	}
	item->setShortcut(keySequence);
	if (m_config) {
		m_config->setQtOption(item->name(), keySequence.toString(), KEY_SECTION);
	}
	emit dataChanged(createIndex(index.row(), 0, index.internalPointer()),
	                 createIndex(index.row(), 2, index.internalPointer()));
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
	if (oldButton >= 0) {
		m_buttons.take(oldButton);
	}
	updateAxis(index, -1, GamepadAxisEvent::NEUTRAL);
	item->setButton(button);
	if (button >= 0) {
		m_buttons[button] = item;
	}
	if (m_config) {
		m_config->setQtOption(item->name(), button, BUTTON_SECTION);
		if (!m_profile.isNull()) {
			m_config->setQtOption(item->name(), button, BUTTON_PROFILE_SECTION + m_profile);
		}
	}
	emit dataChanged(createIndex(index.row(), 0, index.internalPointer()),
	                 createIndex(index.row(), 2, index.internalPointer()));
}

void ShortcutController::updateAxis(const QModelIndex& index, int axis, GamepadAxisEvent::Direction direction) {
	if (!index.isValid()) {
		return;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return;
	}
	ShortcutItem* item = itemAt(index);
	int oldAxis = item->axis();
	GamepadAxisEvent::Direction oldDirection = item->direction();
	if (oldAxis >= 0) {
		m_axes.take(qMakePair(oldAxis, oldDirection));
	}
	if (axis >= 0 && direction != GamepadAxisEvent::NEUTRAL) {
		updateButton(index, -1);
		m_axes[qMakePair(axis, direction)] = item;
	}
	item->setAxis(axis, direction);
	if (m_config) {
		char d = '\0';
		if (direction == GamepadAxisEvent::POSITIVE) {
			d = '+';
		}
		if (direction == GamepadAxisEvent::NEGATIVE) {
			d = '-';
		}
		m_config->setQtOption(item->name(), QString("%1%2").arg(d).arg(axis), AXIS_SECTION);
		if (!m_profile.isNull()) {
			m_config->setQtOption(item->name(), QString("%1%2").arg(d).arg(axis), AXIS_PROFILE_SECTION + m_profile);
		}
	}
	emit dataChanged(createIndex(index.row(), 0, index.internalPointer()),
	                 createIndex(index.row(), 2, index.internalPointer()));
}

void ShortcutController::clearKey(const QModelIndex& index) {
	updateKey(index, QKeySequence());
}

void ShortcutController::clearButton(const QModelIndex& index) {
	updateButton(index, -1);
}

bool ShortcutController::eventFilter(QObject*, QEvent* event) {
	if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->isAutoRepeat()) {
			return false;
		}
		auto item = m_heldKeys.find(keyEventToSequence(keyEvent));
		if (item == m_heldKeys.end()) {
			return false;
		}
		ShortcutItem::Functions pair = item.value()->functions();
		if (event->type() == QEvent::KeyPress) {
			if (pair.first) {
				pair.first();
			}
		} else {
			if (pair.second) {
				pair.second();
			}
		}
		event->accept();
		return true;
	}
	if (event->type() == GamepadButtonEvent::Down()) {
		auto item = m_buttons.find(static_cast<GamepadButtonEvent*>(event)->value());
		if (item == m_buttons.end()) {
			return false;
		}
		QAction* action = item.value()->action();
		if (action && action->isEnabled()) {
			action->trigger();
		}
		ShortcutItem::Functions pair = item.value()->functions();
		if (pair.first) {
			pair.first();
		}
		event->accept();
		return true;
	}
	if (event->type() == GamepadButtonEvent::Up()) {
		auto item = m_buttons.find(static_cast<GamepadButtonEvent*>(event)->value());
		if (item == m_buttons.end()) {
			return false;
		}
		ShortcutItem::Functions pair = item.value()->functions();
		if (pair.second) {
			pair.second();
		}
		event->accept();
		return true;
	}
	if (event->type() == GamepadAxisEvent::Type()) {
		GamepadAxisEvent* gae = static_cast<GamepadAxisEvent*>(event);
		auto item = m_axes.find(qMakePair(gae->axis(), gae->direction()));
		if (item == m_axes.end()) {
			return false;
		}
		if (gae->isNew()) {
			QAction* action = item.value()->action();
			if (action && action->isEnabled()) {
				action->trigger();
			}
		}
		ShortcutItem::Functions pair = item.value()->functions();
		if (gae->isNew()) {
			if (pair.first) {
				pair.first();
			}
		} else {
			if (pair.second) {
				pair.second();
			}
		}
		event->accept();
		return true;
	}
	return false;
}

void ShortcutController::loadShortcuts(ShortcutItem* item) {
	if (item->name().isNull()) {
		return;
	}
	QVariant shortcut = m_config->getQtOption(item->name(), KEY_SECTION);
	if (!shortcut.isNull()) {
		QKeySequence keySequence(shortcut.toString());
		if (item->functions().first) {
			QKeySequence oldShortcut = item->shortcut();
			if (!oldShortcut.isEmpty()) {
				m_heldKeys.take(oldShortcut);
			}
			m_heldKeys[keySequence] = item;
		}
		item->setShortcut(keySequence);
	}
	loadGamepadShortcuts(item);
}

void ShortcutController::loadGamepadShortcuts(ShortcutItem* item) {
	if (item->name().isNull()) {
		return;
	}
	QVariant button = m_config->getQtOption(item->name(), !m_profile.isNull() ? BUTTON_PROFILE_SECTION + m_profile : BUTTON_SECTION);
	int oldButton = item->button();
	if (oldButton >= 0) {
		m_buttons.take(oldButton);
		item->setButton(-1);
	}
	if (!button.isNull()) {
		item->setButton(button.toInt());
		m_buttons[button.toInt()] = item;
	}

	QVariant axis = m_config->getQtOption(item->name(), !m_profile.isNull() ? AXIS_PROFILE_SECTION + m_profile : AXIS_SECTION);
	int oldAxis = item->axis();
	GamepadAxisEvent::Direction oldDirection = item->direction();
	if (oldAxis >= 0) {
		m_axes.take(qMakePair(oldAxis, oldDirection));
		item->setAxis(-1, GamepadAxisEvent::NEUTRAL);
	}
	if (!axis.isNull()) {
		QString axisDesc = axis.toString();
		if (axisDesc.size() >= 2) {
			GamepadAxisEvent::Direction direction = GamepadAxisEvent::NEUTRAL;
			if (axisDesc[0] == '-') {
				direction = GamepadAxisEvent::NEGATIVE;
			}
			if (axisDesc[0] == '+') {
				direction = GamepadAxisEvent::POSITIVE;
			}
			bool ok;
			int axis = axisDesc.mid(1).toInt(&ok);
			if (ok) {
				item->setAxis(axis, direction);
				m_axes[qMakePair(axis, direction)] = item;
			}
		}
	}
}

QKeySequence ShortcutController::keyEventToSequence(const QKeyEvent* event) {
	QString modifier = QString::null;

	if (event->modifiers() & Qt::ShiftModifier) {
		modifier += "Shift+";
	}
	if (event->modifiers() & Qt::ControlModifier) {
		modifier += "Ctrl+";
	}
	if (event->modifiers() & Qt::AltModifier) {
		modifier += "Alt+";
	}
	if (event->modifiers() & Qt::MetaModifier) {
		modifier += "Meta+";
	}

	QString key = QKeySequence(event->key()).toString();
	return QKeySequence(modifier + key);
}

void ShortcutController::loadProfile(const QString& profile) {
	m_profile = profile;
	onSubitems(&m_rootMenu, [this](ShortcutItem* item) {
		loadGamepadShortcuts(item);
	});
}

void ShortcutController::onSubitems(ShortcutItem* item, std::function<void(ShortcutItem*)> func) {
	for (ShortcutItem& subitem : item->items()) {
		func(&subitem);
		onSubitems(&subitem, func);
	}
}

ShortcutController::ShortcutItem::ShortcutItem(QAction* action, const QString& name, ShortcutItem* parent)
	: m_action(action)
	, m_shortcut(action->shortcut())
	, m_menu(nullptr)
	, m_name(name)
	, m_button(-1)
	, m_axis(-1)
	, m_direction(GamepadAxisEvent::NEUTRAL)
	, m_parent(parent)
{
	m_visibleName = action->text()
		.remove(QRegExp("&(?!&)"))
		.remove("...");
}

ShortcutController::ShortcutItem::ShortcutItem(ShortcutController::ShortcutItem::Functions functions, const QKeySequence& shortcut, const QString& visibleName, const QString& name, ShortcutItem* parent)
	: m_action(nullptr)
	, m_shortcut(shortcut)
	, m_functions(functions)
	, m_menu(nullptr)
	, m_name(name)
	, m_visibleName(visibleName)
	, m_button(-1)
	, m_axis(-1)
	, m_direction(GamepadAxisEvent::NEUTRAL)
	, m_parent(parent)
{
}

ShortcutController::ShortcutItem::ShortcutItem(QMenu* menu, ShortcutItem* parent)
	: m_action(nullptr)
	, m_menu(menu)
	, m_button(-1)
	, m_axis(-1)
	, m_direction(GamepadAxisEvent::NEUTRAL)
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

void ShortcutController::ShortcutItem::addFunctions(ShortcutController::ShortcutItem::Functions functions,
                                                    const QKeySequence& shortcut, const QString& visibleName,
                                                    const QString& name) {
	m_items.append(ShortcutItem(functions, shortcut, visibleName, name, this));
}

void ShortcutController::ShortcutItem::addSubmenu(QMenu* menu) {
	m_items.append(ShortcutItem(menu, this));
}

void ShortcutController::ShortcutItem::setShortcut(const QKeySequence& shortcut) {
	m_shortcut = shortcut;
	if (m_action) {
		m_action->setShortcut(shortcut);
	}
}

void ShortcutController::ShortcutItem::setAxis(int axis, GamepadAxisEvent::Direction direction) {
	m_axis = axis;
	m_direction = direction;
}
