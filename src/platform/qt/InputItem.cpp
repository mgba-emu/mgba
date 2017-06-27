/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputItem.h"

#include <QMenu>

using namespace QGBA;

InputItem::InputItem(QAction* action, const QString& name, InputItem* parent)
	: QObject(parent)
	, m_action(action)
	, m_shortcut(action->shortcut().isEmpty() ? 0 : action->shortcut()[0])
	, m_name(name)
	, m_parent(parent)
{
	m_visibleName = action->text()
		.remove(QRegExp("&(?!&)"))
		.remove("...");
}

InputItem::InputItem(QMenu* menu, const QString& name, InputItem* parent)
	: QObject(parent)
	, m_menu(menu)
	, m_name(name)
	, m_parent(parent)
{
	m_visibleName = menu->title()
		.remove(QRegExp("&(?!&)"))
		.remove("...");
}

InputItem::InputItem(InputItem::Functions functions, const QString& visibleName, const QString& name, InputItem* parent)
	: QObject(parent)
	, m_functions(functions)
	, m_name(name)
	, m_visibleName(visibleName)
	, m_parent(parent)
{
}

InputItem::InputItem(int key, const QString& visibleName, const QString& name, InputItem* parent)
	: QObject(parent)
	, m_key(key)
	, m_name(name)
	, m_visibleName(visibleName)
	, m_parent(parent)
{
}

InputItem::InputItem(const QString& visibleName, const QString& name, InputItem* parent)
	: QObject(parent)
	, m_name(name)
	, m_visibleName(visibleName)
	, m_parent(parent)
{
}

void InputItem::setShortcut(int shortcut) {
	m_shortcut = shortcut;
	if (m_action) {
		m_action->setShortcut(QKeySequence(shortcut));
	}
	emit shortcutBound(this, shortcut);
}

void InputItem::clearShortcut() {
	setShortcut(0);
}

void InputItem::setButton(int button) {
	m_button = button;
	emit buttonBound(this, button);
}

void InputItem::clearButton() {
	setButton(-1);
}

void InputItem::setAxis(int axis, GamepadAxisEvent::Direction direction) {
	m_axis = axis;
	m_direction = direction;
	emit axisBound(this, axis, direction);
}

void InputItem::trigger(bool active) {
	if (active) {
		if (m_functions.first) {
			m_functions.first();
		}
	} else {
		if (m_functions.second) {
			m_functions.second();
		}
	}
}
