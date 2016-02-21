/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputProfile.h"

#include "InputController.h"

#include <QRegExp>

using namespace QGBA;

const InputProfile InputProfile::s_defaultMaps[] = {
	{
		"XInput Controller #\\d+", // XInput (Windows)
		(int[GBA_KEY_MAX]) {
		/*keyA      */ 11,
		/*keyB      */ 10,
		/*keySelect */  5,
		/*keyStart  */  4,
		/*keyRight  */  3,
		/*keyLeft   */  2,
		/*keyUp     */  0,
		/*keyDown   */  1,
		/*keyR      */  9,
		/*keyL      */  8
		},
		(ShortcutButton[]) {
			{"loadState", 12},
			{"saveState", 13},
			{}
		},
		(ShortcutAxis[]) {
			{"holdFastForward", GamepadAxisEvent::Direction::POSITIVE, 5},
			{"holdRewind", GamepadAxisEvent::Direction::POSITIVE, 4},
			{}
		}
	},
	{
		"(Microsoft X-Box 360 pad|Xbox Gamepad \\(userspace driver\\))", // Linux
		(int[GBA_KEY_MAX]) {
		/*keyA      */  1,
		/*keyB      */  0,
		/*keySelect */  6,
		/*keyStart  */  7,
		/*keyRight  */ -1,
		/*keyLeft   */ -1,
		/*keyUp     */ -1,
		/*keyDown   */ -1,
		/*keyR      */  5,
		/*keyL      */  4
		},
		(ShortcutButton[]) {
			{"loadState", 2},
			{"saveState", 3},
			{}
		},
		(ShortcutAxis[]) {
			{"holdFastForward", GamepadAxisEvent::Direction::POSITIVE, 5},
			{"holdRewind", GamepadAxisEvent::Direction::POSITIVE, 2},
			{}
		}
	},
	{
		"Xbox 360 Wired Controller", // OS X
		(int[GBA_KEY_MAX]) {
		/*keyA      */  1,
		/*keyB      */  0,
		/*keySelect */  9,
		/*keyStart  */  8,
		/*keyRight  */ 14,
		/*keyLeft   */ 13,
		/*keyUp     */ 11,
		/*keyDown   */ 12,
		/*keyR      */  5,
		/*keyL      */  4
		},
		(ShortcutButton[]) {
			{"loadState", 2},
			{"saveState", 3},
			{}
		},
		(ShortcutAxis[]) {
			{"holdFastForward", GamepadAxisEvent::Direction::POSITIVE, 5},
			{"holdRewind", GamepadAxisEvent::Direction::POSITIVE, 2},
			{}
		}
	},
	{
		"(Sony Computer Entertainment )?Wireless Controller", // The DualShock 4 device ID is cut off on Windows
		(int[GBA_KEY_MAX]) {
		/*keyA      */  1,
		/*keyB      */  2,
		/*keySelect */  8,
		/*keyStart  */  9,
		/*keyRight  */ -1,
		/*keyLeft   */ -1,
		/*keyUp     */ -1,
		/*keyDown   */ -1,
		/*keyR      */  5,
		/*keyL      */  4
		},
		(ShortcutButton[]) {
			{"loadState", 0},
			{"saveState", 3},
			{"holdFastForward", 7},
			{"holdRewind", 6},
			{}
		},
	},
	{
		"PLAYSTATION\\(R\\)3 Controller", // DualShock 3 (OS X)
		(int[GBA_KEY_MAX]) {
		/*keyA      */ 13,
		/*keyB      */ 14,
		/*keySelect */  0,
		/*keyStart  */  3,
		/*keyRight  */  5,
		/*keyLeft   */  7,
		/*keyUp     */  4,
		/*keyDown   */  6,
		/*keyR      */ 11,
		/*keyL      */ 10
		},
		(ShortcutButton[]) {
			{"loadState", 15},
			{"saveState", 12},
			{"holdFastForward", 9},
			{"holdRewind", 8},
			{}
		}
	},
	{
		"Wiimote \\(..-..-..-..-..-..\\)", // WJoy (OS X)
		(int[GBA_KEY_MAX]) {
		/*keyA      */ 15,
		/*keyB      */ 16,
		/*keySelect */  7,
		/*keyStart  */  6,
		/*keyRight  */ 14,
		/*keyLeft   */ 13,
		/*keyUp     */ 11,
		/*keyDown   */ 12,
		/*keyR      */ 20,
		/*keyL      */ 19
		},
		(ShortcutButton[]) {
			{"loadState", 18},
			{"saveState", 17},
			{"holdFastForward", 22},
			{"holdRewind", 21},
			{}
		}
	},
};

