/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputModel.h"

#include "ConfigController.h"
#include "GamepadButtonEvent.h"
#include "InputProfile.h"

#include <QAction>
#include <QKeyEvent>
#include <QMenu>

using namespace QGBA;

InputModel::InputModel(QObject* parent)
	: QAbstractItemModel(parent)
	, m_rootMenu(nullptr)
	, m_config(nullptr)
	, m_profile(nullptr)
{
}

void InputModel::setConfigController(ConfigController* controller) {
	m_config = controller;
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
	const InputItem* pmenu = &m_rootMenu;
	if (parent.isValid()) {
		pmenu = static_cast<InputItem*>(parent.internalPointer());
	}
	return createIndex(row, column, const_cast<InputItem*>(&pmenu->items()[row]));
}

QModelIndex InputModel::parent(const QModelIndex& index) const {
	if (!index.isValid() || !index.internalPointer()) {
		return QModelIndex();
	}
	InputItem* item = static_cast<InputItem*>(index.internalPointer());
	return this->index(item->parent());
}

QModelIndex InputModel::index(InputItem* item) const {
	if (!item || !item->parent()) {
		return QModelIndex();
	}
	return createIndex(item->parent()->items().indexOf(*item), 0, item);
}

int InputModel::columnCount(const QModelIndex& index) const {
	return 3;
}

int InputModel::rowCount(const QModelIndex& index) const {
	if (!index.isValid()) {
		return m_rootMenu.items().count();
	}
	const InputItem* item = static_cast<const InputItem*>(index.internalPointer());
	return item->items().count();
}

InputItem* InputModel::add(QMenu* menu, std::function<void (InputItem*)> callback) {
	InputItem* smenu = m_menuMap[menu];
	if (!smenu) {
		return nullptr;
	}
	QModelIndex parent = index(smenu);
	beginInsertRows(parent, smenu->items().count(), smenu->items().count());
	callback(smenu);
	endInsertRows();
	InputItem* item = &smenu->items().last();
	emit dataChanged(createIndex(smenu->items().count() - 1, 0, item),
	                 createIndex(smenu->items().count() - 1, 2, item));
	return item;
}

void InputModel::addAction(QMenu* menu, QAction* action, const QString& name) {
	InputItem* item = add(menu, [&](InputItem* smenu) {
		smenu->addAction(action, name);
	});
	if (!item) {
		return;
	}
	if (m_config) {
		loadShortcuts(item);
	}
}

void InputModel::addFunctions(QMenu* menu, std::function<void()> press, std::function<void()> release,
                                      int shortcut, const QString& visibleName, const QString& name) {
	InputItem* item = add(menu, [&](InputItem* smenu) {
		smenu->addFunctions(qMakePair(press, release), shortcut, visibleName, name);
	});
	if (!item) {
		return;
	}

	bool loadedShortcut = false;
	if (m_config) {
		loadedShortcut = loadShortcuts(item);
	}
	if (!loadedShortcut && !m_heldKeys.contains(shortcut)) {
		m_heldKeys[shortcut] = item;
	}
}

void InputModel::addFunctions(QMenu* menu, std::function<void()> press, std::function<void()> release,
                                      const QKeySequence& shortcut, const QString& visibleName, const QString& name) {
	addFunctions(menu, press, release, shortcut[0], visibleName, name);
}

void InputModel::addKey(QMenu* menu, mPlatform platform, int key, int shortcut, const QString& visibleName, const QString& name) {
	InputItem* item = add(menu, [&](InputItem* smenu) {
		smenu->addKey(platform, key, shortcut, visibleName, name);
	});
	if (!item) {
		return;
	}
	m_keys[qMakePair(platform, key)] = item;
}

QModelIndex InputModel::addMenu(QMenu* menu, QMenu* parentMenu) {
	InputItem* smenu = m_menuMap[parentMenu];
	if (!smenu) {
		smenu = &m_rootMenu;
	}
	QModelIndex parent = index(smenu);
	beginInsertRows(parent, smenu->items().count(), smenu->items().count());
	smenu->addSubmenu(menu);
	endInsertRows();
	InputItem* item = &smenu->items().last();
	emit dataChanged(createIndex(smenu->items().count() - 1, 0, item),
	                 createIndex(smenu->items().count() - 1, 2, item));
	m_menuMap[menu] = item;
	return index(item);
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
	return &pmenu->items()[index.row()];
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
	return &pmenu->items()[index.row()];
}

