/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QEvent>

namespace QGBA {

class InputController;

class GamepadHatEvent : public QEvent {
public:
	enum Direction {
		CENTER = 0,
		UP = 1,
		RIGHT = 2,
		DOWN = 4,
		LEFT = 8
	};

	GamepadHatEvent(Type pressType, int hatId, Direction direction, int type, InputController* controller = nullptr);

	int hatId() const { return m_hatId; }
	Direction direction() const { return m_direction; }
	int platformKeys() const { return m_keys; }

	static Type Down();
	static Type Up();

private:
	static Type s_downType;
	static Type s_upType;

	int m_hatId;
	Direction m_direction;
	int m_keys;
};

}
