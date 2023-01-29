/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include "input/GamepadAxisEvent.h"

#include <mgba/gba/interface.h>
#include <mgba/internal/gba/input.h>

namespace QGBA {

class InputController;

class InputProfile {
public:
	static const InputProfile* findProfile(const QString& name);

	void apply(InputController*) const;
	bool lookupShortcutButton(const QString& shortcut, int* button) const;
	bool lookupShortcutAxis(const QString& shortcut, int* axis, GamepadAxisEvent::Direction* direction) const;

private:
	struct Coord {
		int x;
		int y;
	};

	struct AxisValue {
		GamepadAxisEvent::Direction direction;
		int axis;
	};

	template <typename T> struct Shortcuts {
		T loadState;
		T saveState;
		T holdFastForward;
		T holdRewind;
	};

	struct Axis {
		GamepadAxisEvent::Direction direction;
		int axis;
	};

	template <typename T> struct KeyList {
		T keyA;
		T keyB;
		T keySelect;
		T keyStart;
		T keyRight;
		T keyLeft;
		T keyUp;
		T keyDown;
		T keyR;
		T keyL;
	};

	constexpr InputProfile(const char* name,
	                       const KeyList<int> keys,
	                       const Shortcuts<int> shortcutButtons = { -1, -1, -1, -1},
	                       const Shortcuts<Axis> shortcutAxes = {
	                                                            {GamepadAxisEvent::Direction::NEUTRAL, -1},
	                                                            {GamepadAxisEvent::Direction::NEUTRAL, -1},
	                                                            {GamepadAxisEvent::Direction::NEUTRAL, -1},
	                                                            {GamepadAxisEvent::Direction::NEUTRAL, -1}},
	                       const KeyList<AxisValue> axes = {
	                                                       { GamepadAxisEvent::Direction::NEUTRAL, -1 },
	                                                       { GamepadAxisEvent::Direction::NEUTRAL, -1 },
	                                                       { GamepadAxisEvent::Direction::NEUTRAL, -1 },
	                                                       { GamepadAxisEvent::Direction::NEUTRAL, -1 },
	                                                       { GamepadAxisEvent::Direction::POSITIVE, 0 },
	                                                       { GamepadAxisEvent::Direction::NEGATIVE, 0 },
	                                                       { GamepadAxisEvent::Direction::NEGATIVE, 1 },
	                                                       { GamepadAxisEvent::Direction::POSITIVE, 1 },
	                                                       { GamepadAxisEvent::Direction::NEUTRAL, -1 },
	                                                       { GamepadAxisEvent::Direction::NEUTRAL, -1 }},
	                       const struct Coord& tiltAxis = { 2, 3 },
	                       const struct Coord& gyroAxis = { 0, 1 },
	                       float gyroSensitivity = 2e+09f);

	static const InputProfile s_defaultMaps[];

	const char* m_profileName;
	const int m_keys[GBA_KEY_MAX];
	const AxisValue m_axes[GBA_KEY_MAX];
	const Shortcuts<int> m_shortcutButtons;
	const Shortcuts<Axis> m_shortcutAxes;
	Coord m_tiltAxis;
	Coord m_gyroAxis;
	float m_gyroSensitivity;
};

}
