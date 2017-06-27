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
	, m_rootMenu(QString(), QString())
{
	connect(&m_rootMenu, &InputItem::childAdded, this, &InputModel::itemAdded);
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
		return m_rootMenu.items().count();
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

InputItem* InputModel::itemForMenu(const QMenu* menu) {
	InputItem* item = m_menus[menu];
	if (!item) {
		return &m_rootMenu;
	}
	return item;
}
const InputItem* InputModel::itemForMenu(const QMenu* menu) const {
	const InputItem* item = m_menus[menu];
	if (!item) {
		return &m_rootMenu;
	}
	return item;
}

bool InputModel::loadShortcuts(InputItem* item) {
	if (item->name().isNull()) {
		return false;
	}
	loadGamepadShortcuts(item);
	QVariant shortcut = m_config->getQtOption(item->name(), KEY_SECTION);
	if (!shortcut.isNull()) {
		if (shortcut.toString().endsWith("+")) {
			item->setShortcut(toModifierShortcut(shortcut.toString()));
		} else {
			item->setShortcut(QKeySequence(shortcut.toString())[0]);
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
	if (button.isNull() && m_profile) {
		int buttonInt;
		if (m_profile->lookupShortcutButton(item->name(), &buttonInt)) {
			button = buttonInt;
		}
	}
	if (!button.isNull()) {
		item->setButton(button.toInt());
	}

	QVariant axis = m_config->getQtOption(item->name(), !m_profileName.isNull() ? AXIS_PROFILE_SECTION + m_profileName : AXIS_SECTION);
	int oldAxis = item->axis();
	if (oldAxis >= 0) {
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
			}
		}
	}
}

void InputModel::loadProfile(const QString& profile) {
	m_profileName = profile;
	m_profile = InputProfile::findProfile(profile);
	onSubitems(&m_rootMenu, [this](InputItem* item) {
		loadGamepadShortcuts(item);
	});
}

void InputModel::onSubitems(InputItem* item, std::function<void(InputItem*)> func) {
	for (InputItem* subitem : item->items()) {
		func(subitem);
		onSubitems(subitem, func);
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

void InputModel::itemAdded(InputItem* parent, InputItem* child) {
	const QMenu* menu = child->menu();
	if (menu) {
		m_menus[menu] = child;
	}
	m_names[child->name()] = child;
	connect(child, &InputItem::childAdded, this, &InputModel::itemAdded);
}
