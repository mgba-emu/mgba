/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "Action.h"

#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QSet>

#include <functional>
#include <memory>

class QMenu;
class QMenuBar;

namespace QGBA {

class ConfigOption;
class ShortcutController;

class ActionMapper : public QObject {
Q_OBJECT

public:
	void addMenu(const QString& visibleName, const QString& name, const QString& parent = {});
	void addHiddenMenu(const QString& visibleName, const QString& name, const QString& parent = {});
	void clearMenu(const QString& name);
	void rebuildMenu(QMenuBar*, QWidget* context, const ShortcutController&);

	void addSeparator(const QString& menu);

	std::shared_ptr<Action> addAction(const QString& visibleName, const QString& name, Action::Function&& action, const QString& menu = {}, const QKeySequence& = {});
	template<typename T, typename V> std::shared_ptr<Action> addAction(const QString& visibleName, const QString& name, T* obj, V (T::*method)(), const QString& menu = {}, const QKeySequence& = {});
	std::shared_ptr<Action> addAction(const QString& visibleName, ConfigOption* option, const QVariant& variant, const QString& menu = {});

	std::shared_ptr<Action> addBooleanAction(const QString& visibleName, const QString& name, Action::BooleanFunction&& action, const QString& menu = {}, const QKeySequence& = {});
	std::shared_ptr<Action> addBooleanAction(const QString& visibleName, ConfigOption* option, const QString& menu = {});

	std::shared_ptr<Action> addHeldAction(const QString& visibleName, const QString& name, Action::BooleanFunction&& action, const QString& menu = {}, const QKeySequence& = {});

	std::shared_ptr<Action> addHiddenAction(const QString& visibleName, const QString& name, Action::Function&& action, const QString& menu = {}, const QKeySequence& = {});
	template<typename T, typename V> std::shared_ptr<Action> addHiddenAction(const QString& visibleName, const QString& name, T* obj, V (T::*method)(), const QString& menu = {}, const QKeySequence& = {});

	bool isHeld(const QString& name) const { return m_heldActions.contains(name); }

	QStringList menuItems(const QString& menu = QString()) const;
	QString menuFor(const QString& action) const;
	QString menuName(const QString& menu) const;

	std::shared_ptr<Action> getAction(const QString& action);
	QKeySequence defaultShortcut(const QString& action);

signals:
	void actionAdded(const QString& name);
	void menuCleared(const QString& name);

private:
	void rebuildMenu(const QString& menu, QMenu* qmenu, QWidget* context, const ShortcutController&);
	std::shared_ptr<Action> addAction(const Action& act, const QString& name, const QString& menu, const QKeySequence& shortcut);

	QHash<QString, std::shared_ptr<Action>> m_actions;
	QHash<QString, QStringList> m_menus;
	QHash<QString, QString> m_reverseMenus;
	QHash<QString, QString> m_menuNames;
	QHash<QString, QKeySequence> m_defaultShortcuts;
	QSet<QString> m_hiddenActions;
	QSet<QString> m_heldActions;
};

template<typename T, typename V>
std::shared_ptr<Action> ActionMapper::addAction(const QString& visibleName, const QString& name, T* obj, V (T::*method)(), const QString& menu, const QKeySequence& shortcut) {
	return addAction(visibleName, name, [method, obj]() -> void {
		(obj->*method)();
	}, menu, shortcut);
}

template<typename T, typename V>
std::shared_ptr<Action> ActionMapper::addHiddenAction(const QString& visibleName, const QString& name, T* obj, V (T::*method)(), const QString& menu, const QKeySequence& shortcut) {
	m_hiddenActions.insert(name);
	return addAction(visibleName, name, obj, method, menu, shortcut);
}

}
