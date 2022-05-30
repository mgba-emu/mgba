/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ActionMapper.h"

#include "ConfigController.h"
#include "ShortcutController.h"

#include <QMenu>
#include <QMenuBar>

using namespace QGBA;

void ActionMapper::addMenu(const QString& visibleName, const QString& name, const QString& parent) {
	QString mname(QString(".%1").arg(name));
	m_menus[parent].append(mname);
	m_reverseMenus[mname] = parent;
	m_menuNames[name] = visibleName;
}

void ActionMapper::addHiddenMenu(const QString& visibleName, const QString& name, const QString& parent) {
	m_hiddenActions.insert(QString(".%1").arg(name));
	addMenu(visibleName, name, parent);
}

void ActionMapper::clearMenu(const QString& name) {
	m_menus[name].clear();
	emit menuCleared(name);
}

void ActionMapper::rebuildMenu(QMenuBar* menubar, QWidget* context, const ShortcutController& shortcuts) {
	menubar->clear();
	for (QAction* action : context->actions()) {
		context->removeAction(action);
	}
	for (const QString& m : m_menus[{}]) {
		if (m_hiddenActions.contains(m)) {
			continue;
		}
		QString menu = m.mid(1);
		QMenu* qmenu = menubar->addMenu(m_menuNames[menu]);

		rebuildMenu(menu, qmenu, context, shortcuts);
	}
}

void ActionMapper::rebuildMenu(const QString& menu, QMenu* qmenu, QWidget* context, const ShortcutController& shortcuts) {
	for (const QString& actionName : m_menus[menu]) {
		if (actionName.isNull()) {
			qmenu->addSeparator();
			continue;
		}
		if (m_hiddenActions.contains(actionName)) {
			continue;
		}
		if (actionName[0] == '.') {
			QString name = actionName.mid(1);
			QMenu* newMenu = qmenu->addMenu(m_menuNames[name]);
			rebuildMenu(name, newMenu, context, shortcuts);
			continue;
		}
		Action* action = &m_actions[actionName];
		QAction* qaction = qmenu->addAction(action->visibleName());
		qaction->setEnabled(action->isEnabled());
		qaction->setShortcutContext(Qt::WidgetShortcut);
		if (action->isExclusive() || action->booleanAction()) {
			qaction->setCheckable(true);
		}
		if (action->isActive()) {
			qaction->setChecked(true);
		}
		const Shortcut* shortcut = shortcuts.shortcut(actionName);
		if (shortcut) {
			if (shortcut->shortcut() > 0) {
				qaction->setShortcut(QKeySequence(shortcut->shortcut()));
			}
		} else if (!m_defaultShortcuts[actionName].isEmpty()) {
			qaction->setShortcut(m_defaultShortcuts[actionName][0]);
		}
		switch (action->role()) {
		case Action::Role::NO_ROLE:
			qaction->setMenuRole(QAction::NoRole);
			break;
		case Action::Role::SETTINGS:
			qaction->setMenuRole(QAction::PreferencesRole);
			break;
		case Action::Role::ABOUT:
			qaction->setMenuRole(QAction::AboutRole);
			break;
		case Action::Role::QUIT:
			qaction->setMenuRole(QAction::QuitRole);
			break;
		}
		QObject::connect(qaction, &QAction::triggered, [qaction, action](bool enabled) {
			if (qaction->isCheckable()) {
				action->trigger(enabled);
			} else {
				action->trigger();
			}
		});
		QObject::connect(action, &Action::enabled, qaction, &QAction::setEnabled);
		QObject::connect(action, &Action::activated, [qaction, action](bool active) {
			if (qaction->isCheckable()) {
				qaction->setChecked(active);
			} else if (active) {
				action->setActive(false);
			}
		});
		QObject::connect(action, &Action::destroyed, qaction, &QAction::deleteLater);
		if (shortcut) {
			QObject::connect(shortcut, &Shortcut::shortcutChanged, qaction, [qaction](int shortcut) {
				qaction->setShortcut(QKeySequence(shortcut));
			});
		}
		context->addAction(qaction);
	}
}

void ActionMapper::addSeparator(const QString& menu) {
	m_menus[menu].append(QString{});
}

Action* ActionMapper::addAction(const Action& act, const QString& name, const QString& menu, const QKeySequence& shortcut) {
	m_actions.insert(name, act);
	m_reverseMenus[name] = menu;
	m_menus[menu].append(name);
	if (!shortcut.isEmpty()) {
		m_defaultShortcuts[name] = shortcut;
	}
	emit actionAdded(name);

	return &m_actions[name];
}

Action* ActionMapper::addAction(const QString& visibleName, const QString& name, Action::Function action, const QString& menu, const QKeySequence& shortcut) {
	return addAction(Action(action, name, visibleName), name, menu, shortcut);
}

Action* ActionMapper::addAction(const QString& visibleName, ConfigOption* option, const QVariant& variant, const QString& menu) {
	return addAction(Action([option, variant]() {
		option->setValue(variant);
	}, option->name(), visibleName), QString("%1.%2").arg(option->name()).arg(variant.toString()), menu, {});
}

Action* ActionMapper::addBooleanAction(const QString& visibleName, const QString& name, Action::BooleanFunction action, const QString& menu, const QKeySequence& shortcut) {
	return addAction(Action(action, name, visibleName), name, menu, shortcut);
}

Action* ActionMapper::addBooleanAction(const QString& visibleName, ConfigOption* option, const QString& menu) {
	return addAction(Action([option](bool value) {
		option->setValue(value);
	}, option->name(), visibleName), option->name(), menu, {});
}

Action* ActionMapper::addHeldAction(const QString& visibleName, const QString& name, Action::BooleanFunction action, const QString& menu, const QKeySequence& shortcut) {
	m_hiddenActions.insert(name);
	m_heldActions.insert(name);
	return addBooleanAction(visibleName, name, action, menu, shortcut);
}

Action* ActionMapper::addHiddenAction(const QString& visibleName, const QString& name, Action::Function action, const QString& menu, const QKeySequence& shortcut) {
	m_hiddenActions.insert(name);
	return addAction(visibleName, name, action, menu, shortcut);
}

QStringList ActionMapper::menuItems(const QString& menu) const {
	return m_menus[menu];
}

QString ActionMapper::menuFor(const QString& menu) const {
	return m_reverseMenus[menu];
}

QString ActionMapper::menuName(const QString& menu) const {
	if (!menu.isNull() && menu[0] == '.') {
		return m_menuNames[menu.mid(1)];
	}
	return m_menuNames[menu];
}

Action* ActionMapper::getAction(const QString& itemName) {
	return &m_actions[itemName];
}

QKeySequence ActionMapper::defaultShortcut(const QString& itemName) {
	return m_defaultShortcuts[itemName];
}