int InputModel::shortcutAt(const QModelIndex& index) const {
	const InputItem* item = itemAt(index);
	if (!item) {
		return 0;
	}
	return item->shortcut();
}

int InputModel::keyAt(const QModelIndex& index) const {
	const InputItem* item = itemAt(index);
	if (!item) {
		return -1;
	}
	return item->key();
}

bool InputModel::isMenuAt(const QModelIndex& index) const {
	const InputItem* item = itemAt(index);
	if (!item) {
		return false;
	}
	return item->menu();
}

void InputModel::updateKey(const QModelIndex& index, int keySequence) {
	if (!index.isValid()) {
		return;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return;
	}
	InputItem* item = itemAt(index);
	updateKey(item, keySequence);
	if (m_config) {
		m_config->setQtOption(item->name(), QKeySequence(keySequence).toString(), KEY_SECTION);
	}
}

void InputModel::updateKey(InputItem* item, int keySequence) {
	int oldShortcut = item->shortcut();
	if (item->functions().first || item->key() >= 0) {
		if (oldShortcut > 0) {
			m_heldKeys.take(oldShortcut);
		}
		if (keySequence >= 0) {
			m_keys[qMakePair(item->platform(), keySequence)] = item;
		}
	}

	item->setShortcut(keySequence);

	emit dataChanged(createIndex(index(item).row(), 0, item),
	                 createIndex(index(item).row(), 2, item));

	emit keyRebound(index(item), keySequence);
}

void InputModel::updateButton(const QModelIndex& index, int button) {
	if (!index.isValid()) {
		return;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return;
	}
	InputItem* item = itemAt(index);
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
		if (!m_profileName.isNull()) {
			m_config->setQtOption(item->name(), button, BUTTON_PROFILE_SECTION + m_profileName);
		}
	}
	emit dataChanged(createIndex(index.row(), 0, index.internalPointer()),
	                 createIndex(index.row(), 2, index.internalPointer()));

	emit buttonRebound(index, button);
}

void InputModel::updateAxis(const QModelIndex& index, int axis, GamepadAxisEvent::Direction direction) {
	if (!index.isValid()) {
		return;
	}
	const QModelIndex& parent = index.parent();
	if (!parent.isValid()) {
		return;
	}
	InputItem* item = itemAt(index);
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
		if (!m_profileName.isNull()) {
			m_config->setQtOption(item->name(), QString("%1%2").arg(d).arg(axis), AXIS_PROFILE_SECTION + m_profileName);
		}
	}
	emit dataChanged(createIndex(index.row(), 0, index.internalPointer()),
	                 createIndex(index.row(), 2, index.internalPointer()));

	emit axisRebound(index, axis, direction);
}

void InputModel::clearKey(const QModelIndex& index) {
	updateKey(index, 0);
}

void InputModel::clearButton(const QModelIndex& index) {
	updateButton(index, -1);
}

bool InputModel::triggerKey(int keySequence, bool down, mPlatform platform) {
	auto key = m_keys.find(qMakePair(platform, keySequence));
	if (key != m_keys.end()) {
		m_keyCallback(key.value()->parent()->menu(), key.value()->key(), down);
		return true;
	}
	auto heldKey = m_heldKeys.find(keySequence);
	if (heldKey != m_heldKeys.end()) {
		auto pair = heldKey.value()->functions();
		if (down) {
			if (pair.first) {
				pair.first();
			}
		} else {
			if (pair.second) {
				pair.second();
			}
		}
		return true;
	}
	return false;
}

bool InputModel::triggerButton(int button, bool down) {
	auto item = m_buttons.find(button);
	if (item == m_buttons.end()) {
		return false;
	}
	if (down) {
		QAction* action = item.value()->action();
		if (action && action->isEnabled()) {
			action->trigger();
		}
		auto pair = item.value()->functions();
		if (pair.first) {
			pair.first();
		}
	} else {
		auto pair = item.value()->functions();
		if (pair.second) {
			pair.second();
		}
	}
	return true;
}

bool InputModel::triggerAxis(int axis, GamepadAxisEvent::Direction direction, bool isNew) {
	auto item = m_axes.find(qMakePair(axis, direction));
	if (item == m_axes.end()) {
		return false;
	}
	if (isNew) {
		QAction* action = item.value()->action();
		if (action && action->isEnabled()) {
			action->trigger();
		}
	}
	auto pair = item.value()->functions();
	if (isNew) {
		if (pair.first) {
			pair.first();
		}
	} else {
		if (pair.second) {
			pair.second();
		}
	}
	return true;
}

