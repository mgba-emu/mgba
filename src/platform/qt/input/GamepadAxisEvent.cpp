/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "input/GamepadAxisEvent.h"

#include "InputController.h"

using namespace QGBA;

QEvent::Type GamepadAxisEvent::s_type = QEvent::None;

GamepadAxisEvent::GamepadAxisEvent(int axis, Direction direction, bool isNew, int type, InputController* controller)
	: QEvent(Type())
	, m_axis(axis)
	, m_direction(direction)
	, m_isNew(isNew)
	, m_key(-1)
{
	ignore();
	if (controller) {
		m_key = mInputMapAxis(controller->map(), type, axis, direction * INT_MAX);
	}
}

QEvent::Type GamepadAxisEvent::Type() {
	if (s_type == None) {
		s_type = static_cast<enum Type>(registerEventType());
	}
	return s_type;
}
