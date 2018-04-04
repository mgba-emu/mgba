/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QEvent>

#include <mgba/internal/gba/input.h>

namespace QGBA {

class InputController;

class GamepadButtonEvent : public QEvent {
public:
	GamepadButtonEvent(Type pressType, int button, int type, InputController* controller = nullptr);

	int value() const { return m_button; }
	GBAKey gbaKey() const { return m_key; }

	static Type Down();
	static Type Up();

private:
	static Type s_downType;
	static Type s_upType;

	int m_button;
	InputController* m_controller;
	GBAKey m_key;
};

}
