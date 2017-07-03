/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputItem.h"

#include <QMenu>

using namespace QGBA;

InputItem::InputItem()
{
}

InputItem::InputItem(QAction* action, const QString& name, QMenu* parent)
	: QObject(parent)
	, m_action(action)
	, m_shortcut(action->shortcut().isEmpty() ? -2 : action->shortcut()[0])
	, m_name(name)
	, m_menu(parent)
{
	m_visibleName = action->text()
		.remove(QRegExp("&(?!&)"))
		.remove("...");
}

InputItem::InputItem(InputItem::Functions functions, const QString& visibleName, const QString& name, QMenu* parent)
	: QObject(parent)
	, m_functions(functions)
	, m_name(name)
	, m_visibleName(visibleName)
	, m_menu(parent)
{
}

InputItem::InputItem(const QString& visibleName, const QString& name, QMenu* parent)
	: QObject(parent)
	, m_name(name)
	, m_visibleName(visibleName)
	, m_menu(parent)
{
}

InputItem::InputItem(const InputItem& other)
	: QObject(other.m_menu)
	, m_name(other.m_name)
	, m_visibleName(other.m_visibleName)
	, m_shortcut(other.m_shortcut)
	, m_button(other.m_button)
	, m_axis(other.m_axis)
	, m_direction(other.m_direction)
	, m_menu(other.m_menu)
{
}

InputItem::InputItem(InputItem& other)
	: QObject(other.m_menu)
	, m_action(other.m_action)
	, m_functions(other.m_functions)
	, m_name(other.m_name)
	, m_visibleName(other.m_visibleName)
	, m_shortcut(other.m_shortcut)
	, m_button(other.m_button)
	, m_axis(other.m_axis)
	, m_direction(other.m_direction)
	, m_menu(other.m_menu)
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
