/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputIndex.h"

#include "ConfigController.h"
#include "InputProfile.h"

using namespace QGBA;

void InputIndex::setConfigController(ConfigController* controller) {
	m_config = controller;
}

void InputIndex::clone(InputIndex* root, bool actions) {
	if (!actions) {
		clone(const_cast<const InputIndex*>(root));
	} else {
		m_root.clear();
		onSubitems(root->root(), [this](InputItem* item, InputItem* parent, QVariant accum) -> QVariant {
			InputItem* newParent = qvariant_cast<InputItem*>(accum);
			InputItem* newItem = newParent->addItem(*item);
			itemAdded(newItem);
			return QVariant::fromValue(newItem);
		}, QVariant::fromValue(&m_root));
	}
}

void InputIndex::clone(const InputIndex* root) {
	m_root.clear();
	onSubitems(root->root(), [this](const InputItem* item, const InputItem* parent, QVariant accum) -> QVariant {
		InputItem* newParent = qvariant_cast<InputItem*>(accum);
		InputItem* newItem = newParent->addItem(*item);
		itemAdded(newItem);
		return QVariant::fromValue(newItem);
	}, QVariant::fromValue(&m_root));
}

void InputIndex::rebuild(const InputIndex* root) {
	rebuild(root->root());
}

void InputIndex::rebuild(const InputItem* root) {
	const InputItem* sourceRoot;
	if (!root) {
		sourceRoot = &m_root;
	} else {
		sourceRoot = root;
	}

	m_names.clear();
	m_menus.clear();
	m_shortcuts.clear();
	m_buttons.clear();
	m_axes.clear();

	onSubitems(sourceRoot, [this](const InputItem* item, const InputItem* parent, QVariant accum) -> QVariant {
		InputItem* newParent = qvariant_cast<InputItem*>(accum);
		InputItem* newItem = nullptr;
		for (auto iter : newParent->items()) {
			if (*iter == *item) {
				newItem = iter;
				break;
			}
		}
		newItem->setShortcut(item->shortcut());
		newItem->setButton(item->button());
		newItem->setAxis(item->axis(), item->direction());

		itemAdded(newItem);
		return QVariant::fromValue(newItem);
	}, QVariant::fromValue(&m_root));
}

InputItem* InputIndex::itemAt(const QString& name) {
	return m_names[name];
}

const InputItem* InputIndex::itemAt(const QString& name) const {
	return m_names[name];
}

InputItem* InputIndex::itemForMenu(const QMenu* menu) {
	InputItem* item = m_menus[menu];
	return item;
}

const InputItem* InputIndex::itemForMenu(const QMenu* menu) const {
	const InputItem* item = m_menus[menu];
	return item;
}

InputItem* InputIndex::itemForShortcut(int shortcut) {
	return m_shortcuts[shortcut];
}

InputItem* InputIndex::itemForButton(int button) {
	return m_buttons[button];
}

InputItem* InputIndex::itemForAxis(int axis, GamepadAxisEvent::Direction direction) {
	return m_axes[qMakePair(axis, direction)];
}

bool InputIndex::loadShortcuts(InputItem* item) {
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

void InputIndex::loadGamepadShortcuts(InputItem* item) {
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

int InputIndex::toModifierShortcut(const QString& shortcut) {
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

void InputIndex::itemAdded(InputItem* child) {
	const QMenu* menu = child->menu();
	if (menu) {
		m_menus[menu] = child;
	}
	m_names[child->name()] = child;

	if (child->shortcut()) {
		m_shortcuts[child->shortcut()] = child;
	}
	if (child->button() >= 0) {
		m_buttons[child->button()] = child;
	}
	if (child->direction() != GamepadAxisEvent::NEUTRAL) {
		m_axes[qMakePair(child->axis(), child->direction())] = child;
	}
}

bool InputIndex::isModifierKey(int key) {
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

int InputIndex::toModifierKey(int key) {
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

void InputIndex::onSubitems(InputItem* item, std::function<void(InputItem*)> func) {
	for (InputItem* subitem : item->items()) {
		func(subitem);
		onSubitems(subitem, func);
	}
}

void InputIndex::onSubitems(InputItem* item, std::function<QVariant(InputItem*, InputItem*, QVariant)> func, QVariant accum) {
	for (InputItem* subitem : item->items()) {
		QVariant newAccum = func(subitem, item, accum);
		onSubitems(subitem, func, newAccum);
	}
}

void InputIndex::onSubitems(const InputItem* item, std::function<QVariant(const InputItem*, const InputItem*, QVariant)> func, QVariant accum) {
	for (const InputItem* subitem : item->items()) {
		QVariant newAccum = func(subitem, item, accum);
		onSubitems(subitem, func, newAccum);
	}
}

void InputIndex::loadProfile(const QString& profile) {
	m_profileName = profile;
	m_profile = InputProfile::findProfile(profile);
	onSubitems(&m_root, [this](InputItem* item) {
		loadGamepadShortcuts(item);
	});
}

