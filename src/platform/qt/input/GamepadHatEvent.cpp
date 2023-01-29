/* Copyright (c) 2013-2023 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "input/GamepadHatEvent.h"

#include "InputController.h"

using namespace QGBA;

QEvent::Type GamepadHatEvent::s_downType = QEvent::None;
QEvent::Type GamepadHatEvent::s_upType = QEvent::None;

GamepadHatEvent::GamepadHatEvent(QEvent::Type pressType, int hatId, Direction direction, int type, InputController* controller)
	: QEvent(pressType)
	, m_hatId(hatId)
	, m_direction(direction)
	, m_keys(0)
{
	ignore();
	if (controller) {
		m_keys = mInputMapHat(controller->map(), type, hatId, direction);
	}
}

QEvent::Type GamepadHatEvent::Down() {
	if (s_downType == None) {
		s_downType = static_cast<Type>(registerEventType());
	}
	return s_downType;
}

QEvent::Type GamepadHatEvent::Up() {
	if (s_upType == None) {
		s_upType = static_cast<Type>(registerEventType());
	}
	return s_upType;
}
