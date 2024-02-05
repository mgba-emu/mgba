/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QEvent>

namespace QGBA {

class InputController;

class GamepadAxisEvent : public QEvent {
public:
	enum Direction {
		NEUTRAL = 0,
		POSITIVE = 1,
		NEGATIVE = -1
	};

	GamepadAxisEvent(int axis, Direction direction, bool isNew, int type, InputController* controller = nullptr);

	int axis() const { return m_axis; }
	Direction direction() const { return m_direction; }
	bool isNew() const { return m_isNew; }
	int platformKey() const { return m_key; }

	static enum Type Type();

private:
	static enum Type s_type;

	int m_axis;
	Direction m_direction;
	bool m_isNew;
	int m_key;
};

}
