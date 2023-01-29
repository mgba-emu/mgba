/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ShortcutController.h"

#include "ConfigController.h"
#include "input/GamepadButtonEvent.h"
#include "input/GamepadHatEvent.h"
#include "InputProfile.h"
#include "scripting/ScriptingController.h"

#include <QAction>
#include <QKeyEvent>
#include <QMenu>
#include <QRegularExpression>

using namespace QGBA;

ShortcutController::ShortcutController(QObject* parent)
	: QObject(parent)
{
}

void ShortcutController::setConfigController(ConfigController* controller) {
	m_config = controller;
}

void ShortcutController::setActionMapper(ActionMapper* actions) {
	m_actions = actions;
	connect(actions, &ActionMapper::actionAdded, this, &ShortcutController::generateItem);
	connect(actions, &ActionMapper::menuCleared, this, &ShortcutController::menuCleared);
	rebuildItems();
}

void ShortcutController::setScriptingController(ScriptingController* controller) {
	m_scripting = controller;
}

void ShortcutController::updateKey(const QString& name, int keySequence) {
	auto item = m_items[name];
	if (!item) {
		return;
	}
	updateKey(item, keySequence);
	if (m_config) {
		m_config->setQtOption(item->name(), QKeySequence(keySequence).toString(), KEY_SECTION);
	}
}

void ShortcutController::updateKey(std::shared_ptr<Shortcut> item, int keySequence) {
	int oldShortcut = item->shortcut();
	if (oldShortcut != keySequence && m_actions->isHeld(item->name())) {
		if (oldShortcut > 0) {
			if (item->action() && item->action()->booleanAction()) {
				item->action()->booleanAction()(false);
			}
			m_heldKeys.take(oldShortcut);
		}
		if (keySequence > 0) {
			m_heldKeys[keySequence] = item;
		}
	}

	item->setShortcut(keySequence);
}

void ShortcutController::updateButton(const QString& name, int button) {
	auto item = m_items[name];
	if (!item) {
		return;
	}
	int oldButton = item->button();
	if (oldButton >= 0) {
		m_buttons.take(oldButton);
	}
	item->setButton(button);
	if (button >= 0) {
		clearAxis(name);
		m_buttons[button] = item;
	}
	if (m_config) {
		m_config->setQtOption(name, button, BUTTON_SECTION);
		if (!m_profileName.isNull()) {
			m_config->setQtOption(name, button, BUTTON_PROFILE_SECTION + m_profileName);
		}
	}
}

void ShortcutController::updateAxis(const QString& name, int axis, GamepadAxisEvent::Direction direction) {
	auto item = m_items[name];
	if (!item) {
		return;
	}
	int oldAxis = item->axis();
	GamepadAxisEvent::Direction oldDirection = item->direction();
	if (oldAxis >= 0) {
		m_axes.take(std::make_pair(oldAxis, oldDirection));
	}
	if (axis >= 0 && direction != GamepadAxisEvent::NEUTRAL) {
		clearButton(name);
		m_axes[std::make_pair(axis, direction)] = item;
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
		m_config->setQtOption(name, QString("%1%2").arg(d).arg(axis), AXIS_SECTION);
		if (!m_profileName.isNull()) {
			m_config->setQtOption(name, QString("%1%2").arg(d).arg(axis), AXIS_PROFILE_SECTION + m_profileName);
		}
	}
}

void ShortcutController::clearKey(const QString& name) {
	updateKey(name, 0);
}

void ShortcutController::clearButton(const QString& name) {
	updateButton(name, -1);
}

void ShortcutController::clearAxis(const QString& name) {
	updateAxis(name, -1, GamepadAxisEvent::NEUTRAL);
}

void ShortcutController::rebuildItems() {
	m_items.clear();
	m_buttons.clear();
	m_axes.clear();
	m_heldKeys.clear();
	onSubitems({}, std::bind(&ShortcutController::generateItem, this, std::placeholders::_1));
}

bool ShortcutController::eventFilter(QObject* obj, QEvent* event) {
	if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
#ifdef ENABLE_SCRIPTING
		if (m_scripting) {
			m_scripting->event(obj, event);
		}
#endif
		if (keyEvent->isAutoRepeat()) {
			return false;
		}
		int key = keyEvent->key();
		if (!isModifierKey(key)) {
			key |= (keyEvent->modifiers() & ~Qt::KeypadModifier);
		} else {
			key = toModifierKey(key | (keyEvent->modifiers() & ~Qt::KeypadModifier));
		}
		auto item = m_heldKeys.find(key);
		if (item != m_heldKeys.end()) {
			Action::BooleanFunction fn = item.value()->action()->booleanAction();
			fn(event->type() == QEvent::KeyPress);
			event->accept();
		}
	}
	if (event->type() == GamepadButtonEvent::Down()) {
#ifdef ENABLE_SCRIPTING
		if (m_scripting) {
			m_scripting->event(obj, event);
		}
#endif
		auto item = m_buttons.find(static_cast<GamepadButtonEvent*>(event)->value());
		if (item == m_buttons.end()) {
			return false;
		}
		Action* action = item.value()->action();
		if (action) {
			if (m_actions->isHeld(action->name())) {
				action->trigger(true);
			} else {
				action->trigger(!action->isActive());
			}
		}
		event->accept();
		return true;
	}
	if (event->type() == GamepadButtonEvent::Up()) {
#ifdef ENABLE_SCRIPTING
		if (m_scripting) {
			m_scripting->event(obj, event);
		}
#endif
		auto item = m_buttons.find(static_cast<GamepadButtonEvent*>(event)->value());
		if (item == m_buttons.end()) {
			return false;
		}
		Action* action = item.value()->action();
		if (action && m_actions->isHeld(action->name())) {
			action->trigger(false);
		}
		event->accept();
		return true;
	}
	if (event->type() == GamepadAxisEvent::Type()) {
		GamepadAxisEvent* gae = static_cast<GamepadAxisEvent*>(event);
		auto item = m_axes.find(std::make_pair(gae->axis(), gae->direction()));
		if (item == m_axes.end()) {
			return false;
		}
		Action* action = item.value()->action();
		if (action) {
			if (gae->isNew()) {
				if (m_actions->isHeld(action->name())) {
					action->trigger(true);
				} else {
					action->trigger(!action->isActive());
				}
			} else if (m_actions->isHeld(action->name())) {
				action->trigger(false);
			}
		}
		event->accept();
		return true;
	}
