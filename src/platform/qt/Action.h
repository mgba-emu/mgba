/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QObject>

#include <functional>

namespace QGBA {

class Action : public QObject {
Q_OBJECT

public:
	typedef std::function<void ()> Function;
	typedef std::function<void (bool)> BooleanFunction;
	enum class Role {
		NO_ROLE = 0,
		ABOUT,
		SETTINGS,
		QUIT,
	};

	Action(Function, const QString& name, const QString& visibleName, QObject* parent = nullptr);
	Action(BooleanFunction, const QString& name, const QString& visibleName, QObject* parent = nullptr);
	Action(const QString& name, const QString& visibleName, QObject* parent = nullptr);

	Action(QObject* parent = nullptr);
	Action(Action&);
	Action(const Action&);

	Function action() const { return m_function; }
	BooleanFunction booleanAction() const { return m_booleanFunction; }

	const QString& name() const { return m_name; }
	const QString& visibleName() const { return m_visibleName; }

	bool operator==(const Action& other) const {
		if (m_name.isNull()) {
			return this == &other;
		}
		return m_name == other.m_name;
	}

	void connect(Function);

	bool isEnabled() const { return m_enabled; }
	bool isActive() const { return m_active; }
	bool isExclusive() const { return m_exclusive; }
	Role role() const { return m_role; }

	void setExclusive(bool exclusive = true) { m_exclusive = exclusive; }
	void setRole(Role role) { m_role = role; }

	Action& operator=(const Action&);

public slots:
	void trigger(bool = true);
	void setEnabled(bool = true);
	void setActive(bool = true);

signals:
	void enabled(bool);
	void activated(bool);

private:
	bool m_enabled = true;
	bool m_active = false;
	bool m_exclusive = false;
	Role m_role = Role::NO_ROLE;

	Function m_function;
	BooleanFunction m_booleanFunction;

	QString m_name;
	QString m_visibleName;
};

}
