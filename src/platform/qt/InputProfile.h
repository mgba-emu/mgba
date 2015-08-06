/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef QGBA_INPUT_PROFILE
#define QGBA_INPUT_PROFILE

#include "GamepadAxisEvent.h"

extern "C" {
#include "gba/interface.h"
}

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

	struct ShortcutButton {
		const char* shortcut;
		int button;
	};

	struct ShortcutAxis {
		const char* shortcut;
		GamepadAxisEvent::Direction direction;
		int axis;
	};

	constexpr InputProfile(const char* name,
	                       int keys[GBA_KEY_MAX],
	                       const ShortcutButton* shortcutButtons = (ShortcutButton[]) {{}},
	                       const ShortcutAxis* shortcutAxes = (ShortcutAxis[]) {{}},
	                       AxisValue axes[GBA_KEY_MAX] = (AxisValue[GBA_KEY_MAX]) {
	                       	                             {}, {}, {}, {}, 
	                                                     { GamepadAxisEvent::Direction::POSITIVE, 0 },
	                                                     { GamepadAxisEvent::Direction::NEGATIVE, 0 },
	                                                     { GamepadAxisEvent::Direction::NEGATIVE, 1 },
	                                                     { GamepadAxisEvent::Direction::POSITIVE, 1 },
	                                                     {}, {}},
	                       const struct Coord& tiltAxis = { 2, 3 },
	                       const struct Coord& gyroAxis = { 0, 1 },
	                       float gyroSensitivity = 2e+09f);

	static const InputProfile s_defaultMaps[];

	const char* m_profileName;
	const int m_keys[GBA_KEY_MAX];
	const AxisValue m_axes[GBA_KEY_MAX];
	const ShortcutButton* m_shortcutButtons;
	const ShortcutAxis* m_shortcutAxes;
	Coord m_tiltAxis;
	Coord m_gyroAxis;
	float m_gyroSensitivity;
};

}

#endif
