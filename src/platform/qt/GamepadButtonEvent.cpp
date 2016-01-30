/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GamepadButtonEvent.h"

#include "InputController.h"

using namespace QGBA;

QEvent::Type GamepadButtonEvent::s_downType = QEvent::None;
QEvent::Type GamepadButtonEvent::s_upType = QEvent::None;

GamepadButtonEvent::GamepadButtonEvent(QEvent::Type pressType, int button, int type, InputController* controller)
	: QEvent(pressType)
	, m_button(button)
	, m_controller(controller)
	, m_key(GBA_KEY_NONE)
{
	ignore();
	if (controller) {
		m_key = static_cast<GBAKey>(mInputMapKey(controller->map(), type, button));
	}
}

QEvent::Type GamepadButtonEvent::Down() {
	if (s_downType == None) {
		s_downType = static_cast<Type>(registerEventType());
	}
	return s_downType;
}

QEvent::Type GamepadButtonEvent::Up() {
	if (s_upType == None) {
		s_upType = static_cast<Type>(registerEventType());
	}
	return s_upType;
}
