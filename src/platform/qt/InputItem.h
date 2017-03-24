/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_ITEM
#define QGBA_INPUT_ITEM

#include "GamepadAxisEvent.h"

#include <QAction>

namespace QGBA {

class InputItem {
public:
	typedef QPair<std::function<void ()>, std::function<void ()>> Functions;

	InputItem(QAction* action, const QString& name, InputItem* parent = nullptr);
	InputItem(Functions functions, int shortcut, const QString& visibleName,
	          const QString& name, InputItem* parent = nullptr);
	InputItem(QMenu* action, InputItem* parent = nullptr);

	QAction* action() { return m_action; }
	const QAction* action() const { return m_action; }
	const int shortcut() const { return m_shortcut; }
	Functions functions() const { return m_functions; }
	QMenu* menu() { return m_menu; }
	const QMenu* menu() const { return m_menu; }
	const QString& visibleName() const { return m_visibleName; }
	const QString& name() const { return m_name; }
	QList<InputItem>& items() { return m_items; }
	const QList<InputItem>& items() const { return m_items; }
	InputItem* parent() { return m_parent; }
	const InputItem* parent() const { return m_parent; }
	void addAction(QAction* action, const QString& name);
	void addFunctions(Functions functions, int shortcut, const QString& visibleName,
	                  const QString& name);
	void addSubmenu(QMenu* menu);
	int button() const { return m_button; }
	void setShortcut(int sequence);
	void setButton(int button) { m_button = button; }
	int axis() const { return m_axis; }
	GamepadAxisEvent::Direction direction() const { return m_direction; }
	void setAxis(int axis, GamepadAxisEvent::Direction direction);

	bool operator==(const InputItem& other) const {
		return m_menu == other.m_menu && m_action == other.m_action;
	}

private:
	QAction* m_action;
	int m_shortcut;
	QMenu* m_menu;
	Functions m_functions;
	QString m_name;
	QString m_visibleName;
	int m_button;
	int m_axis;
	GamepadAxisEvent::Direction m_direction;
	QList<InputItem> m_items;
	InputItem* m_parent;
};

}

#endif