constexpr InputProfile::InputProfile(const char* name,
                                     int keys[GBA_KEY_MAX],
                                     const ShortcutButton* shortcutButtons,
                                     const ShortcutAxis* shortcutAxes,
                                     AxisValue axes[GBA_KEY_MAX],
                                     const struct Coord& tiltAxis,
                                     const struct Coord& gyroAxis,
                                     float gyroSensitivity)
	: m_profileName(name)
	, m_keys {
		keys[GBA_KEY_A],
		keys[GBA_KEY_B],
		keys[GBA_KEY_SELECT],
		keys[GBA_KEY_START],
		keys[GBA_KEY_RIGHT],
		keys[GBA_KEY_LEFT],
		keys[GBA_KEY_UP],
		keys[GBA_KEY_DOWN],
		keys[GBA_KEY_R],
		keys[GBA_KEY_L]
	}
	, m_shortcutButtons(shortcutButtons)
	, m_shortcutAxes(shortcutAxes)
	, m_axes {
		axes[GBA_KEY_A],
		axes[GBA_KEY_B],
		axes[GBA_KEY_SELECT],
		axes[GBA_KEY_START],
		axes[GBA_KEY_RIGHT],
		axes[GBA_KEY_LEFT],
		axes[GBA_KEY_UP],
		axes[GBA_KEY_DOWN],
		axes[GBA_KEY_R],
		axes[GBA_KEY_L]
	}
	, m_tiltAxis(tiltAxis)
	, m_gyroAxis(gyroAxis)
	, m_gyroSensitivity(gyroSensitivity)
{
}

const InputProfile* InputProfile::findProfile(const QString& name) {
	for (size_t i = 0; i < sizeof(s_defaultMaps) / sizeof(*s_defaultMaps); ++i) {
		QRegExp re(s_defaultMaps[i].m_profileName);
		if (re.exactMatch(name)) {
			return &s_defaultMaps[i];
		}
	}
	return nullptr;
}

void InputProfile::apply(InputController* controller) const {
	for (size_t i = 0; i < GBA_KEY_MAX; ++i) {
#ifdef BUILD_SDL
		controller->bindKey(SDL_BINDING_BUTTON, m_keys[i], static_cast<GBAKey>(i));
		controller->bindAxis(SDL_BINDING_BUTTON, m_axes[i].axis, m_axes[i].direction, static_cast<GBAKey>(i));
#endif
	}
	controller->registerTiltAxisX(m_tiltAxis.x);
	controller->registerTiltAxisY(m_tiltAxis.y);
	controller->registerGyroAxisX(m_gyroAxis.x);
	controller->registerGyroAxisY(m_gyroAxis.y);
	controller->setGyroSensitivity(m_gyroSensitivity);
}

bool InputProfile::lookupShortcutButton(const QString& shortcutName, int* button) const {
	for (size_t i = 0; m_shortcutButtons[i].shortcut; ++i) {
		const ShortcutButton& shortcut = m_shortcutButtons[i];
		if (QLatin1String(shortcut.shortcut) == shortcutName) {
			*button = shortcut.button;
			return true;
		}
	}
	return false;
}

bool InputProfile::lookupShortcutAxis(const QString& shortcutName, int* axis, GamepadAxisEvent::Direction* direction) const {
	for (size_t i = 0; m_shortcutAxes[i].shortcut; ++i) {
		const ShortcutAxis& shortcut = m_shortcutAxes[i];
		if (QLatin1String(shortcut.shortcut) == shortcutName) {
			*axis = shortcut.axis;
			*direction = shortcut.direction;
			return true;
		}
	}
	return false;
}