#ifdef ENABLE_SCRIPTING
	if (event->type() == GamepadHatEvent::Type()) {
		if (m_scripting) {
			m_scripting->event(obj, event);
		}
	}
#endif
	return false;
}

void ShortcutController::generateItem(const QString& itemName) {
	if (itemName.isNull() || itemName[0] == '.') {
		return;
	}
	Action* action = m_actions->getAction(itemName);
	if (action) {
		std::shared_ptr<Shortcut> item = std::make_shared<Shortcut>(action);
		m_items[itemName] = item;
		loadShortcuts(item);
	}
	emit shortcutAdded(itemName);
}

bool ShortcutController::loadShortcuts(std::shared_ptr<Shortcut> item) {
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
	} else {
		QKeySequence defaultShortcut = m_actions->defaultShortcut(item->name());
		if (!defaultShortcut.isEmpty()) {
			updateKey(item, defaultShortcut[0]);
			return true;
		}
	}
	return false;
}

void ShortcutController::loadGamepadShortcuts(std::shared_ptr<Shortcut> item) {
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
		m_axes.take(std::make_pair(oldAxis, oldDirection));
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
				m_axes[std::make_pair(axis, direction)] = item;
			}
		}
	}
}

void ShortcutController::loadProfile(const QString& profile) {
	m_profileName = profile;
	m_profile = InputProfile::findProfile(profile);
	onSubitems({}, [this](std::shared_ptr<Shortcut> item) {
		loadGamepadShortcuts(item);
	});
}

void ShortcutController::onSubitems(const QString& menu, std::function<void(std::shared_ptr<Shortcut>)> func) {
	for (const QString& subitem : m_actions->menuItems(menu)) {
		auto item = m_items[subitem];
		if (item) {
			func(item);
		}
		if (subitem.size() && subitem[0] == '.') {
			onSubitems(subitem.mid(1), func);
		}
	}
}

void ShortcutController::onSubitems(const QString& menu, std::function<void(const QString&)> func) {
	for (const QString& subitem : m_actions->menuItems(menu)) {
		func(subitem);
		if (subitem.size() && subitem[0] == '.') {
			onSubitems(subitem.mid(1), func);
		}
	}
}

int ShortcutController::toModifierShortcut(const QString& shortcut) {
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

bool ShortcutController::isModifierKey(int key) {
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

int ShortcutController::toModifierKey(int key) {
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

const Shortcut* ShortcutController::shortcut(const QString& action) const {
	return m_items[action].get();
}

QString ShortcutController::name(int index, const QString& parent) const {
	QStringList menu = m_actions->menuItems(parent.isNull() || parent[0] != '.' ? parent : parent.mid(1));
	menu.removeAll({});
	if (index >= menu.size()) {
		return {};
	}

	return menu[index];
}

QString ShortcutController::parent(const QString& action) const {
	return QString(".%1").arg(m_actions->menuFor(action));
}

QString ShortcutController::visibleName(const QString& action) const {
	if (action.isNull()) {
		return {};
	}
	QString name;
	if (action[0] == '.') {
		name = m_actions->menuName(action.mid(1));
	} else {
		name = m_actions->getAction(action)->visibleName();
	}
	return name.replace(QRegularExpression("&(.)"), "\\1");
}

int ShortcutController::indexIn(const QString& action) const {
	QString name = m_actions->menuFor(action);
	QStringList menu = m_actions->menuItems(name);
	menu.removeAll({});
	return menu.indexOf(action);
}

int ShortcutController::count(const QString& name) const {
	QStringList menu;
	if (name.isNull()) {
		menu = m_actions->menuItems();
	} else if (name[0] != '.') {
		return 0;
	} else {
		menu = m_actions->menuItems(name.mid(1));
	}
	menu.removeAll({});
	return menu.count();
}

Shortcut::Shortcut(Action* action)
	: m_action(action)
{
}

void Shortcut::setShortcut(int shortcut) {
	if (m_shortcut == shortcut) {
		return;
	}
	m_shortcut = shortcut;
	emit shortcutChanged(shortcut);
}

void Shortcut::setButton(int button) {
	if (m_button == button) {
		return;
	}
	m_button = button;
	emit buttonChanged(button);
}

void Shortcut::setAxis(int axis, GamepadAxisEvent::Direction direction) {
	if (m_axis == axis && m_direction == direction) {
		return;
	}
	m_axis = axis;
	m_direction = direction;
	emit axisChanged(axis, direction);
}
