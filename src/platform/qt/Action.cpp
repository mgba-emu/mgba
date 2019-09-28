/* Copyright (c) 2013-2018 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Action.h"

using namespace QGBA;

Action::Action(QObject* parent)
	: QObject(parent)
{
}

Action::Action(Function function, const QString& name, const QString& visibleName, QObject* parent)
	: QObject(parent)
	, m_function(function)
	, m_name(name)
	, m_visibleName(visibleName)
{
}

Action::Action(Action::BooleanFunction function, const QString& name, const QString& visibleName, QObject* parent)
	: QObject(parent)
	, m_booleanFunction(function)
	, m_name(name)
	, m_visibleName(visibleName)
{
}

Action::Action(const QString& name, const QString& visibleName, QObject* parent)
	: QObject(parent)
	, m_name(name)
	, m_visibleName(visibleName)
{
}

Action::Action(const Action& other)
	: QObject(other.parent())
	, m_enabled(other.m_enabled)
	, m_active(other.m_active)
	, m_function(other.m_function)
	, m_booleanFunction(other.m_booleanFunction)
	, m_name(other.m_name)
	, m_visibleName(other.m_visibleName)
{
}

Action::Action(Action& other)
	: QObject(other.parent())
	, m_enabled(other.m_enabled)
	, m_active(other.m_active)
	, m_function(other.m_function)
	, m_booleanFunction(other.m_booleanFunction)
	, m_name(other.m_name)
	, m_visibleName(other.m_visibleName)
{
}

void Action::connect(Function func) {
	m_booleanFunction = {};
	m_function = func;
}

void Action::trigger(bool active) {
	if (!m_enabled) {
		return;
	}

	if (m_exclusive && !m_booleanFunction) {
		active = true;
	}

	if (m_function && active) {
		m_function();
	}
	if (m_booleanFunction) {
		m_booleanFunction(active);
	}

	m_active = active;
	emit activated(active);
}

void Action::setEnabled(bool e) {
	if (m_enabled == e) {
		return;
	}
	m_enabled = e;
	emit enabled(e);
}

void Action::setActive(bool a) {
	if (m_active == a) {
		return;
	}
	m_active = a;
	emit activated(a);
}

Action& Action::operator=(const Action& other) {
	setParent(other.parent());
	m_enabled = other.m_enabled;
	m_active = other.m_active;
	m_function = other.m_function;
	m_booleanFunction = other.m_booleanFunction;
	m_name = other.m_name;
	m_visibleName = other.m_visibleName;
	return *this;
}