bool InputModel::loadShortcuts(InputItem* item) {
	if (item->name().isNull()) {
		return false;
	}
	loadGamepadShortcuts(item);
	QVariant shortcut = m_config->getQtOption(item->name(), KEY_SECTION);
	if (!shortcut.isNull()) {
		if (shortcut.toString().endsWith("+")) {
			updateKey(item, toModifierShortcut(shortcut.toString()));
		} else {
			updateKey(item, QKeySequence(shortcut.toString())[0]);
		}
		return true;
	}
	return false;
}

void InputModel::loadGamepadShortcuts(InputItem* item) {
	if (item->name().isNull()) {
		return;
	}
	QVariant button = m_config->getQtOption(item->name(), !m_profileName.isNull() ? BUTTON_PROFILE_SECTION + m_profileName : BUTTON_SECTION);
	int oldButton = item->button();
	if (oldButton >= 0) {
		m_buttons.take(oldButton);
		item->setButton(-1);
	}
	if (button.isNull() && m_profile) {
		int buttonInt;
		if (m_profile->lookupShortcutButton(item->name(), &buttonInt)) {
			button = buttonInt;
		}
	}
	if (!button.isNull()) {
		item->setButton(button.toInt());
		m_buttons[button.toInt()] = item;
	}

	QVariant axis = m_config->getQtOption(item->name(), !m_profileName.isNull() ? AXIS_PROFILE_SECTION + m_profileName : AXIS_SECTION);
	int oldAxis = item->axis();
	GamepadAxisEvent::Direction oldDirection = item->direction();
	if (oldAxis >= 0) {
		m_axes.take(qMakePair(oldAxis, oldDirection));
		item->setAxis(-1, GamepadAxisEvent::NEUTRAL);
	}
	if (axis.isNull() && m_profile) {
		int axisInt;
		GamepadAxisEvent::Direction direction;
		if (m_profile->lookupShortcutAxis(item->name(), &axisInt, &direction)) {
			axis = QLatin1String(direction == GamepadAxisEvent::Direction::NEGATIVE ? "-" : "+") + QString::number(axisInt);
		}
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

void InputModel::loadProfile(mPlatform platform, const QString& profile) {
	m_profileName = profile;
	m_profile = InputProfile::findProfile(platform, profile);
	onSubitems(&m_rootMenu, [this](InputItem* item) {
		loadGamepadShortcuts(item);
	});
}

void InputModel::onSubitems(InputItem* item, std::function<void(InputItem*)> func) {
	for (InputItem& subitem : item->items()) {
		func(&subitem);
		onSubitems(&subitem, func);
	}
}

int InputModel::toModifierShortcut(const QString& shortcut) {
	// Qt doesn't seem to work with raw modifier shortcuts!
	QStringList modifiers = shortcut.split('+');
	int value = 0;
	for (const auto& mod : modifiers) {
		if (mod == QLatin1String("Shift")) {
			value |= Qt::ShiftModifier;
			continue;
		}
		if (mod == QLatin1String("Ctrl")) {
			value |= Qt::ControlModifier;
			continue;
		}
		if (mod == QLatin1String("Alt")) {
			value |= Qt::AltModifier;
			continue;
		}
		if (mod == QLatin1String("Meta")) {
			value |= Qt::MetaModifier;
			continue;
		}
	}
	return value;
}

bool InputModel::isModifierKey(int key) {
	switch (key) {
	case Qt::Key_Shift:
	case Qt::Key_Control:
	case Qt::Key_Alt:
	case Qt::Key_Meta:
		return true;
	default:
		return false;
	}
}

int InputModel::toModifierKey(int key) {
	int modifiers = key & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
	key ^= modifiers;
	switch (key) {
	case Qt::Key_Shift:
		modifiers |= Qt::ShiftModifier;
		break;
	case Qt::Key_Control:
		modifiers |= Qt::ControlModifier;
		break;
	case Qt::Key_Alt:
		modifiers |= Qt::AltModifier;
		break;
	case Qt::Key_Meta:
		modifiers |= Qt::MetaModifier;
		break;
	default:
		break;
	}
	return modifiers;
}
