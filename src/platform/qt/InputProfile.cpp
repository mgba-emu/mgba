/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "InputProfile.h"

#include "input/InputMapper.h"
#include "InputController.h"

#include <QRegExp>

using namespace QGBA;

const InputProfile InputProfile::s_defaultMaps[] = {
	{
		"XInput Controller #\\d+", // XInput (Windows)
		{
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
		{
		/*loadState       */ 12,
		/*saveState       */ 13,
		/*holdFastForward */ -1,
		/*holdRewind      */ -1,
		},
		{
		/*loadState       */ {GamepadAxisEvent::Direction::NEUTRAL, -1},
		/*saveState       */ {GamepadAxisEvent::Direction::NEUTRAL, -1},
		/*holdFastForward */ {GamepadAxisEvent::Direction::POSITIVE, 5},
		/*holdRewind      */ {GamepadAxisEvent::Direction::POSITIVE, 4},
		}
	},
	{
		"(Microsoft X-Box 360 pad|Xbox Gamepad \\(userspace driver\\))", // Linux
		{
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
		{
		/*loadState       */ 2,
		/*saveState       */ 3,
		/*holdFastForward */ -1,
		/*holdRewind      */ -1,
		},
		{
		/*loadState       */ {GamepadAxisEvent::Direction::NEUTRAL, -1},
		/*saveState       */ {GamepadAxisEvent::Direction::NEUTRAL, -1},
		/*holdFastForward */ {GamepadAxisEvent::Direction::POSITIVE, 5},
		/*holdRewind      */ {GamepadAxisEvent::Direction::POSITIVE, 2},
		}
	},
	{
		"Xbox 360 Wired Controller", // OS X
		{
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
		{
		/*loadState       */ 2,
		/*saveState       */ 3,
		/*holdFastForward */ -1,
		/*holdRewind      */ -1,
		},
		{
		/*loadState       */ {GamepadAxisEvent::Direction::NEUTRAL, -1},
		/*saveState       */ {GamepadAxisEvent::Direction::NEUTRAL, -1},
		/*holdFastForward */ {GamepadAxisEvent::Direction::POSITIVE, 5},
		/*holdRewind      */ {GamepadAxisEvent::Direction::POSITIVE, 2},
		}
	},
	{
		"(Sony Computer Entertainment )?Wireless Controller", // The DualShock 4 device ID is cut off on Windows
		{
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
		{
		/*loadState       */ 0,
		/*saveState       */ 3,
		/*holdFastForward */ 7,
		/*holdRewind      */ 6,
		},
	},
	{
		"PLAYSTATION\\(R\\)3 Controller", // DualShock 3 (OS X)
		{
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
		{
		/*loadState       */ 15,
		/*saveState       */ 12,
		/*holdFastForward */ 9,
		/*holdRewind      */ 8,
		},
	},
	{
		"Wiimote \\(..-..-..-..-..-..\\)", // WJoy (OS X)
		{
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
		{
		/*loadState       */ 18,
		/*saveState       */ 17,
		/*holdFastForward */ 22,
		/*holdRewind      */ 21,
		},
	},
};

constexpr InputProfile::InputProfile(const char* name,
                                     const KeyList<int> keys,
                                     const Shortcuts<int> shortcutButtons,
                                     const Shortcuts<Axis> shortcutAxes,
                                     const KeyList<AxisValue> axes,
                                     const struct Coord& tiltAxis,
                                     const struct Coord& gyroAxis,
                                     float gyroSensitivity)
	: m_profileName(name)
	, m_keys {
		keys.keyA,
		keys.keyB,
		keys.keySelect,
		keys.keyStart,
		keys.keyRight,
		keys.keyLeft,
		keys.keyUp,
		keys.keyDown,
		keys.keyR,
		keys.keyL,
	}
	, m_axes {
		axes.keyA,
		axes.keyB,
		axes.keySelect,
		axes.keyStart,
		axes.keyRight,
		axes.keyLeft,
		axes.keyUp,
		axes.keyDown,
		axes.keyR,
		axes.keyL,
	}
	, m_shortcutButtons(shortcutButtons)
	, m_shortcutAxes(shortcutAxes)
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
	auto gamepadDriver = controller->gamepadDriver();
	if (gamepadDriver) {
		InputMapper mapper = controller->mapper(gamepadDriver);
		for (size_t i = 0; i < GBA_KEY_MAX; ++i) {
			mapper.bindKey(m_keys[i], i);
			mapper.bindAxis(m_axes[i].axis, m_axes[i].direction, i);
		}
	}

	InputDriver* sensorDriver = controller->sensorDriver();
	if (sensorDriver) {
		sensorDriver->registerTiltAxisX(m_tiltAxis.x);
		sensorDriver->registerTiltAxisY(m_tiltAxis.y);
		sensorDriver->registerGyroAxisX(m_gyroAxis.x);
		sensorDriver->registerGyroAxisY(m_gyroAxis.y);
		sensorDriver->setGyroSensitivity(m_gyroSensitivity);
	}
}

bool InputProfile::lookupShortcutButton(const QString& shortcutName, int* button) const {
	if (shortcutName == QLatin1String("loadState")) {
		*button = m_shortcutButtons.loadState;
		return true;
	}
	if (shortcutName == QLatin1String("saveState")) {
		*button = m_shortcutButtons.saveState;
		return true;
	}
	if (shortcutName == QLatin1String("holdFastForward")) {
		*button = m_shortcutButtons.holdFastForward;
		return true;
	}
	if (shortcutName == QLatin1String("holdRewind")) {
		*button = m_shortcutButtons.holdRewind;
		return true;
	}
	return false;
}

bool InputProfile::lookupShortcutAxis(const QString& shortcutName, int* axis, GamepadAxisEvent::Direction* direction) const {
	if (shortcutName == QLatin1String("loadState")) {
		*axis = m_shortcutAxes.loadState.axis;
		*direction = m_shortcutAxes.loadState.direction;
		return true;
	}
	if (shortcutName == QLatin1String("saveState")) {
		*axis = m_shortcutAxes.saveState.axis;
		*direction = m_shortcutAxes.saveState.direction;
		return true;
	}
	if (shortcutName == QLatin1String("holdFastForward")) {
		*axis = m_shortcutAxes.holdFastForward.axis;
		*direction = m_shortcutAxes.holdFastForward.direction;
		return true;
	}
	if (shortcutName == QLatin1String("holdRewind")) {
		*axis = m_shortcutAxes.holdRewind.axis;
		*direction = m_shortcutAxes.holdRewind.direction;
		return true;
	}
	return false;
}
