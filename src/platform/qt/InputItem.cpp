/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputItem.h"

#include <QMenu>

using namespace QGBA;

InputItem::InputItem(QAction* action, const QString& name, InputItem* parent)
	: m_action(action)
	, m_shortcut(action->shortcut().isEmpty() ? 0 : action->shortcut()[0])
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

InputItem::InputItem(InputItem::Functions functions, int shortcut, const QString& visibleName, const QString& name, InputItem* parent)
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

InputItem::InputItem(QMenu* menu, InputItem* parent)
	: m_action(nullptr)
	, m_shortcut(0)
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

void InputItem::addAction(QAction* action, const QString& name) {
	m_items.append(InputItem(action, name, this));
}

void InputItem::addFunctions(InputItem::Functions functions,
                             int shortcut, const QString& visibleName,
                             const QString& name) {
	m_items.append(InputItem(functions, shortcut, visibleName, name, this));
}

void InputItem::addSubmenu(QMenu* menu) {
	m_items.append(InputItem(menu, this));
}

void InputItem::setShortcut(int shortcut) {
	m_shortcut = shortcut;
	if (m_action) {
		m_action->setShortcut(QKeySequence(shortcut));
	}
}

void InputItem::setAxis(int axis, GamepadAxisEvent::Direction direction) {
	m_axis = axis;
	m_direction = direction;
}
